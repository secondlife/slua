#define lllevents_c

#include <cstring>
#include "lua.h"
#include "lcommon.h"
#include "lualib.h"
#include "llsl.h"
#include "lllevents.h"
#include "llltimers.h"
#include "lyieldablemacros.h"

#include "lobject.h"
#include "lstate.h"
#include "lgc.h"
#include "lapi.h"
#include "lstring.h"

// Guard function to prevent direct calls to internal timer wrapper
int timer_wrapper_guard(lua_State *L)
{
    luaL_errorL(L, "Cannot call internal wrapper directly");
    return 0;
}

int luaSL_pushdetectedevent(lua_State *L, int index, bool valid, bool can_change_damage)
{
    lua_checkstack(L, 1);
    auto *detected_event = (lua_DetectedEvent *)lua_newuserdatataggedwithmetatable(L, sizeof(lua_DetectedEvent), UTAG_DETECTED_EVENT);
    detected_event->index = index;
    detected_event->valid = valid;
    detected_event->can_change_damage = can_change_damage;
    return 1;
}

static int detected_event_index(lua_State *L)
{
    auto *detected_event = (const lua_DetectedEvent *)lua_touserdatatagged(L, 1, UTAG_DETECTED_EVENT);
    if (!detected_event)
        luaL_typeerror(L, 1, "DetectedEvent");

    const char *field = luaL_checkstring(L, 2);

    // Handle field access for index, valid, canAdjustDamage
    if (strcmp(field, "index") == 0)
    {
        lua_pushinteger(L, detected_event->index);
        return 1;
    }
    else if (strcmp(field, "valid") == 0)
    {
        lua_pushboolean(L, detected_event->valid);
        return 1;
    }
    else if (strcmp(field, "canAdjustDamage") == 0)
    {
        lua_pushboolean(L, detected_event->can_change_damage);
        return 1;
    }

    // Fall back to metatable for method dispatch
    lua_getuserdatametatable(L, UTAG_DETECTED_EVENT);
    lua_rawgetfield(L, -1, field);
    return 1;
}

static int detected_event_call_ll_function(lua_State *L, const char *function_name)
{
    auto *detected_event = (const lua_DetectedEvent *)lua_touserdatatagged(L, 1, UTAG_DETECTED_EVENT);
    if (!detected_event)
        luaL_typeerror(L, 1, "DetectedEvent");

    if (!detected_event->valid)
        luaL_errorL(L, "DetectedEvent is no longer valid");

    lua_rawgetfield(L, LUA_BASEGLOBALSINDEX, "ll");
    lua_rawgetfield(L, -1, function_name);
    lua_pushinteger(L, detected_event->index);
    lua_call(L, 1, 1);
    return 1;
}

static int detected_event_adjust_damage(lua_State *L)
{
    auto *detected_event = (const lua_DetectedEvent *)lua_touserdatatagged(L, 1, UTAG_DETECTED_EVENT);
    if (!detected_event)
        luaL_typeerror(L, 1, "DetectedEvent");

    if (!detected_event->valid)
        luaL_errorL(L, "DetectedEvent is no longer valid");

    if (!detected_event->can_change_damage)
        luaL_errorL(L, "Cannot adjust damage for this DetectedEvent");

    double damage = luaL_checknumber(L, 2);

    lua_rawgetfield(L, LUA_BASEGLOBALSINDEX, "ll");
    lua_rawgetfield(L, -1, "AdjustDamage");
    lua_pushinteger(L, detected_event->index);
    lua_pushnumber(L, damage);
    lua_call(L, 2, 0);
    return 0;
}

static int detected_event_tostring(lua_State *L)
{
    auto *detected_event = (const lua_DetectedEvent *)lua_touserdatatagged(L, 1, UTAG_DETECTED_EVENT);
    if (!detected_event)
        luaL_typeerror(L, 1, "DetectedEvent");

    lua_pushfstringL(L, "DetectedEvent{index=%d, valid=%s, canAdjustDamage=%s}",
                     detected_event->index,
                     detected_event->valid ? "true" : "false",
                     detected_event->can_change_damage ? "true" : "false");
    return 1;
}

void luaSL_setup_detectedevent_metatable(lua_State *L)
{
    // create metatable with all the metamethods
    lua_newtable(L);
    lua_pushvalue(L, -1);

    // pin it to the refs
    lua_ref(L, -1);
    lua_setuserdatametatable(L, UTAG_DETECTED_EVENT);

    lua_pushcfunction(L, detected_event_index, "__index");
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, detected_event_tostring, "__tostring");
    lua_setfield(L, -2, "__tostring");

    // Helper macro for adding getter methods that call ll.Detected* functions
#define ADD_DETECTED_GETTER(lua_name, ll_func_name) \
    do { \
        lua_pushcfunction(L, [](lua_State *L) -> int { \
            return detected_event_call_ll_function(L, (ll_func_name)); \
        }, lua_name); \
        lua_setfield(L, -2, (lua_name)); \
    } while(0)

    // Add the methods to the metatable
    ADD_DETECTED_GETTER("getOwner", "DetectedOwner");
    ADD_DETECTED_GETTER("getName", "DetectedName");
    ADD_DETECTED_GETTER("getKey", "DetectedKey");
    ADD_DETECTED_GETTER("getTouchFace", "DetectedTouchFace");
    ADD_DETECTED_GETTER("getLinkNumber", "DetectedLinkNumber");
    ADD_DETECTED_GETTER("getGrab", "DetectedGrab");
    ADD_DETECTED_GETTER("getGroup", "DetectedGroup");
    ADD_DETECTED_GETTER("getPos", "DetectedPos");
    ADD_DETECTED_GETTER("getRot", "DetectedRot");
    ADD_DETECTED_GETTER("getTouchBinormal", "DetectedTouchBinormal");
    ADD_DETECTED_GETTER("getTouchNormal", "DetectedTouchNormal");
    ADD_DETECTED_GETTER("getTouchPos", "DetectedTouchPos");
    ADD_DETECTED_GETTER("getTouchST", "DetectedTouchST");
    ADD_DETECTED_GETTER("getTouchUV", "DetectedTouchUV");
    ADD_DETECTED_GETTER("getType", "DetectedType");
    ADD_DETECTED_GETTER("getVel", "DetectedVel");
    ADD_DETECTED_GETTER("getRezzer", "DetectedRezzer");
    ADD_DETECTED_GETTER("getDamage", "DetectedDamage");

#undef ADD_DETECTED_GETTER

    lua_pushcfunction(L, detected_event_adjust_damage, "adjustDamage");
    lua_setfield(L, -2, "adjustDamage");

    // give it a proper name for `typeof()`
    lua_pushstring(L, "DetectedEvent");
    lua_setfield(L, -2, "__type");

    // Please don't mess with our metatable!
    lua_setreadonly(L, -1, true);
    // Okay, don't need this on the stack anymore
    lua_pop(L, 1);
}


static void llevents_dtor(lua_State *L, void *data)
{
    lua_LLEvents *llevents = (lua_LLEvents *)data;
    if (llevents->handlers_tab_ref != -1)
    {
        lua_unref(L, llevents->handlers_tab_ref);
        llevents->handlers_tab_ref = -1;
    }
}

int luaSL_createeventmanager(lua_State *L)
{
    lua_checkstack(L, 2);
    auto *llevents = (lua_LLEvents *)lua_newuserdatataggedwithmetatable(L, sizeof(lua_LLEvents), UTAG_LLEVENTS);
    llevents->handlers_tab_ref = -1;  // Initialize before allocations

    // Create empty handlers table
    lua_createtable(L, 0, 0);
    llevents->handlers_tab_ref = lua_ref(L, -1);
    llevents->handlers_tab = hvalue(luaA_toobject(L, -1));
    // event name -> {event handler functions}
    lua_pop(L, 1);

    return 1;
}

static int llevents_index(lua_State *L)
{
    const lua_LLEvents *llevents = (const lua_LLEvents *)lua_touserdatatagged(L, 1, UTAG_LLEVENTS);
    if (!llevents)
        luaL_typeerror(L, 1, "LLEvents");

    const char *field = luaL_checkstring(L, 2);

    // Fall back to metatable for method dispatch
    lua_getuserdatametatable(L, UTAG_LLEVENTS);
    lua_rawgetfield(L, -1, field);
    return 1;
}

static int llevents_newindex(lua_State *L)
{
    const lua_LLEvents *llevents = (const lua_LLEvents *)lua_touserdatatagged(L, 1, UTAG_LLEVENTS);
    if (!llevents)
        luaL_typeerror(L, 1, "LLEvents");

    luaL_checktype(L, 2, LUA_TSTRING);
    if (!lua_iscallable(L, 3))
        luaL_typeerror(L, 3, "function or callable table");

    // Check if handler was defined with : syntax
    if (luaSL_ismethodstyle(L, 3))
        luaL_errorL(L, "Event handler defined with ':' syntax; use '.' instead");

    // Setting a function - treat as event handler registration
    // Call LLEvents:on(event_name, handler)
    lua_getuserdatametatable(L, UTAG_LLEVENTS);
    lua_rawgetfield(L, -1, "on");
    lua_pushvalue(L, 1); // self
    lua_pushvalue(L, 2); // event_name
    lua_pushvalue(L, 3); // handler
    lua_call(L, 3, 0);

    return 0;
}

static int llevents_on(lua_State *L)
{
    auto *llevents = (const lua_LLEvents *)lua_touserdatatagged(L, 1, UTAG_LLEVENTS);
    if (!llevents)
        luaL_typeerror(L, 1, "LLEvents");

    const char *event_name = luaL_checkstring(L, 2);
    if (!lua_iscallable(L, 3))
        luaL_typeerror(L, 3, "function or callable table");
    lua_settop(L, 3);

    // Get the handlers table
    lua_getref(L, llevents->handlers_tab_ref);

    // Get or create the handlers array for this event
    lua_rawgetfield(L, -1, event_name);
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        // Okay, this is something that didn't have handlers before
        auto *sl_state = LUAU_GET_SL_VM_STATE(lua_mainthread(L));
        if (sl_state->eventHandlerRegistrationCb != nullptr && !sl_state->eventHandlerRegistrationCb(L, event_name, true))
        {
            // Event handler check failed, let the user know
            luaL_errorL(L, "'%s' is not a supported event name", event_name);
        }

        // Create the handlers array and add the handler to it.
        lua_createtable(L, 1, 0);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, event_name);
    }

    // Wrap the handler in a single-element table for unique identity
    // This makes ensures that we can (somewhat sanely) detect if a
    // handler is removed during `_handleEvent()` without weird edge cases.
    lua_createtable(L, 1, 0);
    lua_pushvalue(L, 3); // handler function
    lua_rawseti(L, -2, 1); // wrapper[1] = handler
    lua_setreadonly(L, -1, true); // Readonly since we never expect mutation.

    // Add the wrapper to the end of the array
    lua_rawseti(L, -2, lua_objlen(L, -2) + 1);
    luau_shrinktable(L, -1, 0);

    // Return the unwrapped handler so it can be unregistered later
    lua_pushvalue(L, 3);
    return 1;
}

static int llevents_off(lua_State *L)
{
    auto *sl_state = LUAU_GET_SL_VM_STATE(lua_mainthread(L));

    auto *llevents = (const lua_LLEvents *)lua_touserdatatagged(L, 1, UTAG_LLEVENTS);
    if (!llevents)
        luaL_typeerror(L, 1, "LLEvents");

    const char *event_name = luaL_checkstring(L, 2);
    if (!lua_iscallable(L, 3))
        luaL_typeerror(L, 3, "function or callable table");
    lua_settop(L, 3);


    // Get the handlers table
    lua_getref(L, llevents->handlers_tab_ref);
    lua_rawgetfield(L, -1, event_name);

    if (lua_isnil(L, -1))
    {
        // Okay, this is something that didn't have handlers before.
        if (sl_state->eventHandlerRegistrationCb != nullptr && !sl_state->eventHandlerRegistrationCb(L, event_name, false))
        {
            // Event handler check failed, let the user know
            luaL_errorL(L, "'%s' is not a supported event name", event_name);
        }
        lua_pushboolean(L, false);
        return 1;
    }

    // Find and remove the wrapper containing the handler
    // This might seem jank, but it's basically a merged `table.find()` and `table.remove()`.
    int len = lua_objlen(L, -1);
    bool found = false;

    for (int i = len; i >= 1; i--)
    {
        lua_rawgeti(L, -1, i); // Get wrapper table
        // Get the actual handler from wrapper[1]
        lua_rawgeti(L, -1, 1);
        if (lua_rawequal(L, -1, 3))
        {
            // pop the unwrapped handler and wrapper, we don't need them.
            lua_pop(L, 2);
            // Remove from array by shifting elements down
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
        // pop the unwrapped handler and wrapper.
        lua_pop(L, 2);
    }

    // We found and removed something
    if (found)
    {
        if (lua_objlen(L, -1) == 0)
        {
            // Clear out the empty handler array if we have no more handlers for this event type
            lua_pushnil(L);
            lua_setfield(L, -3, event_name);

            // Let the host know that we have no more handlers for this event, if it cares.
            // We don't need to check the result, we're not asking, we're telling the host
            // that there are no more handlers of this type.
            if (sl_state->eventHandlerRegistrationCb != nullptr)
            {
                sl_state->eventHandlerRegistrationCb(L, event_name, false);
            }
        }
        else
        {
            // Just shrink the table to its new size.
            luau_shrinktable(L, -1, 0);
        }
    }

    lua_pushboolean(L, found);
    return 1;
}


enum OnceWrapperUpvalue {
    ONCE_SELF = 1,
    ONCE_EVENT_NAME = 2,
    ONCE_HANDLER = 3
};

static int llevents_once_wrapper(lua_State *L)
{
    // Remove the wrapper via `LLEvents:off()` before calling the handler
    lua_getuserdatametatable(L, UTAG_LLEVENTS);
    lua_rawgetfield(L, -1, "off");
    lua_pushvalue(L, lua_upvalueindex(ONCE_SELF));
    lua_pushvalue(L, lua_upvalueindex(ONCE_EVENT_NAME));
    luaA_pushobject(L, L->ci->func); // Push currently executing function (this wrapper)
    lua_call(L, 3, 0);

    // Get rid of things we don't need on the stack, because we're about to call the real event handler.
    lua_pop(L, 1);

    // Put the actual handler before the args that are still on the stack
    lua_pushvalue(L, lua_upvalueindex(ONCE_HANDLER));
    lua_insert(L, 1);

    lua_call(L, lua_gettop(L) - 1, 0);
    if (L->status == LUA_YIELD || L->status == LUA_BREAK)
        return -1;

    return 0;
}

static int llevents_once_wrapper_cont(lua_State *L, int status)
{
    // We don't do anything in the continuation. When control is returned to us
    // from the event handler below us, we just let the caller know there are 0 retvals.
    return 0;
}

static int llevents_once(lua_State *L)
{
    auto *llevents = (const lua_LLEvents *)lua_touserdatatagged(L, 1, UTAG_LLEVENTS);
    if (!llevents)
        luaL_typeerror(L, 1, "LLEvents");

    luaL_checkstring(L, 2);
    if (!lua_iscallable(L, 3))
        luaL_typeerror(L, 3, "function or callable table");
    lua_settop(L, 3);

    // Create wrapper function with upvalues
    lua_pushvalue(L, 1); // self
    lua_pushvalue(L, 2); // event_name
    lua_pushvalue(L, 3); // handler
    lua_pushcclosurek(L, llevents_once_wrapper, "once_wrapper", 3, llevents_once_wrapper_cont);

    // Register the wrapper using :on()
    lua_getuserdatametatable(L, UTAG_LLEVENTS);
    lua_rawgetfield(L, -1, "on");
    lua_pushvalue(L, 1); // self
    lua_pushvalue(L, 2); // event_name
    lua_pushvalue(L, 4); // wrapper
    lua_call(L, 3, 1);

    return 1;
}

static int llevents_handlers(lua_State *L)
{
    auto *llevents = (const lua_LLEvents *)lua_touserdatatagged(L, 1, UTAG_LLEVENTS);
    if (!llevents)
        luaL_typeerror(L, 1, "LLEvents");

    const char *event_name = luaL_checkstring(L, 2);
    lua_settop(L, 2);

    // Get the handlers table
    lua_getref(L, llevents->handlers_tab_ref);
    lua_rawgetfield(L, -1, event_name);

    if (lua_isnil(L, -1))
    {
        lua_createtable(L, 0, 0);
        return 1;
    }

    // Return a clone of the handlers array with unwrapped handlers
    lua_clonetable(L, -1);
    int len = lua_objlen(L, -1);

    // Unwrap each handler
    for (int i = 1; i <= len; i++)
    {
        lua_rawgeti(L, -1, i); // Get wrapper table
        lua_rawgeti(L, -1, 1); // Get wrapper[1] (the actual handler)

        // Check if this is the internal timer wrapper - replace with guard
        // We _really_ don't want people fiddling with this handler, or even calling
        // it manually. This only serves as an indication to you that _something_ is
        // registered to the `timer` event, but you can't unregister it :)
        const TValue* handler = luaA_toobject(L, -1);
        if (iscfunction(handler) && clvalue(handler)->c.f == timer_event_wrapper)
        {
            // Replace with guard function from registry
            lua_pop(L, 1);
            lua_getfield(L, LUA_REGISTRYINDEX, "LLEVENTS_TIMER_WRAPPER_GUARD");
        }

        // Set it back in the cloned array
        lua_rawseti(L, -3, i);
        lua_pop(L, 1);
    }

    return 1;
}

static int llevents_eventnames(lua_State *L)
{
    const lua_LLEvents *llevents = (const lua_LLEvents *)lua_touserdatatagged(L, 1, UTAG_LLEVENTS);
    if (!llevents)
        luaL_typeerror(L, 1, "LLEvents");

    lua_settop(L, 1);

    // Get the handlers table
    lua_getref(L, llevents->handlers_tab_ref);

    lua_createtable(L, 0, 0);
    int index = 1;

    lua_pushnil(L);
    while (lua_next(L, -3) != 0)
    {
        if (lua_type(L, -2) == LUA_TSTRING)
        {
            lua_pushvalue(L, -2); // key (event name)
            lua_rawseti(L, -4, index++);
        }
        lua_pop(L, 1); // pop value, keep key for next iteration
    }

    return 1;
}

// Multi-event types that need DetectedEvent wrappers
static const std::unordered_set<std::string> MULTI_EVENT_NAMES = {
    "touch_start",
    "touch_end",
    "touch",
    "collision_start",
    "collision",
    "collision_end",
    "sensor",
    "on_damage",
    "final_damage"
};

// Stack indices for yield-safe event handler
enum EventHandlerStack {
    HANDLERS_TABLE = 2,
    ORIGINAL_HANDLERS_TABLE = 3,
    ARG_START = 4,
};

static bool is_multi_event(const char* event_name)
{
    return MULTI_EVENT_NAMES.find(event_name) != MULTI_EVENT_NAMES.end();
}

DEFINE_YIELDABLE(llevents_handle_event, 0)
{
    YIELDABLE_RETURNS_DEFAULT;

    enum class Phase : uint8_t {
        DEFAULT = 0,
        INTERRUPT_CHECK = 1,
        CALL_HANDLER = 2,
    };

    SlotManager slots(L, is_init);

    DEFINE_SLOT(Phase, phase, Phase::DEFAULT);
    DEFINE_SLOT(int32_t, handler_index, 1);
    DEFINE_SLOT(int32_t, handlers_len, 0);
    DEFINE_SLOT(int32_t, nargs, 0);
    DEFINE_SLOT(bool, is_multi, false);
    slots.finalize();

    if (is_init)
    {
        auto *llevents = (const lua_LLEvents *)lua_touserdatatagged(L, 2, UTAG_LLEVENTS);
        if (!llevents)
            luaL_typeerror(L, 2, "LLEvents");

        luaL_checktype(L, 3, LUA_TSTRING);

        const char *event_name = luaL_checkstring(L, 3);
        nargs = lua_gettop(L) - ARG_START + 1;
        is_multi = is_multi_event(event_name);
        bool can_adjust_damage = is_multi && (strcmp(event_name, "on_damage") == 0);

        lua_getref(L, llevents->handlers_tab_ref);
        lua_rawgetfield(L, -1, event_name);

        // We have no handlers for this event.
        if (lua_isnil(L, -1))
            return 0;

        int len = lua_objlen(L, -1);

        // Empty array isn't valid, val should be nil if no handlers for an event.
        LUAU_ASSERT(len > 0);
        handlers_len = len;

        // Place original handlers table at position 3 (replacing event_name arg)
        lua_replace(L, ORIGINAL_HANDLERS_TABLE);
        // Clone from its new home, place at position 2 (replacing llevents arg)
        lua_clonetable(L, ORIGINAL_HANDLERS_TABLE);
        lua_replace(L, HANDLERS_TABLE);
        // Pop leftover handlers_tab
        lua_pop(L, 1);

        if (is_multi)
        {
            int num_detected = luaL_checkinteger(L, ARG_START);

            lua_createtable(L, num_detected, 0);

            for (int i = 1; i <= num_detected; i++)
            {
                luaSL_pushdetectedevent(L, i, true, can_adjust_damage);
                lua_rawseti(L, -2, i);
            }

            lua_setreadonly(L, -1, true);
            // Set the first arg to the table of detected event wrappers
            lua_replace(L, ARG_START);
        }
    }

    lua_checkstack(L, lua_gettop(L) * 3);

    YIELD_DISPATCH_BEGIN(phase, slots);
    YIELD_DISPATCH(INTERRUPT_CHECK);
    YIELD_DISPATCH(CALL_HANDLER);
    YIELD_DISPATCH_END();

    for (; handler_index <= handlers_len; ++handler_index)
    {
        // Check for interrupts between handlers to prevent abuse
        YIELD_CHECK(L, INTERRUPT_CHECK, LUA_INTERRUPT_EVENTS);

        // Enclose this in an anonymous scope so lifetimes of temporaries are bounded
        {
            // Get the (table-wrapped) function to call from the handlers table
            lua_rawgeti(L, HANDLERS_TABLE, handler_index);
            LUAU_ASSERT(lua_type(L, -1) == LUA_TTABLE);

            // Check if this wrapper still exists in the original handlers table
            bool found = false;
            int original_len = lua_objlen(L, ORIGINAL_HANDLERS_TABLE);
            for (int i = 1; i <= original_len; i++)
            {
                lua_rawgeti(L, ORIGINAL_HANDLERS_TABLE, i);
                if (lua_rawequal(L, -1, -2))
                {
                    found = true;
                    lua_pop(L, 1);
                    break;
                }
                lua_pop(L, 1);
            }

            // Not registered anymore? One of the intermediary handlers
            // may have unregistered it. Try the next one.
            if (!found)
            {
                lua_pop(L, 1);
                continue;
            }

            // Unwrap to get the actual handler function
            lua_rawgeti(L, -1, 1);
            lua_remove(L, -2);
            LUAU_ASSERT(lua_iscallable(L, -1));

            // Push arguments for this handler call
            for (int j = 0; j < nargs; j++)
                lua_pushvalue(L, ARG_START + j);
        }

        // handler function and arguments are in order on the stack, do the call.
        YIELD_CALL(L, nargs, 0, CALL_HANDLER);
    }

    // All handlers completed - mark DetectedEvent wrappers as invalid
    if (is_multi)
    {
        int num_wrappers = lua_objlen(L, ARG_START);

        for (int i = 1; i <= num_wrappers; i++)
        {
            lua_rawgeti(L, ARG_START, i);
            auto *detected_event = (lua_DetectedEvent *)lua_touserdatatagged(L, -1, UTAG_DETECTED_EVENT);
            if (detected_event)
                detected_event->valid = false;
            lua_pop(L, 1);
        }
    }

    return 0;
}

void luaSL_setup_llevents_metatable(lua_State *L, int expose_internal_funcs)
{
    // Pre-create and fix system strings with memcat 0.
    // These strings are used by the host to call event handlers.
    // If they're first created in user memcat context, looking them up
    // while at the memory limit will throw LUA_ERRMEM.
    {
        LUAU_MEMCAT_GUARD(0);
        luaS_fix(luaS_newliteral(L, LLEVENTS_HANDLEEVENT_KEY));
        luaS_fix(luaS_newliteral(L, LLEVENTS_TIMER_WRAPPER_GUARD_KEY));
        luaS_fix(luaS_newliteral(L, LLEVENTS_GLOBAL_NAME));
        luaS_fix(luaS_newliteral(L, "LLEvents"));
    }

    // Set up destructor for LLEvents
    lua_setuserdatadtor(L, UTAG_LLEVENTS, llevents_dtor);

    // create metatable with all the metamethods
    lua_newtable(L);
    lua_pushvalue(L, -1);

    // pin it to the refs
    lua_ref(L, -1);
    lua_setuserdatametatable(L, UTAG_LLEVENTS);

    lua_pushcfunction(L, llevents_index, "__index");
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, llevents_newindex, "__newindex");
    lua_setfield(L, -2, "__newindex");

    // Add the event management methods to the metatable
    lua_pushcfunction(L, llevents_on, "on");
    lua_setfield(L, -2, "on");

    lua_pushcfunction(L, llevents_off, "off");
    lua_setfield(L, -2, "off");

    lua_pushcfunction(L, llevents_once, "once");
    lua_setfield(L, -2, "once");

    lua_pushcfunction(L, llevents_handlers, "handlers");
    lua_setfield(L, -2, "handlers");

    lua_pushcfunction(L, llevents_eventnames, "eventNames");
    lua_setfield(L, -2, "eventNames");

    // Store _handleEvent in registry for host access
    lua_pushcclosurek(L, llevents_handle_event_v0, "_handleEvent", 0, llevents_handle_event_v0_k);
    lua_setfield(L, LUA_REGISTRYINDEX, LLEVENTS_HANDLEEVENT_KEY);

    // Store timer wrapper guard in registry for handlers() protection
    lua_pushcfunction(L, timer_wrapper_guard, "timer_wrapper_guard");
    lua_setfield(L, LUA_REGISTRYINDEX, LLEVENTS_TIMER_WRAPPER_GUARD_KEY);

    if (expose_internal_funcs)
    {
        lua_pushcclosurek(L, llevents_handle_event_v0, "_handleEvent", 0, llevents_handle_event_v0_k);
        lua_setfield(L, -2, "_handleEvent");
    }

    // give it a proper name for `typeof()`
    lua_pushstring(L, "LLEvents");
    lua_setfield(L, -2, "__type");

    // Please don't mess with our metatable!
    lua_setreadonly(L, -1, true);
    // Okay, don't need this on the stack anymore
    lua_pop(L, 1);
}
