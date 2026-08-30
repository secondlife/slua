#include "Luau/Script.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>

#include "lualib.h"
#include "llsl.h"

#include "lgc.h"
#include "lstate.h"

namespace Luau
{
namespace Executor
{

// Parks the GC for a run window by making luaC_needsGC unreachable. One under
// SIZE_MAX, because that exact value is the engine-internal limits-off
// sentinel: lmem skips the beforeallocate hook there, and serialization
// asserts on it meaning "currently serializing".
constexpr size_t kGCParkedThreshold = SIZE_MAX - 1;

// Buckets unrecognized persisted fault kinds instead of trusting the byte
static FaultKind validate_fault_kind(uint8_t kind)
{
    if (kind <= (uint8_t)FaultKind::UnexpectedYield || kind == (uint8_t)FaultKind::Unknown)
        return (FaultKind)kind;
    return FaultKind::Unknown;
}

// basically just lua_resume, but it catches hard OoM errors too.
static int safe_lua_resume(lua_State* L, lua_State* from, int nargs)
{
    try
    {
        return lua_resume(L, from, nargs);
    }
    catch (lua_exception& ex)
    {
        // What can escape lua_resume's protected path is an allocation failure
        // at the memory limit while the error object is being materialized.
        if (ex.getStatus() == LUA_ERRMEM)
        {
            return LUA_ERRMEM;
        }
        throw;
    }
}

// Used when we're executing something in the current scope. Generally
// only used via its subclass `HandlerScope` except in the LSL ctor case.
class Script::ExecutionScope
{
public:
    explicit ExecutionScope(Script* execute)
        : mExecute(execute)
    {
        LUAU_ASSERT(!execute->mInExecution);
        execute->mInExecution = true;
        // If we started running this then we expect there to be allocations.
        // The exact size is almost certainly going to be dirty.
        execute->mExactSizeDirty = true;
    }
    ~ExecutionScope()
    {
        mExecute->mInExecution = false;
        mExecute->mExactSizeDirty = true;
    }

private:
    Script* mExecute;
};

// Cleans up handler context when throwing
// Handler context is a lot more sensitive and we _really_ don't want to mess it up,
// so we use RAII to ensure it's always sane when the stack is unwound.
class Script::HandlerScope
{
public:
    HandlerScope(Script* execute, bool new_thread)
        : mExecute(execute)
        , mExecutionScope(execute)
    {
        lua_State* instance = execute->mInstance.thread();
        if (new_thread)
        {
            // Handler thread lives permanently at position 1 on the instance and gets
            // reset via lua_resetthread between handlers - no allocation needed here.
            LUAU_ASSERT(lua_gettop(instance) == 1);
            LUAU_ASSERT(lua_isthreadreset(lua_tothread(instance, 1)));
        }
        else
        {
            LUAU_ASSERT(lua_gettop(instance) == 1);
        }

        LUAU_ASSERT(!execute->mHandlerState);
        execute->mHandlerState = lua_tothread(instance, 1);
        LUAU_ASSERT(execute->mHandlerState != nullptr);
    }
    ~HandlerScope()
    {
        mExecute->mHandlerState = nullptr;
    }

private:
    Script* mExecute;
    ExecutionScope mExecutionScope;
};

Script::Script(const std::shared_ptr<IImage>& image, const ScriptConfig& config)
    // Each script gets (well, _is_) its own lua_SLRuntimeState.
    : lua_SLRuntimeState(image->getRuntimeState())
    , mHostContext(config.hostContext)
    , mImage(image)
    , mIsLSL(image->isLSL())
    , mAPIVersion(image->getAPIVersion())
    , mChargedBytecodeSize(image->getChargedBytecodeSize())
    , mScriptId(config.scriptId ? config.scriptId : "")
    , mMemoryLimit(config.memoryLimit)
{
    LUAU_ASSERT(image->isValid());

    // This will mark our `lua_State` (and those of any of our child threads) as being owned
    // by a `Script`, and that `L->userdata` may be cast to a `Script*` to get back to `this`
    // from our `mInstance`'s `lua_State`.
    slStateKind = LUA_SLSTATE_SCRIPT;

    // Copy all these callbacks into members so we have quick access.
    const HostCallbacks& callbacks = image->getProvisioner().getCallbacks();
    clockProvider = callbacks.clockProvider;
    performanceClockProvider = callbacks.performanceClockProvider;
    randomProvider = callbacks.randomProvider;
    setTimerEventCb = callbacks.setTimerEventCb;
    eventHandlerRegistrationCb = callbacks.eventHandlerRegistrationCb;
    mQuantaClockProvider = callbacks.quantaClockProvider;

    // Non-null from the environment's existence: the provisioner built the
    // watchdog when it built the first environment.
    mWatchdog = image->getProvisioner().getQuantaWatchdog();
    LUAU_ASSERT(mWatchdog != nullptr);
    mCallbacks = lua_callbacks(image->getEnvironment().getBaseState());
}

Script::~Script()
{
    LUAU_ASSERT(!mHandlerState && !mInExecution);
    // A teardown mid-window must disarm (a later fire would write into a
    // lua_Callbacks that dies with the VM) and unpark the GC of the
    // environment's VM, which outlives us.
    if (mRunWindowOpen)
    {
        mWatchdog->disarm();
        if (mInstance)
            mInstance.thread()->global->GCthreshold = mSavedGCThreshold;
    }
    // mInstance releases its anchor before mImage lets go of the VM, by
    // member declaration order.
}

// static
void Script::installVMCallbacks(lua_State* L)
{
    lua_Callbacks* callbacks = lua_callbacks(L);
    callbacks->interrupt = Script::interruptHandler;
    callbacks->userthread = Script::userThreadCallback;
    callbacks->beforeallocate = Script::memoryLimitCallback;
}

// Given an arbitrary `lua_State*`, see if it can be traced back to a
// particular `Script`
Script* Script::fromLuaState(lua_State* L)
{
    auto* runtime_state = static_cast<lua_SLRuntimeState*>(L->userdata);
    if (runtime_state == nullptr)
        return nullptr;
    if ((runtime_state->slIdentifier & LUA_SL_IDENTIFIER_MASK) != LUA_SL_IDENTIFIER)
        return nullptr;
    if (runtime_state->slStateKind != LUA_SLSTATE_SCRIPT)
        return nullptr;
    return static_cast<Script*>(runtime_state);
}

bool Script::reset()
{
    LUAU_ASSERT(!mInExecution);

    // Whatever was running is forfeit, so we always satisfy loadDefaultState()'s
    // "no live instance" precondition.
    mInstance = Instance();

    return loadDefaultState();
}

bool Script::loadDefaultState()
{
    LUAU_ASSERT(!mInExecution);

    if (mInstance)
    {
        logWarn(logSource(), "Refusing to load the default state over a live script instance");
        setFault(FaultKind::Runtime, "invalid script state");
        return false;
    }

    mMaxPossibleSize = 0;
    mExactSize = 0;
    mExactSizeDirty = true;
    mYieldDue = false;
    mMandatoryYieldRaised = false;
    mMandatoryDeadline = 0.0;
    mExcludedTime = 0.0;
    mSleep = 0.0f;
    // in LSL we treat the main function as complete, since it runs
    // while loading the image.
    mMainFunctionComplete = mIsLSL;
    clearFault();

    // Alright, let's fork off a new instance
    mInstance = mImage->forkInstance(this);
    if (!mInstance)
    {
        setFault(FaultKind::Runtime, "Failed to fork script instance");
        return false;
    }
    lua_State* instance = mInstance.thread();

    // Attach the LLEvents/LLTimers managers for SLua
    if (!mIsLSL)
    {
        luaSL_createeventmanager(instance);
        lua_pushvalue(instance, -1);
        lua_pushvalue(instance, -1);
        lua_setglobal(instance, "LLEvents");
        lua_setglobal(instance, LLEVENTS_GLOBAL_NAME);

        luaSL_createtimermanager(instance);
        lua_pushvalue(instance, -1);
        lua_setglobal(instance, "LLTimers");
        lua_setglobal(instance, LLTIMERS_GLOBAL_NAME);
    }

    if (mIsLSL)
    {
        // Let the "constructor" run for trivial LSL scripts
        int status;
        {
            ExecutionScope script_scope(this);
            // Make sure allocs are attributed to the user script
            MemcatGuard guard{instance, kUserMemcat};
            status = safe_lua_resume(instance, nullptr, 0);
        }

        // If the constructor failed or yielded... something terrible happened.
        if (status != LUA_OK)
        {
            setLuaFaultFromStatus(instance, status);
            // This is most likely to happen if someone induces OoMs due to list construction in the constructor.
            logWarn(logSource(), "LSL constructor failed somehow?");
            mInstance = Instance();
            return false;
        }

        // Create persistent handler thread. Stack is empty after constructor.
        lua_newthread(instance);
        LUAU_ASSERT(lua_getmemcat(lua_tothread(instance, 1)) == kUserMemcat);
    }
    else
    {
        // Create persistent handler thread and move the main function onto it.
        // The main function may define locals that handlers close over, but that's fine:
        // lua_resetthread calls luaF_close which migrates open upvalues to heap-allocated
        // copies, so handler closures in LLEvents/globals continue to work.
        lua_newthread(instance);
        lua_insert(instance, 1);
        LUAU_ASSERT(lua_getmemcat(lua_tothread(instance, 1)) == kUserMemcat);
        lua_xmove(instance, lua_tothread(instance, 1), 1);
    }

    LUAU_ASSERT(lua_gettop(instance) == 1);
    return true;
}

void Script::beginRunWindow(double quanta)
{
    LUAU_ASSERT(mInstance);
    LUAU_ASSERT(!mRunWindowOpen);
    // Sleep is a window output; the host banks and zeroes it before
    // scheduling us again.
    LUAU_ASSERT(mSleep == 0.0f);
    // Force-yield is a mid-window mechanism: a host that knows a script is
    // reset/disabled shouldn't be scheduling a window for it at all.
    LUAU_ASSERT(!mForceYield);
    lua_State* instance = mInstance.thread();

    mRunWindowOpen = true;
    mYieldDue = false;
    mMandatoryYieldRaised = false;
    mMandatoryDeadline = 0.0;
    mExcludedTime = 0.0;
    mWindowStart = mQuantaClockProvider(instance);
    mQuanta = quanta;

    // Park the GC for the window; endRunWindow() pays down the debt
    mSavedGCThreshold = instance->global->GCthreshold;
    instance->global->GCthreshold = kGCParkedThreshold;

    mWatchdog->arm(mCallbacks, mWindowStart + quanta);
}

void Script::endRunWindow()
{
    LUAU_ASSERT(mRunWindowOpen);
    mRunWindowOpen = false;
    // A force-yield's life ends with its window; the host re-asserts it if
    // it still wants one.
    mForceYield = false;

    mWatchdog->disarm();
    if (mInstance)
    {
        // Restore before stepping: the pacer computes its debt from the
        // threshold. This is the window's deferred GC work, on host time.
        lua_State* instance = mInstance.thread();
        instance->global->GCthreshold = mSavedGCThreshold;
        while (luaC_needsGC(instance))
            luaC_step(instance, false);
    }
}

void Script::setSleep(float sleep)
{
    mSleep = sleep;
    // A condition set mid-window must run the handler at the next safepoint.
    // Outside a window our safepoints aren't live and the member alone is
    // enough: a force-yield is read by the next begin's flush branch, and a
    // sleep is the host's to serve before it schedules us again.
    if (sleep > 0.0f && mRunWindowOpen)
        mWatchdog->fireNow();
}

void Script::setForceYield(bool force)
{
    mForceYield = force;
    if (force && mRunWindowOpen)
        mWatchdog->fireNow();
}

RunResult Script::callEventHandler(int lsl_state, const char* event_name, PushArgsFn pushArgs, void* push_args_ctx)
{
    if (isHandlerActive())
    {
        logWarn(logSource(), "Ignoring call to %s handler while another handler is resumable", event_name);
        return {HandlerRunStatus::Refused, 0};
    }

    // Track the new thread as mHandlerState and make sure it's cleaned up correctly
    HandlerScope handler_scope(this, true);
    MemcatGuard memcat_guard{mInstance.thread(), kUserMemcat};

    int status = LUA_OK;

    // Wrap setup in try/catch - stack growth or string operations can throw
    // LUA_ERRMEM if the script is at its memory limit.
    try
    {
        if (!mIsLSL)
        {
            lua_getfield(mHandlerState, LUA_REGISTRYINDEX, LLEVENTS_HANDLEEVENT_KEY);
            lua_getglobal(mHandlerState, LLEVENTS_GLOBAL_NAME);
        }
        else
        {
            pushEventHandlerFunction(mHandlerState, lsl_state, event_name);
        }

        if (!lua_isfunction(mHandlerState, 1))
        {
            // Either the host dispatched an event this script never handled,
            // or a handler global was replaced with a non-function.
            destroyHandlerThread();
            logWarn(logSource(), "No %s handler to call", event_name);
            return {HandlerRunStatus::NotRun, 0};
        }

        if (!mIsLSL)
        {
            // LLEvents needs a string event name before the args
            lua_pushstring(mHandlerState, event_name);
        }
    }
    catch (lua_exception& err)
    {
        // probably a memory error...
        status = err.getStatus();
    }

    if (status != LUA_OK)
        return handleEventHandlerStatus(status);

    const int arg_base = lua_gettop(mHandlerState);
    LUAU_ASSERT(arg_base == (mIsLSL ? 1 : 3));

    // push any arguments that need to be pushed onto the stack
    if (pushArgs)
    {
        try
        {
            pushArgs(mHandlerState, push_args_ctx);
        }
        catch (lua_exception& err)
        {
            // Sometimes we can't even allocate enough memory for the error message,
            // so Lua has to throw.
            status = err.getStatus();
        }
    }

    if (status != LUA_OK)
        return handleEventHandlerStatus(status);

    int num_args = lua_gettop(mHandlerState) - arg_base;

    if (!mIsLSL)
    {
        // Need to take `self` and `event_name` into account when calling into `LLEvents`!
        num_args += 2;
    }

    status = safe_lua_resume(mHandlerState, nullptr, num_args);
    return handleEventHandlerStatus(status);
}

RunResult Script::resumeEventHandler()
{
    // Resuming a reset handler thread would hand lua_resume an empty stack
    if (!isHandlerActive())
    {
        logWarn(logSource(), "Ignoring resume with no resumable handler");
        return {HandlerRunStatus::Refused, 0};
    }

    HandlerScope handler_scope(this, false);
    MemcatGuard memcat_guard{mInstance.thread(), kUserMemcat};

    int status = safe_lua_resume(mHandlerState, nullptr, 0);
    return handleEventHandlerStatus(status);
}

bool Script::isHandlerActive() const
{
    if (!mInstance || lua_gettop(mInstance.thread()) < 1)
        return false;
    lua_State* handler = lua_tothread(mInstance.thread(), 1);
    return handler != nullptr && !lua_isthreadreset(handler);
}

void Script::setLuaFaultFromStatus(lua_State* L, int status)
{
    if (status == LUA_OK)
        return;

    // Don't clobber an existing fault if we already have one
    if (mFaultKind != FaultKind::None)
        return;

    // Acts as a signal that we've registered all of our event handlers, and
    // don't need to be greedy anymore. Crashed scripts tell no tales.
    mMainFunctionComplete = true;

    if (status == LUA_YIELD)
    {
        mFaultKind = FaultKind::UnexpectedYield;
        mFaultString = "unexpected yield";
        mExtendedFaultString = "";
    }
    else if (status == LUA_BREAK)
    {
        mFaultKind = FaultKind::Timeout;
        mFaultString = "exceeded time limit";
        mExtendedFaultString = "";
    }
    else if (status == LUA_ERRMEM || lua_isstring(L, -1))
    {
        size_t len;
        const char* msg;
        if (status == LUA_ERRMEM)
        {
            msg = "not enough memory";
            len = strlen(msg);
        }
        else
            msg = lua_tolstring(L, -1, &len);
        std::string fault_string{msg, len};

        if (status == LUA_ERRMEM)
        {
            mFaultKind = FaultKind::OutOfMemory;
            mFaultString = fault_string;
        }
        else if (mMandatoryYieldRaised)
        {
            mFaultKind = FaultKind::Timeout;
            mFaultString = "exceeded time limit";
        }
        else
        {
            mFaultKind = FaultKind::Runtime;
            mFaultString = "runtime error";
        }

        mExtendedFaultString = fault_string;
        const char* traceback = lua_debugtrace(L);
        if (traceback != nullptr)
        {
            mExtendedFaultString += "\n";
            mExtendedFaultString += traceback;
        }

        logInfo(logSource(), "Script error: %s", msg);
    }
    else
    {
        mFaultKind = FaultKind::Unknown;
        mFaultString = "Unknown runtime error";
        mExtendedFaultString = "";
    }
}

RunResult Script::handleEventHandlerStatus(int status)
{
    switch (status)
    {
    case LUA_OK:
    {
        if (mFaultKind != FaultKind::None)
        {
            destroyHandlerThread();
            return {HandlerRunStatus::Fault, 0};
        }
        // Report the completion on the transition only, so the host acts on
        // it exactly once. LSL starts out complete and never reports.
        bool first_completion = !mMainFunctionComplete;
        mMainFunctionComplete = true;
        destroyHandlerThread();
        return {HandlerRunStatus::Ok, 0, first_completion};
    }
    case LUA_YIELD:
        if (mFaultKind != FaultKind::None)
        {
            destroyHandlerThread();
            return {HandlerRunStatus::Fault, 0};
        }
        if (mIsLSL && lua_gettop(mHandlerState) == 1)
        {
            // Just yielded a new state number, mark this handler finished.
            // Setting the next state register and state_exit queueing are the
            // host's job.
            int isnum = 0;
            int new_state = (int)lua_tounsignedx(mHandlerState, 1, &isnum);
            if (!isnum)
            {
                setFault(FaultKind::Runtime, "invalid state change");
                destroyHandlerThread();
                return {HandlerRunStatus::Fault, 0};
            }
            destroyHandlerThread();
            return {HandlerRunStatus::StateChange, new_state};
        }
        return {HandlerRunStatus::Preempted, 0};
    case LUA_BREAK:
        if (mFaultKind != FaultKind::None)
        {
            destroyHandlerThread();
            return {HandlerRunStatus::Fault, 0};
        }
        return {HandlerRunStatus::Preempted, 0};
    default:
        setLuaFaultFromStatus(mHandlerState, status);
        destroyHandlerThread();
        return {HandlerRunStatus::Fault, 0};
    }
}

void Script::destroyHandlerThread()
{
    lua_State* instance = mInstance.thread();
    LUAU_ASSERT(lua_gettop(instance) == 1 && lua_tothread(instance, 1) == mHandlerState);
    lua_resetthread(lua_tothread(instance, 1));
    mHandlerState = nullptr;
}

void Script::abortHandler()
{
    LUAU_ASSERT(!mInExecution);
    if (!isHandlerActive())
        return;
    lua_resetthread(lua_tothread(mInstance.thread(), 1));
}

void Script::pushEventHandlerFunction(lua_State* L, int lsl_state, const char* event_name)
{
    LUAU_ASSERT(mIsLSL);

    // We don't want any allocs here to count against the user
    MemcatGuard guard{L, kSystemMemcat};

    // Figure out what the function name for this handler should be
    char eh_global_name[256] = {0};
    snprintf(eh_global_name, sizeof(eh_global_name), "_e%d/%s", lsl_state, event_name);
    lua_getglobal(L, eh_global_name);
}

bool Script::hasLSLEventHandler(int lsl_state, const char* event_name)
{
    if (!mInstance)
        return false;

    if (!mIsLSL)
        return false;

    lua_State* instance = mInstance.thread();
    pushEventHandlerFunction(instance, lsl_state, event_name);
    bool has_handler = lua_isfunction(instance, -1);
    lua_pop(instance, 1);
    return has_handler;
}

bool Script::serializeState(std::string& out)
{
    LUAU_ASSERT(!mInExecution);

    out.clear();

    if (!mInstance)
        return false;

    std::string ares_blob;
    if (!mImage->serializeInstance(mInstance, ares_blob))
        return false;

    const StateFingerprint fingerprint = getStateFingerprint();

    ByteWriter writer{out};
    writer.writeBytes(fingerprint.tag, sizeof(fingerprint.tag));
    writer.writeU32(fingerprint.version);
    writer.writeF32(mSleep);
    // The limit drifts at runtime (llSetMemoryLimit), so it is engine state
    // the payload must carry
    writer.writeU32((uint32_t)mMemoryLimit);
    writer.writeU8((uint8_t)mFaultKind);
    writer.writeString(mFaultString);
    writer.writeString(mExtendedFaultString.data(), std::min(mExtendedFaultString.size(), kMaxPersistedFaultLen));
    writer.writeU8((uint8_t)mMainFunctionComplete);
    writer.writeString(ares_blob);

    // The image flavor this state came off, so restoring it into the wrong
    // image fails here rather than somewhere inside Ares
    writer.writeU8((uint8_t)mIsLSL);
    writer.writeU32(mAPIVersion);

    return serializeExtra(writer);
}

bool Script::restoreState(const char* data, size_t len)
{
    LUAU_ASSERT(!mInExecution);

    // Parse the whole payload before touching anything, so a bad one leaves
    // us exactly as we were
    ByteReader reader{data, len};
    uint32_t version = 0;
    float sleep = 0.0f;
    uint32_t memory_limit = 0;
    uint8_t fault_kind = 0;
    std::string fault_string;
    std::string extended_fault_string;
    uint8_t main_function_complete = 0;
    std::string ares_blob;
    uint8_t is_lsl = 0;
    uint32_t api_version = 0;

    const StateFingerprint fingerprint = getStateFingerprint();

    char tag[sizeof(fingerprint.tag)];
    if (!reader.readBytes(tag, sizeof(tag)) || memcmp(tag, fingerprint.tag, sizeof(tag)) != 0)
    {
        logWarn(logSource(), "Script state has no recognisable header");
        setFault(FaultKind::Runtime, "invalid script state");
        return false;
    }
    // Payloads from older builds still load. ones from a newer build we can
    // only refuse, since we don't know what moved.
    if (!reader.readU32(version) || version > fingerprint.version)
    {
        logWarn(logSource(), "Unsupported script state version %u", version);
        setFault(FaultKind::Runtime, "invalid script state");
        return false;
    }
// Every field fails the same way, keep each read to one line
#define READ_OR_BAIL(read_expr) \
    do \
    { \
        if (!(read_expr)) \
        { \
            logWarn(logSource(), "Script state is truncated"); \
            setFault(FaultKind::Runtime, "invalid script state"); \
            return false; \
        } \
    } while (false)

    READ_OR_BAIL(reader.readF32(sleep));
    READ_OR_BAIL(reader.readU32(memory_limit));
    READ_OR_BAIL(reader.readU8(fault_kind));
    READ_OR_BAIL(reader.readString(fault_string));
    READ_OR_BAIL(reader.readString(extended_fault_string));
    READ_OR_BAIL(reader.readU8(main_function_complete));
    READ_OR_BAIL(reader.readString(ares_blob));
    READ_OR_BAIL(reader.readU8(is_lsl));
    READ_OR_BAIL(reader.readU32(api_version));

#undef READ_OR_BAIL
    if ((is_lsl != 0) != mIsLSL || api_version != mAPIVersion)
    {
        logWarn(logSource(), "Script state is for a different flavor (lsl %d, api %u)", (int)is_lsl, api_version);
        setFault(FaultKind::Runtime, "invalid script state");
        return false;
    }

    if (memory_limit <= 0 || memory_limit > INT32_MAX)
    {
        logWarn(logSource(), "Script state carries an unusable memory limit");
        setFault(FaultKind::Runtime, "invalid script state");
        return false;
    }

    if (mInstance)
    {
        logWarn(logSource(), "Refusing to restore state over a live script instance");
        setFault(FaultKind::Runtime, "invalid script state");
        return false;
    }

    mInstance = mImage->forkInstance(this, &ares_blob);
    if (!mInstance)
    {
        setFault(FaultKind::Runtime, "invalid script state");
        return false;
    }

    // Subclass state comes last, once there's a live instance for it to key off.
    // Anything left over means we didn't understand the whole payload.
    if (!restoreExtra(reader) || reader.remaining != 0)
    {
        logWarn(logSource(), "Script state's trailing section is unusable");
        setFault(FaultKind::Runtime, "invalid script state");
        mInstance = Instance();
        return false;
    }

    mSleep = sleep;
    setMemoryLimitUnsafe((int)memory_limit);
    mFaultKind = validate_fault_kind(fault_kind);
    mFaultString = fault_string;
    mExtendedFaultString = extended_fault_string;
    mMainFunctionComplete = main_function_complete != 0;
    mMaxPossibleSize = 0;
    mExactSize = 0;
    mExactSizeDirty = true;
    return true;
}

// static
void Script::interruptHandler(lua_State* L, int gc)
{
    Script* execute = fromLuaState(L);

    // If it's not a Script object or we're not executing a handler state, we don't really care.
    if (!execute || !execute->mInExecution || execute->mHandlerState == nullptr)
        return;

    // GC is parked for every run window, so a GC bracket can't arrive
    // mid-execution; the end-of-window catch-up steps return at the gate
    // above.
    if (gc >= 0)
        return;

    // This particular handler had better be _active_, or we're somehow triggering
    // interrupts through pushing handler arguments or something like that.
    LUAU_ASSERT(execute->mHandlerState->isactive);

    if (execute->mSleep > 0.0f || execute->mForceYield)
    {
        execute->tryYield(L, 0.0, true);
        return;
    }

    double elapsed = execute->mQuantaClockProvider(L) - execute->mWindowStart;
    if (elapsed > execute->mQuanta)
    {
        execute->tryYield(L, elapsed);
    }
    else
    {
        // A spurious install (the deadline hadn't actually passed): uninstall
        // through the watchdog, where a bare store could wipe a concurrent
        // fire and spend the window's one-shot.
        execute->mWatchdog->clearIfArmed();
    }
}

// static
void Script::userThreadCallback(lua_State* LP, lua_State* L)
{
    if (LP == nullptr)
    {
        // Thread destruction case
        return;
    }
    // Copy thread data pointer to the child thread
    lua_setthreaddata(L, lua_getthreaddata(LP));
}

// static
// Helper callback that enforces reachability-based memory limits
int Script::memoryLimitCallback(lua_State* L, size_t osize, size_t nsize)
{
    Script* execute = fromLuaState(L);

    // Meh, not even running a script.
    if (!execute || !execute->mInExecution)
        return 0;

    if (!execute->mInstance)
    {
        // It's not deserialized yet, meh.
        return 0;
    }

    // This is a net shrink in memory, not relevant for our limiting logic. We can't assume that
    // memory being freed has anything to do with us, given that the GC can work on things unrelated
    // to the currently executing task.
    if (osize >= nsize)
        return 0;

    size_t net_gain = nsize - osize;

    // A single allocation larger than the whole limit can never fit, reject it
    // here so the int-typed accounting below can't overflow.
    if (net_gain > (size_t)execute->mMemoryLimit)
        return 1;

    // Figure out the actual current size of the heap if we didn't already have it,
    // or the alloc would push us over the limit given the current approximate size.
    execute->mMaxPossibleSize += (int)net_gain;

    if (execute->mExactSize == 0 || (execute->mMaxPossibleSize > execute->mMemoryLimit))
    {
        // Max possible size is now over the memory limit, or we didn't have a "actual"
        // base memory measurement before. Recalculate "actual" memory usage.

        // Discount only the baseline measurement's walk: its timing depends on engine
        // bookkeeping state, while walks forced by allocating near the limit are the
        // script's own work and stay charged.
        bool discount_walk = execute->mExactSize == 0 && execute->mHandlerState != nullptr;
        double walk_start = discount_walk ? execute->mQuantaClockProvider(L) : 0.0;

        size_t current_size = lua_userthreadgc(execute->mInstance.thread(), &execute->mImage->getFreeObjects()) + execute->mChargedBytecodeSize;

        if (discount_walk)
            execute->mExcludedTime += execute->mQuantaClockProvider(L) - walk_start;

        execute->mExactSize = execute->mMaxPossibleSize = (int)current_size;
        int new_size = execute->mExactSize + (int)net_gain;
        if (new_size > execute->mMemoryLimit)
            return 1;

        execute->mMaxPossibleSize = execute->mExactSize = new_size;
    }

    return 0;
}

static inline float calculate_slowness_punishment(double quanta, double elapsed)
{
    // Punish the script by forcing it to sleep 5x as long as it ran over its quanta
    constexpr float PUNISHMENT_MULTIPLIER = 5.0f;
    return (float)(std::max(elapsed - quanta, 0.0) * PUNISHMENT_MULTIPLIER);
}

void Script::tryYield(lua_State* L, double elapsed, bool mandatory)
{
    YieldableStatus interrupt_status = luaSL_may_interrupt(L);
    // Some quantas are extremely small, so don't punish if they were overrun due to blocking code.
    // Use a higher threshold for punishment in those cases.
    double punish_quanta = std::max(mQuanta, 0.001);
    // Discount time spent on work outside the script's control
    double charged = std::max(elapsed - mExcludedTime, 0.0);

    if (interrupt_status == YieldableStatus::OK)
    {
        // Reaching a yieldable point means any catchable mandatory-yield error
        // was caught and the script handed control back, so it has earned
        // a forced sleep instead.
        mMandatoryYieldRaised = false;
        mMandatoryDeadline = 0.0;

        // If the script was really abusive we need to punish it.
        // NB: This is mostly a copy of the existing Mono logic, except we only apply
        //  the punishment when the script is actually able to yield, rather than continuously
        //  try to punish the script over and over.
        if (charged > (punish_quanta * 3.0))
        {
            if (!mYieldDue)
                mSleep += calculate_slowness_punishment(punish_quanta, charged);
            logInfo(logSource(), "Punishing abusive script that took %f (charged %f) on quanta of %f", elapsed, charged, mQuanta);
        }
        // This will get us out of the host's `runQuanta()` loop
        mYieldDue = true;
        lua_break(L);
    }
    else
    {
        // Okay, we couldn't yield, but might it be time for capital punishment?
        if (charged > (punish_quanta * 3.5))
        {
            // They're over the time limit. Set a deadline. Don't kill immediately in case
            // a temporary performance blip caused them to go over time. The deadline is
            // in charged time so excluded work (GC steps, heap walks) can't eat the grace.
            if (mMandatoryDeadline == 0.0)
            {
                mMandatoryDeadline = charged + punish_quanta * 0.5;
            }
            else if (charged >= mMandatoryDeadline)
            {
                // It's very easy for people to intentionally write un-yieldable code. Punish them
                // by killing their evil script. Yield is now mandatory.
                logInfo(logSource(), "Making yield mandatory due to quanta overrun (wall %f, charged %f)", elapsed, charged);
                mandatory = true;
            }
        }

        if (mandatory)
        {
            // Even if they eventually recover from the error, they'll have to eat
            // this cost eventually.
            if (!mYieldDue)
                mSleep += calculate_slowness_punishment(punish_quanta, charged);

            // This will get us out of the host's `runQuanta()` loop
            mYieldDue = true;

            if (!mMandatoryYieldRaised)
            {
                // We're going to be polite. Throw a typical Lua error to try to
                // get back to a yieldable place in the script.
                mMandatoryYieldRaised = true;
                luaL_errorL(L, "Failed to perform mandatory yield");
            }

            // Here we are again. Okay, time to do an uncatchable killerror.
            // By convention, this renders the script inoperable rather than
            // letting the script run wild. Set the fault, make the kill.
            std::string extended = "Failed to perform mandatory yield";
            const char* traceback = lua_debugtrace(L);
            if (traceback != nullptr)
            {
                extended += '\n';
                extended += traceback;
            }
            setFault(FaultKind::Timeout, "exceeded time limit", extended.c_str());

            lua_killerror(L, "Failed to perform mandatory yield");
        }
    }
}

int Script::getUsedMemory()
{
    int bc_size = (int)mChargedBytecodeSize;
    if (!mInstance)
        return bc_size;

    // This check is basically free if we're not currently running and we haven't allocated
    // since it was last calculated
    if (!mExactSizeDirty && !mInExecution && mMaxPossibleSize == mExactSize)
        return mExactSize;

    mExactSize = mMaxPossibleSize = (int)lua_userthreadgc(mInstance.thread(), &mImage->getFreeObjects()) + bc_size;
    mExactSizeDirty = false;
    return mExactSize;
}

bool Script::setMemoryLimit(int memory_limit)
{
    int current_memory = getUsedMemory();
    // Shrinking the limit is okay even if we're above the typical limit.
    if (memory_limit >= current_memory && (memory_limit <= kDefaultMemoryLimit || memory_limit <= mMemoryLimit))
    {
        mMemoryLimit = memory_limit;
        return true;
    }
    return false;
}

void Script::setMemoryLimitUnsafe(int memory_limit)
{
    mMemoryLimit = std::max(0, memory_limit);
}

void Script::clearFault()
{
    mFaultKind = FaultKind::None;
    mFaultString.clear();
    mExtendedFaultString.clear();
}

void Script::setFault(FaultKind kind, const char* fault_string, const char* extended)
{
    // Don't clobber an existing fault if we already have one
    if (mFaultKind != FaultKind::None)
        return;
    // Well if we have a fault, then the main function is _definitely_ done executing.
    mMainFunctionComplete = true;
    mFaultKind = kind;
    mFaultString = fault_string ? fault_string : "";
    mExtendedFaultString = extended ? extended : "";
}

} // namespace Executor
} // namespace Luau
