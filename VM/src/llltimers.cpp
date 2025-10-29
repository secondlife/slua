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
    TIMER_NEXT_RUN = 3,
    TIMER_LEN = TIMER_NEXT_RUN,
};

static void lltimers_dtor(lua_State *L, void *data)
{
    auto *lltimers = (lua_LLTimers *)data;
    if (lltimers->timers_tab_ref != -1)
    {
        lua_unref(L, lltimers->timers_tab_ref);
        lltimers->timers_tab_ref = -1;
    }
}

int luaSL_createtimermanager(lua_State *L)
{
    lua_checkstack(L, 2);
    auto *lltimers = (lua_LLTimers *)lua_newuserdatataggedwithmetatable(L, sizeof(lua_LLTimers), UTAG_LLTIMERS);

    // Create empty timers array
    lua_createtable(L, 0, 0);
    lltimers->timers_tab_ref = lua_ref(L, -1);
    lltimers->timers_tab = hvalue(luaA_toobject(L, -1));
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
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_settop(L, 3);

    if (seconds <= 0.0)
        luaL_errorL(L, "timer interval must be positive");

    // Get current time
    double current_time = sl_state->clockProvider ? sl_state->clockProvider(L) : 0.0;

    // Get the timers array
    lua_getref(L, lltimers->timers_tab_ref);

    // Create timer data table
    lua_createtable(L, 3, 0);
    lua_pushvalue(L, 3);
    lua_rawseti(L, -2, TIMER_HANDLER);
    lua_pushnumber(L, current_time + seconds);
    lua_rawseti(L, -2, TIMER_NEXT_RUN);
    lua_pushnumber(L, seconds);
    lua_rawseti(L, -2, TIMER_INTERVAL);

    LUAU_ASSERT(lua_objlen(L, -1) == TIMER_LEN);

    // Add to the end of the timers array
    lua_rawseti(L, -2, lua_objlen(L, -2) + 1);

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
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_settop(L, 3);

    if (seconds <= 0.0)
        luaL_errorL(L, "timer interval must be positive");

    // Get current time
    double current_time = sl_state->clockProvider ? sl_state->clockProvider(L) : 0.0;

    // Get the timers array
    lua_getref(L, lltimers->timers_tab_ref);

    // Create timer data table
    lua_createtable(L, 3, 0);
    lua_pushvalue(L, 3);
    lua_rawseti(L, -2, TIMER_HANDLER);
    lua_pushnil(L);
    lua_rawseti(L, -2, TIMER_INTERVAL);
    lua_pushnumber(L, current_time + seconds);
    lua_rawseti(L, -2, TIMER_NEXT_RUN);
    LUAU_ASSERT(lua_objlen(L, -1) == TIMER_LEN);

    // Add to the timers array
    lua_rawseti(L, -2, lua_objlen(L, -2) + 1);
    lua_pop(L, 1); // Pop timers array

    // Return the passed-in handler so it can be unregistered later
    lua_pushvalue(L, 3);
    return 1;
}

static int lltimers_off(lua_State *L)
{
    auto *lltimers = (lua_LLTimers *)lua_touserdatatagged(L, 1, UTAG_LLTIMERS);
    if (!lltimers)
        luaL_typeerror(L, 1, "LLTimers");

    luaL_checktype(L, 2, LUA_TFUNCTION);
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

// Helper function to schedule the next timer tick
// Expects timers_tab to be on the stack at the given index
static void schedule_next_tick(lua_State *L)
{
    auto *sl_state = LUAU_GET_SL_VM_STATE(lua_mainthread(L));

    if (!sl_state->setTimerEventCb)
        return; // No callback set, can't schedule

    int len = lua_objlen(L, LIVE_TIMERS_ARRAY);

    if (len == 0)
    {
        // No timers pending, unsubscribe from the parent timer event
        sl_state->setTimerEventCb(L, 0.0);
        return;
    }

    // Figure out when we next need to wake up
    double min_time = HUGE_VAL;
    for (int i = 1; i <= len; i++)
    {
        lua_rawgeti(L, LIVE_TIMERS_ARRAY, i);
        lua_rawgeti(L, -1, TIMER_NEXT_RUN);
        double next_run = lua_tonumber(L, -1);
        lua_pop(L, 2); // Pop nextRun and timer data
        min_time = fmin(min_time, next_run);
    }

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

static int lltimers_tick_init(lua_State *L)
{
    auto *lltimers = (lua_LLTimers *)lua_touserdatatagged(L, 1, UTAG_LLTIMERS);
    if (!lltimers)
        luaL_typeerror(L, 1, "LLTimers");

    lua_settop(L, 1);

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
            // Schedule its next run
            // Note that we do this BEFORE the timer is ever run.
            // This ensures that handler runtime has no effect on
            // When the handler will be invoked next.
            lua_pushnumber(L, start_time + interval);
            lua_rawseti(L, CURRENT_TIMER, TIMER_NEXT_RUN);
        }

        // Get the handler from the timer and call it
        lua_pop(L, 1); // Pop timer reference from cloned array
        lua_rawgeti(L, CURRENT_TIMER, TIMER_HANDLER); // Get handler
        lua_replace(L, HANDLER_FUNC); // Store handler in its own slot

        // Verify we got a function
        LUAU_ASSERT(lua_type(L, HANDLER_FUNC) == LUA_TFUNCTION);

        // Update timer_index for continuation
        lua_pushinteger(L, timer_index + 1);
        lua_replace(L, TIMER_INDEX);

        LUAU_ASSERT(lua_gettop(L) == HANDLER_FUNC);

        // No pcall(), errors bubble up to the global error handler!
        lua_pushvalue(L, HANDLER_FUNC);
        lua_call(L, 0, 0);

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
    schedule_next_tick(L);

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

void luaSL_setup_llltimers_metatable(lua_State *L)
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
    lua_setfield(L, -2, "on");

    lua_pushcfunction(L, lltimers_once, "once");
    lua_setfield(L, -2, "once");

    lua_pushcfunction(L, lltimers_off, "off");
    lua_setfield(L, -2, "off");

    lua_pushcclosurek(L, lltimers_tick_init, "_tick", 0, lltimers_tick_cont);
    lua_setfield(L, -2, "_tick");

    // Give it a proper name for `typeof()`
    lua_pushstring(L, "LLTimers");
    lua_setfield(L, -2, "__type");

    // Please don't mess with our metatable!
    lua_setreadonly(L, -1, true);
    // Okay, don't need this on the stack anymore
    lua_pop(L, 1);
}
