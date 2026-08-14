// ServerLua: the per-script runtime instance driven by the script host.
#pragma once

#include <memory>
#include <string>

#include "lua.h"

#include "Luau/ByteStream.h"
#include "Luau/Executor.h"

namespace Luau
{
namespace Executor
{

enum class HandlerRunStatus : uint8_t
{
    // Handler ran to completion, handler thread was reset
    Ok = 0,
    // Handler was preempted or yielded mid-execution, resume it later
    Preempted,
    // LSL only. handler requested a state change and finished, newState holds
    // the new state number
    StateChange,
    // Handler errored, fault fields were updated, handler thread was reset
    Fault,
    // The script has no handler for this event
    NotRun,
    // There's already a handler function staged, we can't run this.
    Refused,
};

enum class FaultKind : uint8_t
{
    None = 0,
    Runtime,
    OutOfMemory,
    Timeout,
    UnexpectedYield,
    Unknown = 255,
};

struct RunResult
{
    HandlerRunStatus status = HandlerRunStatus::Ok;
    int newState = 0;
    // True when this run cleanly completed the SLua main function, so the
    // host can finalize its view of the handler registrations exactly once.
    bool mainFunctionCompleted = false;
};

// Payload format written by Script::serializeState(). The version covers the
// whole layout, so a subclass that appends its own fields has to bump its own
// fingerprint whenever this one moves.
constexpr StateFingerprint kScriptStateFingerprint{{'E', 'X', 'E', 'C'}, 1};

// We need to carry around the error messages in our state, but
// the messages can be arbitrarily large. Cap them.
constexpr size_t kMaxPersistedFaultLen = 2048;

// Callback used to push args after an event handler function to
// prepare it to be called.
using PushArgsFn = void (*)(lua_State* handler, void* ctx);

// Our analogue of the LLScriptExecute object present in indra.
class Script : protected lua_SLRuntimeState
{
public:
    Script(const std::shared_ptr<IImage>& image, const ScriptConfig& config);

    virtual ~Script();

    Script(const Script&) = delete;
    Script& operator=(const Script&) = delete;

    IProvisioner& getProvisioner() const { return mImage->getProvisioner(); }
    IImage& getImage() const { return *mImage; }
    IEnvironment& getEnvironment() const { return mImage->getEnvironment(); }
    bool isLSL() const { return mIsLSL; }
    uint32_t getAPIVersion() const { return mAPIVersion; }

    // Recover the script from any thread in its instance tree
    static Script* fromLuaState(lua_State* L);

    // Installs the interrupt, userthread and allocation callbacks a script
    // needs onto a freshly created VM. Generally done when the environment
    // is created.
    static void installVMCallbacks(lua_State* L);

    lua_State* getInstanceState() const { return mInstance.thread(); }
    // The handler thread while a handler is executing, null otherwise. Lets us
    // distinguish between the engine-created top-level handler thread and any
    // descendant threads created by the script.
    lua_State* getHandlerState() const { return mHandlerState; }
    // True while a forked instance exists (reset()/restoreState() succeeded)
    bool hasInstance() const { return (bool)mInstance; }

    // Forks the image's pristine default state into a fresh instance. Only
    // valid while no instance exists, same as restoreState().
    bool loadDefaultState();

    // Drops any live instance and returns the script to the image's default
    // state. Unlike loadDefaultState(), this is valid at any time.
    bool reset();

    // Start a run window for `quanta` seconds
    void beginRunWindow(double quanta);
    // True once the engine has decided the current run window is over, sticky
    // for the rest of the window.
    bool isYieldDue() const { return mYieldDue; }

    // Level-triggered forced preemption, port of the host's
    // `getReset() || !mIsEnabled` check. The host both sets and clears it.
    void setForceYield(bool force) { mForceYield = force; }

    void *getHostContext() const { return mHostContext; }

    // Call an event handler associated with the given state, works for both
    // LSL and Lua.
    RunResult callEventHandler(int lsl_state, const char* event_name, PushArgsFn pushArgs, void* push_args_ctx = nullptr);
    // Resumes a previously preempted handler (or the staged SLua main function)
    RunResult resumeEventHandler();
    // True when the handler thread holds a resumable in-progress handler
    bool isHandlerActive() const;
    // Discards a resumable in-progress handler without completing it, so a
    // host can recover from disagreement about whether a handler is in
    // flight. A no-op when nothing is resumable.
    void abortHandler();

    bool isMainFunctionComplete() const { return mMainFunctionComplete; }

    // Always returns false for Lua scripts, only used for LSL.
    bool hasLSLEventHandler(int lsl_state, const char* event_name);

    bool serializeState(std::string& out);

    // Rebuilds the instance from a serializeState() payload. Only valid while
    // no valid instance exists.
    bool restoreState(const char* data, size_t len);

    int getUsedMemory();
    bool setMemoryLimit(int memory_limit);
    void setMemoryLimitUnsafe(int memory_limit);
    int getMemoryLimit() const { return mMemoryLimit; }

    FaultKind getFaultKind() const { return mFaultKind; }
    const std::string& getFaultString() const { return mFaultString; }
    // Fault message plus a debug traceback when one was available
    const std::string& getExtendedFaultString() const { return mExtendedFaultString; }
    void clearFault();
    void setFault(FaultKind kind, const char* fault_string, const char* extended = nullptr);

    // How long we've been told to sleep. Never decremented, only zeroed out when the scheduler
    // decides we're done the sleep.
    float getSleep() const { return mSleep; }
    void setSleep(float sleep) { mSleep = sleep; }

protected:
    // Written at the head of every payload this class produces, and required to
    // match on restore. Subclasses return their own so a payload meant for
    // another class is refused before anything is forked.
    virtual StateFingerprint getStateFingerprint() const { return kScriptStateFingerprint; }

    // Durable subclass state, written after our own fields. A subclass partway
    // down a hierarchy chains to its base before writing or reading its own.
    virtual bool serializeExtra(ByteWriter&) const { return true; }
    virtual bool restoreExtra(ByteReader&) { return true; }

    static void interruptHandler(lua_State* L, int gc);
    static int memoryLimitCallback(lua_State* L, size_t osize, size_t nsize);
    static void userThreadCallback(lua_State* LP, lua_State* L);

    // Source string for log messages, so subclasses log with the same attribution
    const char* logSource() const { return mScriptId.c_str(); }

private:
    class ExecutionScope;
    class HandlerScope;

    void tryYield(lua_State* L, double elapsed, bool mandatory = false);
    void setLuaFaultFromStatus(lua_State* L, int status);
    RunResult handleEventHandlerStatus(int status);
    void destroyHandlerThread();
    void pushEventHandlerFunction(lua_State* L, int lsl_state, const char* event_name);

    // Opaque per-script host context copied onto the Script. In lscript terms,
    // this is the `LLScriptExecuteLuau` instance.
    void* mHostContext = nullptr;

    // Keeps the image (and through it the environment) alive for as long
    // as the script exists
    std::shared_ptr<IImage> mImage;

    // Cached properties from the parent `IImage` to reduce pointer chasing
    bool mIsLSL = false;
    uint32_t mAPIVersion = 0;
    size_t mChargedBytecodeSize = 0;
    std::string mScriptId;

    // Underlying state instance currently associated with this Script container.
    // The state itself may be nullptr as a sentinel to indicate that we're not
    // currently able to run this (e.g. due to a script fault, or not yet started)
    Instance mInstance;
    // Points at the handler thread only while a handler is executing
    lua_State* mHandlerState = nullptr;

    int mMemoryLimit = kDefaultMemoryLimit;
    int mExactSize = 0;
    int mMaxPossibleSize = 0;
    bool mExactSizeDirty = true;

    // Monotonic stopwatch used for quanta-elapsed measurement, seeded from the
    // provisioner's.
    lua_clockProvider mQuantaClockProvider = nullptr;

    // When did we start running
    double mWindowStart = 0.0;
    // How long are we supposed to run?
    double mQuanta = 0.0;
    float mSleep = 0.0f;
    // The engine has decided that the run window is over.
    bool mYieldDue = false;
    // We already threw a catchable error trying to force a yield.
    bool mMandatoryYieldRaised = false;
    bool mInExecution = false;
    bool mMainFunctionComplete = false;
    // The host has indicated that a yield should happen at the next interrupt.
    // TODO: Hmm. Seems whe might be able to merge this with `mYieldDue` if we're smart.
    bool mForceYield = false;

    FaultKind mFaultKind = FaultKind::None;
    std::string mFaultString;
    std::string mExtendedFaultString;
};

} // namespace Executor
} // namespace Luau
