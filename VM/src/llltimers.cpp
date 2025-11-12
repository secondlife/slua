#define llltimers_c

#include <cstring>
#include <cmath>
#include "lua.h"
#include "lcommon.h"
#include "lualib.h"
#include "llsl.h"
#include "llltimers.h"

#include "lobject.h"
#include "lstate.h"
#include "lapi.h"

// Timer data table array indices
enum TimerDataIndex {
    TIMER_HANDLER = 1,
    TIMER_INTERVAL = 2,
    TIMER_NEXT_RUN = 3,              // Actual time to fire (may be clamped for catch-up)
    TIMER_LOGICAL_SCHEDULE = 4,      // Logical schedule time (never clamped, always += interval)
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

    if (seconds < 0.0)
        luaL_errorL(L, "timer interval must be positive or 0");

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

    // Register timer wrapper with LLEvents when adding first timer
    if (old_len == 0)
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

    if (seconds < 0.0)
        luaL_errorL(L, "timer interval must be positive or 0");

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

    // Reschedule timer tick since we added a new timer
    schedule_next_tick(L, lltimers);

    lua_pop(L, 1); // Pop timers array

    // Register timer wrapper with LLEvents when adding first timer
    if (old_len == 0)
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

// Stack indices for yield-safe timer tick continuation
enum TickStack {
    LLTIMERS_USERDATA = 1,
    TIMER_INDEX = 2,
    TIMERS_LEN = 3,
    CLONED_TIMERS_ARRAY = 4,  // Shallow clone for safe iteration
    LIVE_TIMERS_ARRAY = 5,     // The actual timers array (can be modified)
    CURRENT_TIMER = 6,         // Current timer data table being processed
    HANDLER_FUNC = 7           // Handler function to call
};

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

static int lltimers_tick_cont(lua_State *L, int status);

// Check if we're already inside a _tick() call by walking the call stack
static bool is_already_in_tick(lua_State *L)
{
    // Walk up the call stack looking for lltimers_tick_cont
    // We start from L->ci - 1 because L->ci is the current (new) call to _tick
    for (CallInfo* ci = L->ci - 1; ci > L->base_ci; ci--)
    {
        // Get the function from this call frame
        if (!ttisfunction(ci->func))
            continue;

        Closure* cl = clvalue(ci->func);

        // Check if this is a C function with our continuation
        if (cl->isC && cl->c.cont == lltimers_tick_cont)
        {
            // Found _tick() higher in the call stack - we're reentrant!
            return true;
        }
    }

    return false;
}

static int lltimers_tick_init(lua_State *L)
{
    auto *lltimers = (lua_LLTimers *)lua_touserdatatagged(L, 1, UTAG_LLTIMERS);
    if (!lltimers)
        luaL_typeerror(L, 1, "LLTimers");

    lua_settop(L, 1);

    // Check for reentrancy
    if (is_already_in_tick(L))
    {
        luaL_errorL(L, "Recursive call to LLTimers:_tick() detected");
    }

    // This reserves all the stack slots we need for the continuation
    for (int i = 2; i <= HANDLER_FUNC; i++)
        lua_pushnil(L);

    // Get the live timers array and check if we have any timers
    lua_getref(L, lltimers->timers_tab_ref);
    int len = lua_objlen(L, -1);

    if (!len)
    {
        // No timers to run, go back to sleep
        return 0;
    }

    // Clone the timer array for safe iteration
    lua_clonetable(L, -1);

    // Place both in their stack positions
    lua_replace(L, CLONED_TIMERS_ARRAY);
    lua_replace(L, LIVE_TIMERS_ARRAY);

    // Set up iteration state
    lua_pushinteger(L, 1);
    lua_replace(L, TIMER_INDEX);

    lua_pushinteger(L, len);
    lua_replace(L, TIMERS_LEN);

    LUAU_ASSERT(lua_gettop(L) == HANDLER_FUNC);

    return lltimers_tick_cont(L, LUA_OK);
}

static int lltimers_tick_cont(lua_State *L, [[maybe_unused]]int status)
{
    auto *sl_state = LUAU_GET_SL_VM_STATE(lua_mainthread(L));

    int timer_index = lua_tointeger(L, TIMER_INDEX);
    int timers_len = lua_tointeger(L, TIMERS_LEN);

    // Process timers
    LUAU_ASSERT(timer_index >= 1);
    LUAU_ASSERT(timers_len >= 0);

    // Allocate stack space for loop iterations (needed after resume from yield)
    lua_checkstack(L, 10);

    void (*interrupt)(lua_State*, int) = L->global->cb.interrupt;

    while (timer_index <= timers_len)
    {
        lua_rawgeti(L, CLONED_TIMERS_ARRAY, timer_index); // Get timer from cloned array

        // Verify we got a table
        LUAU_ASSERT(lua_type(L, -1) == LUA_TTABLE);

        // Make sure this still exists in the live timers array before we try and run it
        int timer_idx_in_live_array = -1;
        int live_array_len = lua_objlen(L, LIVE_TIMERS_ARRAY);
        for (int i = 1; i <= live_array_len; i++)
        {
            lua_rawgeti(L, LIVE_TIMERS_ARRAY, i);
            if (lua_rawequal(L, -1, -2))
            {
                timer_idx_in_live_array = i;
                lua_replace(L, CURRENT_TIMER);  // Store timer in stack slot
                break;
            }
            lua_pop(L, 1);
        }

        if (timer_idx_in_live_array == -1)
        {
            // Timer was unscheduled, skip it
            // Pop the timer reference and nil out the timer and handler locals
            lua_pop(L, 1);
            lua_pushnil(L);
            lua_replace(L, CURRENT_TIMER);
            lua_pushnil(L);
            lua_replace(L, HANDLER_FUNC);
            LUAU_ASSERT(lua_gettop(L) == HANDLER_FUNC);
            timer_index++;
            continue;
        }

        // Get the nextRun time from the timer
        lua_rawgeti(L, CURRENT_TIMER, TIMER_NEXT_RUN);
        double next_run = lua_tonumber(L, -1);
        lua_pop(L, 1);

        // Get the logical schedule time (what we'll pass to the handler)
        // We read this BEFORE updating it so the handler gets the current value
        lua_rawgeti(L, CURRENT_TIMER, TIMER_LOGICAL_SCHEDULE);
        double logical_schedule = lua_tonumber(L, -1);
        lua_pop(L, 1);

        // Save the original value - this is what the handler should receive
        double handler_scheduled_time = logical_schedule;

        // Verify nextRun is a reasonable number
        LUAU_ASSERT(next_run >= 0.0);

        // Get current time (do this every loop because we might have yielded)
        double start_time = sl_state->clockProvider ? sl_state->clockProvider(L) : 0.0;

        // Verify start_time is reasonable
        LUAU_ASSERT(start_time >= 0.0);

        // Not time to run this yet, continue
        if (next_run > start_time)
        {
            // Timer was unscheduled, skip it
            // Pop the timer reference and nil out the timer and handler locals
            // TODO: utility function for this?
            lua_pop(L, 1);
            lua_pushnil(L);
            lua_replace(L, CURRENT_TIMER);
            lua_pushnil(L);
            lua_replace(L, HANDLER_FUNC);
            LUAU_ASSERT(lua_gettop(L) == HANDLER_FUNC);
            timer_index++;
            continue;
        }

        // If we reach here, timer should fire
        LUAU_ASSERT(next_run <= start_time);

        // Check if this is a one-shot timer (interval is nil)
        lua_rawgeti(L, CURRENT_TIMER, TIMER_INTERVAL);
        bool is_one_shot = lua_isnil(L, -1);
        double interval = is_one_shot ? 0.0 : lua_tonumber(L, -1);
        lua_pop(L, 1);

        if (is_one_shot)
        {
            // TODO: Utility function for removal cases?
            // One-shot timer, immediately unschedule it
            // Shift elements down in the live timers array
            for (int j = timer_idx_in_live_array; j < live_array_len; j++)
            {
                lua_rawgeti(L, LIVE_TIMERS_ARRAY, j + 1);
                lua_rawseti(L, LIVE_TIMERS_ARRAY, j);
            }
            lua_pushnil(L);
            lua_rawseti(L, LIVE_TIMERS_ARRAY, live_array_len);

            // Verify the array shrunk by one
            LUAU_ASSERT(lua_objlen(L, LIVE_TIMERS_ARRAY) == live_array_len - 1);
        }
        else
        {
            // Schedule its next run using absolute scheduling with clamped catch-up
            // (next = previous_scheduled_time + interval)
            // This prevents drift and ensures the timer maintains its rhythm.
            // However, if the timer is very late (> 2 seconds), skip ahead to prevent
            // excessive catch-up iterations that could bog down the system.
            //
            // We maintain two schedules:
            // - TIMER_NEXT_RUN: May be clamped to prevent catch-up storms
            // - TIMER_LOGICAL_SCHEDULE: Never clamped, always += interval
            //   This lets handlers know their true delay from the logical schedule.
            //
            // Note that we do this BEFORE the timer is ever run.
            // This ensures that handler runtime has no effect on
            // when the handler will be invoked next.
            const double MAX_CATCHUP_TIME = 2.0;
            double next_scheduled = next_run + interval;
            double new_next_run;
            bool did_clamp = false;

            // Check if the next scheduled time would still be >2s behind current time
            if (start_time - next_scheduled > MAX_CATCHUP_TIME)
            {
                // Skip ahead to next interval after current time
                // This prevents spiral of death from excessive catch-up
                double time_behind = start_time - next_run;
                double intervals_to_skip = ceil(time_behind / interval);
                new_next_run = next_run + (intervals_to_skip * interval);
                did_clamp = true;
            }
            else
            {
                // Normal absolute scheduling - maintain rhythm
                new_next_run = next_scheduled;
            }

            // Update actual next run time (may be clamped)
            lua_pushnumber(L, new_next_run);
            lua_rawseti(L, CURRENT_TIMER, TIMER_NEXT_RUN);

            // Update logical schedule
            // When clamping, sync to new_next_run (reset to new reality)
            // When not clamping, increment normally (logical_schedule + interval)
            if (did_clamp)
            {
                // Sync logical schedule when clamping - we're giving up on catch-up
                // so reset the logical schedule to match the new reality.
                // This ensures handlers see the initial delay (current fire), then return to normal.
                lua_pushnumber(L, new_next_run);
            }
            else
            {
                // Normal increment - maintain rhythm
                lua_pushnumber(L, logical_schedule + interval);
            }
            lua_rawseti(L, CURRENT_TIMER, TIMER_LOGICAL_SCHEDULE);
        }

        // Get the handler from the timer and call it
        lua_pop(L, 1); // Pop timer reference from cloned array
        lua_rawgeti(L, CURRENT_TIMER, TIMER_HANDLER); // Get handler
        lua_replace(L, HANDLER_FUNC); // Store handler in its own slot

        // Verify we got a callable
        LUAU_ASSERT(lua_iscallable(L, HANDLER_FUNC));

        // Update timer_index for continuation
        lua_pushinteger(L, timer_index + 1);
        lua_replace(L, TIMER_INDEX);

        LUAU_ASSERT(lua_gettop(L) == HANDLER_FUNC);

        // No pcall(), errors bubble up to the global error handler!
        lua_pushvalue(L, HANDLER_FUNC);
        // Include when it was scheduled to run as first arg, allowing callees to do a diff between
        // scheduled and actual time.
        // We pass the saved handler_scheduled_time (the original logical schedule) so handlers
        // can detect delays. When clamping occurs, the handler still receives the ORIGINAL
        // scheduled time (when it was supposed to run), not the synced time.
        lua_pushnumber(L, handler_scheduled_time);
        // Include the interval as second arg, enabling handlers to calculate missed intervals.
        // For once() timers, this will be nil. For on() timers, it's the interval.
        if (is_one_shot) {
            lua_pushnil(L);
        } else {
            lua_pushnumber(L, interval);
        }
        lua_call(L, 2, 0);

        if (L->status == LUA_YIELD || L->status == LUA_BREAK)
        {
            return -1;
        }

        // We can nil this out, we don't need a reference anymore.
        lua_pushnil(L);
        lua_replace(L, CURRENT_TIMER);
        LUAU_ASSERT(lua_gettop(L) == HANDLER_FUNC);

        // Check for interrupts between timers to prevent abuse
        if (LUAU_LIKELY(!!interrupt))
        {
            interrupt(L, -2);  // -2 indicates "handler interrupt check"
            if (L->status != LUA_OK)
                return -1;
        }

        timer_index++;
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
    lua_pushcfunction(L, lltimers_on, "on");
    luau_dupcclosure(L, -1, "every");
    // Either :every() or :on() is fine
    lua_setfield(L, -3, "every");
    lua_setfield(L, -2, "on");

    lua_pushcfunction(L, lltimers_once, "once");
    lua_setfield(L, -2, "once");

    lua_pushcfunction(L, lltimers_off, "off");
    lua_setfield(L, -2, "off");

    // Store _tick in registry for host and timer wrapper access
    lua_pushcclosurek(L, lltimers_tick_init, "_tick", 0, lltimers_tick_cont);
    lua_setfield(L, LUA_REGISTRYINDEX, "LLTIMERS_TICK");

    if (expose_internal_funcs)
    {
        lua_pushcclosurek(L, lltimers_tick_init, "_tick", 0, lltimers_tick_cont);
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
