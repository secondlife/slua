// ServerLua: per-script execution engine shared with the script host.
#pragma once

#include <memory>
#include <string>

#include "lua.h"

namespace Luau
{
namespace Executor
{

// Memory category for allocations that should not be billed to the script
constexpr int kSystemMemcat = 0;
// Memory category for allocations attributable to the script instance
constexpr int kUserMemcat = LUA_FIRST_USER_MEMCAT;
// Default (and normal maximum) per-script memory limit
constexpr int kDefaultMemoryLimit = 1024 * 128;

// Identifies which class a persisted payload belongs to, and which layout it
// used. Bump `version` whenever that class's fields change.
struct StateFingerprint
{
    char tag[4];
    uint32_t version;
};

// Log levels for LogCallback
enum class LogLevel : uint8_t
{
    Debug = 0,
    Info,
    Warn,
};

// Parameters that define the sealed image consumed by buildImage()
struct ImageConfig
{
    // Luau bytecode with any host asset header already stripped off
    const char* bytecode = nullptr;
    size_t bytecodeSize = 0;
    bool isLSL = false;
    uint32_t apiVersion = 0;
    // We want to leave open the possibility that we can change bytecode behind
    // people's backs for upgrading reasons. Keep around the amount we want to
    // actually "charge" them for the bytecode size for memory accounting purposes
    // so this doesn't break scripts.
    size_t chargedBytecodeSize = 0;
    const char* chunkname = "=lua_script";
    // Identifier used in build log messages. Probably an asset UUID.
    const char* name = "";
};

// Per-placement parameters that can differ between two scripts running the
// same asset, consumed by instantiateScript().
struct ScriptConfig
{
    // Opaque identifier used as the log source for the script's messages
    const char* scriptId = "";
    int memoryLimit = kDefaultMemoryLimit;
    // Opaque per-script host context copied onto the Script. In lscript terms,
    // this is the `LLScriptExecuteLuau` instance.
    void* hostContext = nullptr;
};

class IEnvironment;
class IImage;
class IProvisioner;
class Script;

// Receives an engine log message, already formatted. `source` identifies the
// origin for attribution and throttling.
using LogCallback = void (*)(LogLevel level, const char* source, const char* message);

// Process-wide log sink, same shape as `Luau::assertHandler()`. Messages are
// dropped while it's null.
inline LogCallback& logCallback()
{
    static LogCallback callback = nullptr;
    return callback;
}

// Format a message and hand it to the installed LogCallback. `source`
// identifies the origin for attribution and throttling.
void logDebug(const char* source, const char* fmt, ...) LUA_PRINTF_ATTR(2, 3);
void logInfo(const char* source, const char* fmt, ...) LUA_PRINTF_ATTR(2, 3);
void logWarn(const char* source, const char* fmt, ...) LUA_PRINTF_ATTR(2, 3);

// Figure out which clock source to use
lua_clockProvider resolveDefaultQuantaClock();

// Give the embedder a chance to plop their own things into the environment before it's
// fully set up. This is called before GC fixing / ares perms registration.
using PopulateEnvironmentCallback = void (*)(IEnvironment& environment, lua_State* L);

// Host callback wiring, identical for every script of a provisioner.
struct HostCallbacks
{
    lua_clockProvider clockProvider = nullptr;
    lua_clockProvider performanceClockProvider = nullptr;
    lua_randomProvider randomProvider = nullptr;
    lua_setTimerEventCallback setTimerEventCb = nullptr;
    lua_eventHandlerRegistrationCallback eventHandlerRegistrationCb = nullptr;
    lua_clockProvider quantaClockProvider = nullptr;
    PopulateEnvironmentCallback populateEnvironment = nullptr;
};

// An environment is... basically just a Lua VM with some particular settings. It's
// intended for multiple of these to be able to be living at any given moment, one
// for LSL, one for a particular version of the Lua API, etc.
class IEnvironment
{
public:
    virtual ~IEnvironment() = default;

    virtual IProvisioner& getProvisioner() const = 0;
    virtual bool isLSL() const = 0;
    virtual uint32_t getAPIVersion() const = 0;

    // The root lua_State this environment owns. Images' clone threads and
    // forkservers all hang off it
    virtual lua_State* getBaseState() const = 0;
    // Threaddata seed for every non-script thread in the VM (base state,
    // forkservers, build-time clones)
    virtual lua_SLRuntimeState& getRuntimeState() = 0;
};

// The environment a `Provisioner` mints by default. Public so a host can
// subclass it and hand its own back from `Provisioner::makeEnvironment()`.
class Environment : public IEnvironment
{
public:
    Environment(IProvisioner& provisioner, bool is_lsl, uint32_t api_version);
    ~Environment() override;

    Environment(const Environment&) = delete;
    Environment& operator=(const Environment&) = delete;

    IProvisioner& getProvisioner() const override { return mProvisioner; }
    bool isLSL() const override { return mIsLSL; }
    uint32_t getAPIVersion() const override { return mAPIVersion; }

    lua_State* getBaseState() const override { return mBaseState; }

    // `kind` stays `LUA_SLSTATE_BARE` so the engine callbacks know there is
    // no `Script` behind our threads.
    lua_SLRuntimeState& getRuntimeState() override { return mRuntimeState; }

    // Stands the VM up. Run by the provisioner immediately after construction,
    // so a subclass' constructor goes first and has nothing built to trip over.
    // Templated on the script type because `lua_Callbacks` is per-VM, so the
    // handlers have to be chosen here rather than per script.
    template<typename S>
    void build()
    {
        lua_State* L = openVM();
        S::installVMCallbacks(L);

        // Assigned last so nothing can reach a VM that isn't wired up yet
        mBaseState = L;
    }

private:
    // Everything up to the callbacks, kept out of the template so the library
    // setup stays in the .cpp
    lua_State* openVM();

    IProvisioner& mProvisioner;
    bool mIsLSL = false;
    uint32_t mAPIVersion = 0;

    lua_SLRuntimeState mRuntimeState;
    lua_State* mBaseState = nullptr;
};

// A particular Instance of a given script's run state. Effectively, this is just
// an RAII helper that holds onto a `lua_State*` in the registry and unrefs it
// when it dies. Necessarily separate from `Script` because `Script`s need to keep
// a stable object identity across reset() / deserialize(). Instance is probably
// not a very descriptive name...
class Instance
{
public:
    Instance() = default;

    // Adopts `thread`, releasing registry ref `ref` against `anchor_state` on
    // destruction.
    Instance(lua_State* thread, lua_State* anchor_state, int ref)
        : mThread(thread)
        , mAnchorState(anchor_state)
        , mRef(ref)
    {
    }

    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;

    Instance(Instance&& other) noexcept
        : mThread(other.mThread)
        , mAnchorState(other.mAnchorState)
        , mRef(other.mRef)
    {
        other.mThread = nullptr;
        other.mAnchorState = nullptr;
        other.mRef = LUA_NOREF;
    }

    Instance& operator=(Instance&& other) noexcept
    {
        if (this != &other)
        {
            // Clear out our thread, we're taking another.
            releaseThread();

            mThread = other.mThread;
            mAnchorState = other.mAnchorState;
            mRef = other.mRef;
            other.mThread = nullptr;
            other.mAnchorState = nullptr;
            other.mRef = LUA_NOREF;
        }
        return *this;
    }

    ~Instance() { releaseThread(); }

    explicit operator bool() const { return mThread != nullptr; }
    lua_State* thread() const { return mThread; }

private:
    void releaseThread()
    {
        if (mThread != nullptr)
        {
            // Unrefing it should cause it to be swept up by GC.
            lua_unref(mAnchorState, mRef);
        }
        mThread = nullptr;
        mAnchorState = nullptr;
        mRef = LUA_NOREF;
    }

    lua_State* mThread = nullptr;
    lua_State* mAnchorState = nullptr;
    int mRef = LUA_NOREF;
};


class IImage
{
public:
    virtual ~IImage() = default;

    virtual bool isValid() const = 0;
    // Load-failure message when isValid() is false
    virtual const std::string& getError() const = 0;
    virtual const std::string& getName() const = 0;
    virtual IEnvironment& getEnvironment() const = 0;
    virtual IProvisioner& getProvisioner() const = 0;
    virtual bool isLSL() const = 0;
    virtual uint32_t getAPIVersion() const = 0;

    // Forks off an instance using the forkserver, either with the default
    // state blob, or a provided one if we're resuming.
    virtual Instance forkInstance(lua_SLRuntimeState* owner, const std::string* blob = nullptr) = 0;

    // Serializes `instance` against this image's forkserver into `out`
    virtual bool serializeInstance(const Instance& instance, std::string& out) = 0;

    // The environment's bare runtime state, seed for each Script's live copy
    virtual const lua_SLRuntimeState& getRuntimeState() const = 0;

    // Full asset size charged against every script of this image
    virtual size_t getChargedBytecodeSize() const = 0;

    // Pristine pre-fork objects excluded from per-script memory accounting
    virtual const lua_OpaqueGCObjectSet& getFreeObjects() const = 0;
};

// The image a `Provisioner` mints by default. Public so a host can subclass it
// and hand its own back from `Provisioner::makeImage()`.
class Image : public IImage
{
public:
    Image(std::shared_ptr<IEnvironment> environment, const ImageConfig& config);
    ~Image() override;

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    bool isValid() const override { return mForkerState != nullptr; }
    const std::string& getError() const override { return mError; }
    const std::string& getName() const override { return mName; }
    IEnvironment& getEnvironment() const override { return *mEnvironment; }
    IProvisioner& getProvisioner() const override { return mEnvironment->getProvisioner(); }
    bool isLSL() const override { return mIsLSL; }
    uint32_t getAPIVersion() const override { return mAPIVersion; }

    Instance forkInstance(lua_SLRuntimeState* owner, const std::string* blob) override;
    bool serializeInstance(const Instance& instance, std::string& out) override;

    const lua_SLRuntimeState& getRuntimeState() const override { return mEnvironment->getRuntimeState(); }
    size_t getChargedBytecodeSize() const override { return mChargedBytecodeSize; }
    const lua_OpaqueGCObjectSet& getFreeObjects() const override { return mFreeObjects; }

    // Loads the bytecode and stands up the forkserver. Run by the provisioner
    // immediately after construction, same as `Environment::build()`. A failure
    // leaves `isValid()` false with `getError()` explaining, and the
    // environment still usable.
    void build(const ImageConfig& config);

private:
    // Source string for log messages
    const char* logSource() const { return mName.c_str(); }

    // Keeps the environment alive for as long as the image exists
    std::shared_ptr<IEnvironment> mEnvironment;

    bool mIsLSL = false;
    uint32_t mAPIVersion = 0;

    // Ares forkserver thread holding the pristine post-load snapshot,
    // anchored in the environment's registry
    lua_State* mForkerState = nullptr;
    int mForkerRef = LUA_NOREF;
    // Pristine pre-fork objects excluded from per-script memory accounting
    lua_OpaqueGCObjectSet mFreeObjects;

    size_t mChargedBytecodeSize = 0;
    std::string mName;
    std::string mError;
};


class IProvisioner
{
public:
    virtual ~IProvisioner() = default;

    virtual const HostCallbacks& getCallbacks() const = 0;

    virtual std::shared_ptr<IEnvironment> createEnvironment(bool is_lsl, uint32_t api_version) = 0;
    virtual std::shared_ptr<IImage> buildImage(std::shared_ptr<IEnvironment> environment, const ImageConfig& config) = 0;
    virtual std::shared_ptr<Script> instantiateScript(const std::shared_ptr<IImage>& image, const ScriptConfig& config) = 0;
};

// `S` is the script type this provisioner mints. Both the factory and the VM
// callbacks come from it, so they cannot disagree - the handlers
// `S::installVMCallbacks` installs downcast what `Script::fromLuaState()`
// returns, and that only holds while every script here really is an `S`.
template<typename S = Script>
class Provisioner : public IProvisioner
{
public:
    explicit Provisioner(const HostCallbacks& callbacks)
        : mCallbacks(callbacks)
    {
        if (mCallbacks.quantaClockProvider == nullptr)
            mCallbacks.quantaClockProvider = resolveDefaultQuantaClock();
    }

    Provisioner(const Provisioner&) = delete;
    Provisioner& operator=(const Provisioner&) = delete;

    const HostCallbacks& getCallbacks() const override { return mCallbacks; }

    std::shared_ptr<IEnvironment> createEnvironment(bool is_lsl, uint32_t api_version) override
    {
        std::shared_ptr<Environment> environment = makeEnvironment(is_lsl, api_version);
        environment->build<S>();
        return environment;
    }

    std::shared_ptr<IImage> buildImage(std::shared_ptr<IEnvironment> environment, const ImageConfig& config) override
    {
        if (environment == nullptr || &environment->getProvisioner() != this)
        {
            logWarn(config.name ? config.name : "", "Refusing to build image in an environment provisioned elsewhere");
            return nullptr;
        }
        if (environment->isLSL() != config.isLSL || environment->getAPIVersion() != config.apiVersion)
        {
            logWarn(config.name ? config.name : "", "Refusing to build image in an environment of a different flavor");
            return nullptr;
        }

        std::shared_ptr<Image> image = makeImage(std::move(environment), config);
        image->build(config);
        return image;
    }

    std::shared_ptr<Script> instantiateScript(const std::shared_ptr<IImage>& image, const ScriptConfig& config) override
    {
        if (image == nullptr || &image->getProvisioner() != this)
        {
            logWarn("", "Refusing to instantiate image provisioned elsewhere");
            return nullptr;
        }
        if (!image->isValid())
        {
            logWarn(image->getName().c_str(), "Refusing to instantiate invalid image");
            return nullptr;
        }

        return std::make_shared<S>(image, config);
    }

    // Convenience method so you don't have to do the above manually.
    std::shared_ptr<Script> provisionScript(const ImageConfig& image_config, const ScriptConfig& script_config)
    {
        std::shared_ptr<IImage> image = buildImage(createEnvironment(image_config.isLSL, image_config.apiVersion), image_config);
        if (image == nullptr)
            return nullptr;

        // instantiateScript() logs the refusal for an invalid image
        return instantiateScript(image, script_config);
    }

protected:
    // Override to mint your own types. The tiers hold each other by interface,
    // so a subclass is free to return anything deriving from these, and to
    // downcast what it gets back from `Script::fromLuaState()` and friends.
    virtual std::shared_ptr<Environment> makeEnvironment(bool is_lsl, uint32_t api_version)
    {
        return std::make_shared<Environment>(*this, is_lsl, api_version);
    }

    virtual std::shared_ptr<Image> makeImage(std::shared_ptr<IEnvironment> environment, const ImageConfig& config)
    {
        return std::make_shared<Image>(std::move(environment), config);
    }

private:
    HostCallbacks mCallbacks;
};

} // namespace Executor
} // namespace Luau
