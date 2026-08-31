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

// Signature of the VM interrupt callback, as installed into lua_Callbacks
using InterruptCallback = void (*)(lua_State* L, int gc);

// Aggregate timing of a watchdog's deadline fires, for tuning its fire lead.
// Lateness (seconds) is how far past the intended fire instant the install
// actually happened: OS wakeup jitter in the common case, but it also
// absorbs the whole overshoot when a window is armed with less than a
// lead's worth of quanta, so a bimodal max isn't necessarily jitter.
struct WatchdogStats
{
    uint64_t fires = 0;
    double latenessSum = 0.0;
    double latenessMin = 0.0;
    double latenessMax = 0.0;
};

// Arming policy for a Script's run windows: owns the VM's interrupt handler
// and how it gets installed so that quanta expiry can be delivered. The
// installed pointer is only a request; the handler re-reads the real clock at
// the safepoint, so the clock stays the source of truth. One watchdog serves
// every VM of its provisioner (one per environment); arm() binds the open
// window's VM as the one slot everything else operates on.
class QuantaWatchdog
{
public:
    // `handler` is whatever the script type's installVMCallbacks() put on the
    // VM -- captured by the provisioner, so subclass handlers are honored.
    explicit QuantaWatchdog(InterruptCallback handler)
        : mHandler(handler)
    {
    }

    virtual ~QuantaWatchdog();

    // Open a run window: bind `target` as THE window (exactly one is open per
    // provisioner, assert-enforced), reset its interrupt pointer to the
    // implementation's baseline, and schedule `deadline`, a reading in the
    // quanta clock's domain.
    virtual void arm(lua_Callbacks* target, double deadline) = 0;

    // Close out the window. Guarantees no future fire; does not touch the
    // pointer.
    virtual void disarm() = 0;

    // Install the handler in the open window's VM right now, so the next
    // safepoint runs it: what a deadline fire does, on demand. For conditions
    // that must be noticed mid-window (sleep, force-yield).
    virtual void fireNow() = 0;

    // Self-heal for a spurious install in the open window: uninstall only if
    // the window's fire is still pending, and report whether that happened.
    // A bare store in the handler could instead wipe a concurrent fire and
    // spend the window's one-shot.
    virtual bool clearIfArmed() = 0;

    // Lifetime totals of deadline fires. Zeros for policies where the
    // deadline never fires as a distinct event (the synchronous one).
    virtual WatchdogStats getStats() { return {}; }

protected:
    InterruptCallback mHandler = nullptr;
};

// The synchronous policy: arm installs the handler on the spot, so it stays
// resident and checks the clock at every safepoint.
std::unique_ptr<QuantaWatchdog> createImmediateQuantaWatchdog(InterruptCallback handler);
// Go-sysmon-shaped policy for real-time clocks: windows run with no interrupt
// installed, and a long-running watchdog thread -- asleep whenever no window
// is armed -- stores the handler slightly ahead of the deadline so the
// preemption lands on it rather than after it. Thread-creation failure
// propagates as std::system_error.
std::unique_ptr<QuantaWatchdog> createQuantaWatchdog(InterruptCallback handler, lua_clockProvider quantaClock);

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
    // Install the interrupt handler synchronously at window start (resident
    // at every safepoint) instead of from the watchdog thread at the
    // deadline. Required when quantaClockProvider doesn't advance with real
    // time, e.g. a test's stepped fake clock.
    bool synchronousQuantaArming = false;
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

    // The arming policy this provisioner's scripts use for their run windows.
    // Never null once an environment exists (scripts can't exist before one).
    virtual QuantaWatchdog* getQuantaWatchdog() const = 0;

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

    QuantaWatchdog* getQuantaWatchdog() const override { return mWatchdog.get(); }

    std::shared_ptr<IEnvironment> createEnvironment(bool is_lsl, uint32_t api_version) override
    {
        std::shared_ptr<Environment> environment = makeEnvironment(is_lsl, api_version);
        environment->build<S>();

        if (mWatchdog == nullptr)
        {
            // The first environment tells us which interrupt handler
            // `S::installVMCallbacks` really installs (it may be a subclass's
            // own, wrapping ours) -- every environment of this provisioner
            // installs the same one.
            InterruptCallback handler = lua_callbacks(environment->getBaseState())->interrupt;
            if (mCallbacks.synchronousQuantaArming)
                mWatchdog = createImmediateQuantaWatchdog(handler);
            else
                mWatchdog = createQuantaWatchdog(handler, mCallbacks.quantaClockProvider);
        }

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
    // Created with the first environment, once the installed interrupt
    // handler is known. Destruction joins the threaded impl's thread.
    std::unique_ptr<QuantaWatchdog> mWatchdog;
};

} // namespace Executor
} // namespace Luau
