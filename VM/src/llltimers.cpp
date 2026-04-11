#define llltimers_c

#include <cstring>
#include <cmath>
#include "lua.h"
#include "lcommon.h"
#include "lualib.h"
#include "llsl.h"
#include "llltimers.h"
#include "lyieldablemacros.h"

#include "lobject.h"
#include "lstate.h"
#include "lgc.h"
#include "lapi.h"
#include "lstring.h"

// Timer data table array indices
enum TimerDataIndex {
    TIMER_HANDLER = 1,
    TIMER_INTERVAL = 2,
    TIMER_NEXT_RUN = 3,              // Time at which this timer is next eligible to fire
    TIMER_LOGICAL_SCHEDULE = 4,      // Historical slot: kept for serialization stability, mirrors TIMER_NEXT_RUN
    TIMER_LEN = TIMER_LOGICAL_SCHEDULE,
};

// Timer event wrapper for LLEvents integration
int timer_event_wrapper(lua_State *L)
{
    // Get _tick function from registry
    lua_getfield(L, LUA_REGISTRYINDEX, "LLTIMERS_TICK");
    // Push LLTimers instance from upvalue as self argument
    lua_pushvalue(L, lua_upvalueindex(1));
    // Call _tick(LLTimers)
    lua_call(L, 1, 0);
    if (L->status == LUA_YIELD || L->status == LUA_BREAK)
        return -1;

    return 0;
}

int timer_event_wrapper_cont(lua_State *L, [[maybe_unused]] int status)
{
    return 0;
}

// Forward declarations for timer wrapper management
static bool is_timer_wrapper_registered(lua_State *L, lua_LLTimers *lltimers);
static void register_timer_wrapper(lua_State *L, lua_LLTimers *lltimers);
static void unregister_timer_wrapper(lua_State *L, lua_LLTimers *lltimers);
static void schedule_next_tick(lua_State *L, lua_LLTimers *lltimers);

static void lltimers_dtor(lua_State *L, void *data)
{
    auto *lltimers = (lua_LLTimers *)data;

    // Just clean up our own references.
    // Note: During normal operation, unregister_timer_wrapper is called
    // by schedule_next_tick when the last timer is removed. During GC/shutdown,
    // we cannot safely access LLEvents as it may already be freed.

    if (lltimers->timers_tab_ref != -1)
    {
        lua_unref(L, lltimers->timers_tab_ref);
        lltimers->timers_tab_ref = -1;
    }

    if (lltimers->llevents_ref != -1)
    {
        lua_unref(L, lltimers->llevents_ref);
        lltimers->llevents_ref = -1;
    }

    if (lltimers->timer_wrapper_ref != -1)
    {
        lua_unref(L, lltimers->timer_wrapper_ref);
        lltimers->timer_wrapper_ref = -1;
    }
}

int luaSL_createtimermanager(lua_State *L)
{
    // Validate LLEvents parameter on top of stack
    if (!lua_touserdatatagged(L, -1, UTAG_LLEVENTS))
        luaL_typeerror(L, -1, "LLEvents");

    lua_checkstack(L, 5);

    // Store LLEvents reference and remove from stack
    int llevents_ref = lua_ref(L, -1);
    lua_pop(L, 1);

    auto *lltimers = (lua_LLTimers *)lua_newuserdatataggedwithmetatable(L, sizeof(lua_LLTimers), UTAG_LLTIMERS);
    lltimers->timers_tab_ref = -1;      // Initialize all refs to -1 before allocations
    lltimers->llevents_ref = -1;
    lltimers->timer_wrapper_ref = -1;

    // Create empty timers array
    lua_createtable(L, 0, 0);
    lltimers->timers_tab_ref = lua_ref(L, -1);
    lltimers->timers_tab = hvalue(luaA_toobject(L, -1));
    lua_pop(L, 1);

    // Store LLEvents reference
    lltimers->llevents_ref = llevents_ref;

    // Create timer wrapper with LLTimers as upvalue
    lua_pushvalue(L, -1); // Push LLTimers as upvalue
    lua_pushcclosurek(L, timer_event_wrapper, "timer_wrapper", 1, timer_event_wrapper_cont);
    lltimers->timer_wrapper_ref = lua_ref(L, -1);
    lua_pop(L, 1);

    return 1;
}

static int lltimers_on(lua_State *L)
{
    auto *sl_state = LUAU_GET_SL_VM_STATE(lua_mainthread(L));

    auto *lltimers = (lua_LLTimers *)lua_touserdatatagged(L, 1, UTAG_LLTIMERS);
    if (!lltimers)
        luaL_typeerror(L, 1, "LLTimers");

    double seconds = luaL_checknumber(L, 2);
    if (!lua_iscallable(L, 3))
        luaL_typeerror(L, 3, "function or callable table");
    lua_settop(L, 3);

    if (seconds < 0.0 || std::isnan(seconds))
        luaL_errorL(L, "timer interval must be a positive number or 0");

    // Get current time
    double current_time = sl_state->clockProvider ? sl_state->clockProvider(L) : 0.0;

    // Get the timers array
    lua_getref(L, lltimers->timers_tab_ref);
    int old_len = lua_objlen(L, -1);

    // Create timer data table
    lua_createtable(L, 4, 0);
    lua_pushvalue(L, 3);
    lua_rawseti(L, -2, TIMER_HANDLER);
    lua_pushnumber(L, seconds);
    lua_rawseti(L, -2, TIMER_INTERVAL);
    double next_run_time = current_time + seconds;
    lua_pushnumber(L, next_run_time);
    lua_rawseti(L, -2, TIMER_NEXT_RUN);
    lua_pushnumber(L, next_run_time);
    lua_rawseti(L, -2, TIMER_LOGICAL_SCHEDULE);

    LUAU_ASSERT(lua_objlen(L, -1) == TIMER_LEN);

    // Add to the end of the timers array
    lua_rawseti(L, -2, old_len + 1);
    luau_shrinktable(L, -1, 0);

    // Register timer wrapper with LLEvents when adding first timer
    // (also check if already registered, in case we're inside a handler that just removed a timer)
    if (old_len == 0 && !is_timer_wrapper_registered(L, lltimers))
        register_timer_wrapper(L, lltimers);

    // Reschedule timer tick since we added a new timer
    schedule_next_tick(L, lltimers);

    lua_pop(L, 1); // Pop timers array

    // Return the passed-in handler so it can be unregistered later
    lua_pushvalue(L, 3);
    return 1;
}

static int lltimers_once(lua_State *L)
{
    auto *sl_state = LUAU_GET_SL_VM_STATE(lua_mainthread(L));

    auto *lltimers = (lua_LLTimers *)lua_touserdatatagged(L, 1, UTAG_LLTIMERS);
    if (!lltimers)
        luaL_typeerror(L, 1, "LLTimers");

    double seconds = luaL_checknumber(L, 2);
    if (!lua_iscallable(L, 3))
        luaL_typeerror(L, 3, "function or callable table");
    lua_settop(L, 3);

    if (seconds < 0.0 || std::isnan(seconds))
        luaL_errorL(L, "timer interval must be a positive number or 0");

    // Get current time
    double current_time = sl_state->clockProvider ? sl_state->clockProvider(L) : 0.0;

    // Get the timers array
    lua_getref(L, lltimers->timers_tab_ref);
    int old_len = lua_objlen(L, -1);

    // Create timer data table
    lua_createtable(L, 4, 0);
    lua_pushvalue(L, 3);
    lua_rawseti(L, -2, TIMER_HANDLER);
    lua_pushnil(L);
    lua_rawseti(L, -2, TIMER_INTERVAL);
    double next_run_time = current_time + seconds;
    lua_pushnumber(L, next_run_time);
    lua_rawseti(L, -2, TIMER_NEXT_RUN);
    lua_pushnumber(L, next_run_time);
    lua_rawseti(L, -2, TIMER_LOGICAL_SCHEDULE);
    LUAU_ASSERT(lua_objlen(L, -1) == TIMER_LEN);

    // Add to the timers array
    lua_rawseti(L, -2, old_len + 1);
    luau_shrinktable(L, -1, 0);

    // Reschedule timer tick since we added a new timer
    schedule_next_tick(L, lltimers);

    lua_pop(L, 1); // Pop timers array

    // Register timer wrapper with LLEvents when adding first timer
    // (also check if already registered, in case we're inside a handler that just removed a timer)
    if (old_len == 0 && !is_timer_wrapper_registered(L, lltimers))
        register_timer_wrapper(L, lltimers);

    // Return the passed-in handler so it can be unregistered later
    lua_pushvalue(L, 3);
    return 1;
}

static int lltimers_off(lua_State *L)
{
    auto *lltimers = (lua_LLTimers *)lua_touserdatatagged(L, 1, UTAG_LLTIMERS);
    if (!lltimers)
        luaL_typeerror(L, 1, "LLTimers");

    if (!lua_iscallable(L, 2))
        luaL_typeerror(L, 2, "function or callable table");
    lua_settop(L, 2);

    // Get the timers array
    lua_getref(L, lltimers->timers_tab_ref);

    // Find and remove timer by handler function
    // Timers are removed from the _back_
    int len = lua_objlen(L, -1);
    bool found = false;

    for (int i = len; i >= 1; i--)
    {
        lua_rawgeti(L, -1, i); // Get timer data table
        lua_rawgeti(L, -1, TIMER_HANDLER); // Get handler

        // if is equal to the handler that was passed in
        if (lua_rawequal(L, -1, 2))
        {
            // Found it, remove from array
            lua_pop(L, 2); // Pop handler and timer data

            // Shift elements down
            for (int j = i; j < len; j++)
            {
                lua_rawgeti(L, -1, j + 1);
                lua_rawseti(L, -2, j);
            }
            lua_pushnil(L);
            lua_rawseti(L, -2, len);

            found = true;
            break;
        }
        lua_pop(L, 2); // Pop handler and timer data
    }

    luau_shrinktable(L, -1, 0);

    lua_pop(L, 1); // Pop timers array

    // Reschedule timer tick since we may have removed a timer
    // (this will also unregister the wrapper if we removed the last timer)
    if (found)
    {
        schedule_next_tick(L, lltimers);
    }

    lua_pushboolean(L, found);
    return 1;
}

// Stack indices for yield-safe timer tick
enum TickStack {
    LLTIMERS_USERDATA = 2,
    CLONED_TIMERS_ARRAY = 3,
    LIVE_TIMERS_ARRAY = 4,
    CURRENT_TIMER = 5,
    HANDLER_FUNC = 6,
};

// Check if our timer wrapper is already registered with LLEvents
static bool is_timer_wrapper_registered(lua_State *L, lua_LLTimers *lltimers)
{
    int top = lua_gettop(L);

    // Get LLEvents userdata and its handlers table
    // We can't use `:handlers()` because we lie about the closure identity
    // of our tick wrapper so users can't accidentally unregister it :)
    lua_getref(L, lltimers->llevents_ref);
    auto *llevents = (lua_LLEvents *)lua_touserdata(L, -1);
    lua_getref(L, llevents->handlers_tab_ref);

    // Get "timer" handlers array
    lua_rawgetfield(L, -1, "timer");
    if (lua_isnil(L, -1))
    {
        lua_settop(L, top);
        return false;
    }

    // Get our wrapper to compare against
    lua_getref(L, lltimers->timer_wrapper_ref);

    // Handlers are wrapped in tables: wrapper[1] = handler
    // Check if any wrapper[1] matches our timer wrapper
    int len = lua_objlen(L, -2);
    for (int i = 1; i <= len; i++)
    {
        lua_rawgeti(L, -2, i);  // Get wrapper table
        lua_rawgeti(L, -1, 1);  // Get wrapper[1] (the actual handler)
        if (lua_rawequal(L, -1, -3))  // Compare with our wrapper
        {
            lua_settop(L, top);
            return true;
        }
        lua_pop(L, 2);  // Pop handler and wrapper table
    }

    lua_settop(L, top);
    return false;
}

// Register timer wrapper with LLEvents when first timer is added
static void register_timer_wrapper(lua_State *L, lua_LLTimers *lltimers)
{
    int top = lua_gettop(L);

    // Get LLEvents instance
    lua_getref(L, lltimers->llevents_ref);

    // Get the :on() method
    lua_getuserdatametatable(L, UTAG_LLEVENTS);
    lua_rawgetfield(L, -1, "on");
    lua_remove(L, -2); // Remove metatable

    // Call LLEvents:on("timer", wrapper)
    lua_pushvalue(L, -2); // LLEvents self
    lua_pushstring(L, "timer");
    lua_getref(L, lltimers->timer_wrapper_ref); // timer wrapper
    lua_call(L, 3, 0);

    lua_settop(L, top);
}

// Unregister timer wrapper from LLEvents when last timer is removed
static void unregister_timer_wrapper(lua_State *L, lua_LLTimers *lltimers)
{
    int top = lua_gettop(L);

    // Get LLEvents instance
    lua_getref(L, lltimers->llevents_ref);

    // Get the :off() method
    lua_getuserdatametatable(L, UTAG_LLEVENTS);
    lua_rawgetfield(L, -1, "off");
    lua_remove(L, -2); // Remove metatable

    // Call LLEvents:off("timer", wrapper)
    lua_pushvalue(L, -2); // LLEvents self
    lua_pushstring(L, "timer");
    lua_getref(L, lltimers->timer_wrapper_ref); // timer wrapper
    lua_call(L, 3, 0);

    lua_settop(L, top);
}

// Helper function to schedule the next timer tick
// Accepts LLTimers userdata as parameter and manages its own stack
static void schedule_next_tick(lua_State *L, lua_LLTimers *lltimers)
{
    auto *sl_state = LUAU_GET_SL_VM_STATE(lua_mainthread(L));

    if (!sl_state->setTimerEventCb)
        return; // No callback set, can't schedule

    // Get timers array on the stack
    lua_getref(L, lltimers->timers_tab_ref);
    int len = lua_objlen(L, -1);

    if (len == 0)
    {
        lua_pop(L, 1); // Pop timers array

        // No timers pending, unsubscribe from the parent timer event
        sl_state->setTimerEventCb(L, 0.0);

        // Unregister timer wrapper from LLEvents
        unregister_timer_wrapper(L, lltimers);

        return;
    }

    // Figure out when we next need to wake up
    double min_time = HUGE_VAL;
    for (int i = 1; i <= len; i++)
    {
        lua_rawgeti(L, -1, i);  // Get timer from timers array
        lua_rawgeti(L, -1, TIMER_NEXT_RUN);
        double next_run = lua_tonumber(L, -1);
        lua_pop(L, 2); // Pop nextRun and timer data
        min_time = fmin(min_time, next_run);
    }

    lua_pop(L, 1); // Pop timers array

    // Get current time
    double current_time = sl_state->clockProvider ? sl_state->clockProvider(L) : 0.0;

    // Timer events are relative, but our stored times are absolute.
    // If the timer should have already run (maybe because of a long, blocking handler),
    // schedule a re-invocation for 1 microsecond from now.
    // This is preferable to just looping back from the start within the continuation
    // because it prevents timers from accidentally blocking the ability for the
    // host's event manager to process other events between timers.
    double next_interval = fmax(0.000001, min_time - current_time);
    sl_state->setTimerEventCb(L, next_interval);
}

// Forward-declare continuation for is_already_in_tick.
static int lltimers_tick_v0_k(lua_State *L, int status);

// Check if we're already inside a _tick() call by walking the call stack
static bool is_already_in_tick(lua_State *L)
{
    // Walk up the call stack looking for lltimers_tick_v0_k
    // We start from L->ci - 1 because L->ci is the current (new) call to _tick
    for (CallInfo* ci = L->ci - 1; ci > L->base_ci; ci--)
    {
        // Get the function from this call frame
        if (!ttisfunction(ci->func))
            continue;

        Closure* cl = clvalue(ci->func);

        // Check if this is a C function with our continuation
        if (cl->isC && cl->c.cont == lltimers_tick_v0_k)
        {
            // Found _tick() higher in the call stack - we're reentrant!
            return true;
        }
    }

    return false;
}

DEFINE_YIELDABLE(lltimers_tick, 0)
{
    YIELDABLE_RETURNS_DEFAULT;

    enum class Phase : uint8_t {
        DEFAULT = 0,
        INTERRUPT_CHECK = 1,
        CALL_HANDLER = 2,
    };

    SlotManager slots(L, is_init);

    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, timer_index, 1);
    DEFINE_SLOT(int32_t, timers_len, 0);
    slots.finalize();

    if (is_init)
    {
        // SlotManager already inserted nil at position 1; self is at LLTIMERS_USERDATA (2)
        auto *lltimers = (lua_LLTimers *)lua_touserdatatagged(L, LLTIMERS_USERDATA, UTAG_LLTIMERS);
        if (!lltimers)
            luaL_typeerror(L, LLTIMERS_USERDATA, "LLTimers");

        lua_settop(L, LLTIMERS_USERDATA);

        // Check for reentrancy
        if (is_already_in_tick(L))
            luaL_errorL(L, "Recursive call to LLTimers:_tick() detected");

        // Reserve stack slots up to HANDLER_FUNC
        for (int i = lua_gettop(L) + 1; i <= HANDLER_FUNC; i++)
            lua_pushnil(L);

        // Get the live timers array and check if we have any timers
        lua_getref(L, lltimers->timers_tab_ref);
        int len = lua_objlen(L, -1);

        if (!len)
            return 0;

        // Clone the timer array for safe iteration
        lua_clonetable(L, -1);

        // Place both in their stack positions
        lua_replace(L, CLONED_TIMERS_ARRAY);
        lua_replace(L, LIVE_TIMERS_ARRAY);

        timers_len = len;
        LUAU_ASSERT(lua_gettop(L) == HANDLER_FUNC);
    }

    auto *sl_state = LUAU_GET_SL_VM_STATE(lua_mainthread(L));
    lua_checkstack(L, 10);

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(INTERRUPT_CHECK);
    YIELD_DISPATCH(CALL_HANDLER);
    YIELD_DISPATCH_END();

    for (; timer_index <= timers_len; ++timer_index)
    {
        // Check for interrupts between timers to prevent abuse
        YIELD_CHECK(L, INTERRUPT_CHECK, LUA_INTERRUPT_EVENTS);

        {
            lua_rawgeti(L, CLONED_TIMERS_ARRAY, timer_index);

            LUAU_ASSERT(lua_type(L, -1) == LUA_TTABLE);

            // Make sure this still exists in the live timers array
            int timer_idx_in_live_array = -1;
            int live_array_len = lua_objlen(L, LIVE_TIMERS_ARRAY);
            for (int i = 1; i <= live_array_len; i++)
            {
                lua_rawgeti(L, LIVE_TIMERS_ARRAY, i);
                if (lua_rawequal(L, -1, -2))
                {
                    timer_idx_in_live_array = i;
                    lua_replace(L, CURRENT_TIMER);
                    break;
                }
                lua_pop(L, 1);
            }

            if (timer_idx_in_live_array == -1)
            {
                // Timer was unscheduled, skip it
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_replace(L, CURRENT_TIMER);
                lua_pushnil(L);
                lua_replace(L, HANDLER_FUNC);
                LUAU_ASSERT(lua_gettop(L) == HANDLER_FUNC);
                continue;
            }

            // Get the nextRun time from the timer. We read this BEFORE updating
            // it so we can pass the pre-update value to the handler as its
            // "scheduled time" (the earliest instant this tick was allowed to
            // fire), letting handlers compute `now - scheduled_time` to see
            // how much extra delay there was beyond the minimum interval.
            lua_rawgeti(L, CURRENT_TIMER, TIMER_NEXT_RUN);
            double next_run = lua_tonumber(L, -1);
            lua_pop(L, 1);

            // Verify nextRun is a reasonable number
            LUAU_ASSERT(next_run >= 0.0);

            // Get current time (do this every loop because we might have yielded)
            double start_time = sl_state->clockProvider ? sl_state->clockProvider(L) : 0.0;

            // Verify start_time is reasonable
            LUAU_ASSERT(start_time >= 0.0);

            // Not time to run this yet, skip
            if (next_run > start_time)
            {
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_replace(L, CURRENT_TIMER);
                lua_pushnil(L);
                lua_replace(L, HANDLER_FUNC);
                LUAU_ASSERT(lua_gettop(L) == HANDLER_FUNC);
                continue;
            }

            LUAU_ASSERT(next_run <= start_time);

            // Check if this is a one-shot timer (interval is nil)
            lua_rawgeti(L, CURRENT_TIMER, TIMER_INTERVAL);
            bool is_one_shot = lua_isnil(L, -1);
            double interval = is_one_shot ? 0.0 : lua_tonumber(L, -1);
            lua_pop(L, 1);

            if (is_one_shot)
            {
                // One-shot timer, immediately unschedule it
                for (int j = timer_idx_in_live_array; j < live_array_len; j++)
                {
                    lua_rawgeti(L, LIVE_TIMERS_ARRAY, j + 1);
                    lua_rawseti(L, LIVE_TIMERS_ARRAY, j);
                }
                lua_pushnil(L);
                lua_rawseti(L, LIVE_TIMERS_ARRAY, live_array_len);
                LUAU_ASSERT(lua_objlen(L, LIVE_TIMERS_ARRAY) == live_array_len - 1);
            }
            else
            {
                // Schedule the next run. The default is cadence-preserving
                // absolute scheduling (previous_next_run + interval), so a
                // healthy 6s timer stays near 6s/12s/18s/... without drift.
                // But if that target is already in the past, meaning we
                // would need to fire again immediately to "catch up" on ticks
                // missed while the script was descheduled.
                //
                // Note: we compute this BEFORE invoking the handler so that
                // handler runtime has no effect on when it is next invoked.
                double new_next_run;
                if (interval == 0.0)
                {
                    // It should run again ASAP (but not immediately)!
                    new_next_run = std::nextafter(start_time, INFINITY);
                }
                else
                {
                    new_next_run = next_run + interval;
                    if (new_next_run <= start_time)
                    {
                        // Would need to catch up, don't. Reset cadence instead.
                        new_next_run = start_time + interval;
                        if (new_next_run <= start_time)
                        {
                            // Can happen with very small magnitude intervals, just
                            // schedule for the next representable time after start_time.
                            new_next_run = std::nextafter(start_time, INFINITY);
                        }
                    }
                }

                lua_pushnumber(L, new_next_run);
                lua_rawseti(L, CURRENT_TIMER, TIMER_NEXT_RUN);

                // TIMER_LOGICAL_SCHEDULE is a historical slot, we keep it here
                // so we don't muck up existing states.
                lua_pushnumber(L, new_next_run);
                lua_rawseti(L, CURRENT_TIMER, TIMER_LOGICAL_SCHEDULE);
            }

            // Get the handler from the timer and call it
            lua_pop(L, 1);
            lua_rawgeti(L, CURRENT_TIMER, TIMER_HANDLER);
            lua_replace(L, HANDLER_FUNC);

            LUAU_ASSERT(lua_iscallable(L, HANDLER_FUNC));
            LUAU_ASSERT(lua_gettop(L) == HANDLER_FUNC);

            // No pcall(), errors bubble up to the global error handler!
            lua_pushvalue(L, HANDLER_FUNC);
            // Include when it was scheduled to run as first arg, allowing callees to do a diff between
            // scheduled and actual time. We pass the pre-update next_run value, i.e. the earliest
            // instant this tick was allowed to fire (previous_fire + interval for repeating timers,
            // or create_time + interval for the first tick / a one-shot). `now - scheduled_time`
            // gives the extra delay beyond the minimum interval.
            lua_pushnumber(L, next_run);
            // Include the interval as second arg, enabling handlers to calculate missed intervals.
            // For once() timers, this will be nil. For on() timers, it's the interval.
            if (is_one_shot) {
                lua_pushnil(L);
            } else {
                lua_pushnumber(L, interval);
            }
        }
        YIELD_CALL(L, 2, 0, CALL_HANDLER);

        // Nil out the timer reference, we don't need it anymore
        lua_pushnil(L);
        lua_replace(L, CURRENT_TIMER);
        LUAU_ASSERT(lua_gettop(L) == HANDLER_FUNC);
    }


    // Done processing all timers, schedule the next tick
    auto *lltimers = (lua_LLTimers *)lua_touserdata(L, LLTIMERS_USERDATA);
    schedule_next_tick(L, lltimers);

    return 0;
}

static int lltimers_tostring(lua_State *L)
{
    auto *lltimers = (const lua_LLTimers *)lua_touserdatatagged(L, 1, UTAG_LLTIMERS);
    if (!lltimers)
        luaL_typeerror(L, 1, "LLTimers");

    lua_getref(L, lltimers->timers_tab_ref);
    int len = lua_objlen(L, -1);
    lua_pop(L, 1);

    lua_pushfstringL(L, "LLTimers{timers=%d}", len);
    return 1;
}

void luaSL_setup_llltimers_metatable(lua_State *L, int expose_internal_funcs)
{
    // Pre-create and fix system strings with memcat 0.
    // These strings are used by the host to manage timers.
    // If they're first created in user memcat context, looking them up
    // while at the memory limit will throw LUA_ERRMEM.
    {
        LUAU_MEMCAT_GUARD(0);
        luaS_fix(luaS_newliteral(L, LLTIMERS_GLOBAL_NAME));
        luaS_fix(luaS_newliteral(L, "LLTimers"));
    }

    // Set up destructor for LLTimers
    lua_setuserdatadtor(L, UTAG_LLTIMERS, lltimers_dtor);

    // Create metatable with all the metamethods
    lua_newtable(L);
    lua_pushvalue(L, -1);

    // Pin it to the refs
    lua_ref(L, -1);
    lua_setuserdatametatable(L, UTAG_LLTIMERS);

    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, lltimers_tostring, "__tostring");
    lua_setfield(L, -2, "__tostring");

    // Add methods
    lua_pushcfunction(L, lltimers_on, "every");
    lua_setfield(L, -2, "every");

    lua_pushcfunction(L, lltimers_once, "once");
    lua_setfield(L, -2, "once");

    lua_pushcfunction(L, lltimers_off, "off");
    lua_setfield(L, -2, "off");

    // Store _tick in registry for host and timer wrapper access
    lua_pushcclosurek(L, lltimers_tick_v0, "_tick", 0, lltimers_tick_v0_k);
    lua_setfield(L, LUA_REGISTRYINDEX, "LLTIMERS_TICK");

    if (expose_internal_funcs)
    {
        lua_pushcclosurek(L, lltimers_tick_v0, "_tick", 0, lltimers_tick_v0_k);
        lua_setfield(L, -2, "_tick");
    }

    // Give it a proper name for `typeof()`
    lua_pushstring(L, "LLTimers");
    lua_setfield(L, -2, "__type");

    // Please don't mess with our metatable!
    lua_setreadonly(L, -1, true);
    // Okay, don't need this on the stack anymore
    lua_pop(L, 1);
}
