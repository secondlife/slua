#define lllevents_c

#include <cstring>
#include "lua.h"
#include "lcommon.h"
#include "lualib.h"
#include "llsl.h"
#include "lllevents.h"

#include "lobject.h"
#include "lstate.h"
#include "lapi.h"

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
    const lua_DetectedEvent *detected_event = (const lua_DetectedEvent *)lua_touserdatatagged(L, 1, UTAG_DETECTED_EVENT);
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
    const lua_DetectedEvent *detected_event = (const lua_DetectedEvent *)lua_touserdatatagged(L, 1, UTAG_DETECTED_EVENT);
    if (!detected_event)
        luaL_typeerror(L, 1, "DetectedEvent");

    if (!detected_event->valid)
        luaL_errorL(L, "DetectedEvent is no longer valid");

    lua_getglobal(lua_mainthread(L), "ll");
    lua_rawgetfield(L, -1, function_name);
    lua_pushinteger(L, detected_event->index);
    lua_call(L, 1, 1);
    return 1;
}

// TODO: these can all just be lambdas, they'll be reachable by the permanents collector
//  through the LLEvents metatable.
static int detected_event_get_owner(lua_State *L)
{
    return detected_event_call_ll_function(L, "DetectedOwner");
}

static int detected_event_get_name(lua_State *L)
{
    return detected_event_call_ll_function(L, "DetectedName");
}

static int detected_event_get_key(lua_State *L)
{
    return detected_event_call_ll_function(L, "DetectedKey");
}

static int detected_event_get_face(lua_State *L)
{
    return detected_event_call_ll_function(L, "DetectedFace");
}

static int detected_event_get_link_number(lua_State *L)
{
    return detected_event_call_ll_function(L, "DetectedLinkNumber");
}

static int detected_event_adjust_damage(lua_State *L)
{
    const lua_DetectedEvent *detected_event = (const lua_DetectedEvent *)lua_touserdatatagged(L, 1, UTAG_DETECTED_EVENT);
    if (!detected_event)
        luaL_typeerror(L, 1, "DetectedEvent");

    if (!detected_event->valid)
        luaL_errorL(L, "DetectedEvent is no longer valid");

    if (!detected_event->can_change_damage)
        luaL_errorL(L, "Cannot adjust damage for this DetectedEvent");

    double damage = luaL_checknumber(L, 2);

    lua_getglobal(lua_mainthread(L), "ll");
    lua_rawgetfield(L, -1, "AdjustDamage");
    lua_pushinteger(L, detected_event->index);
    lua_pushnumber(L, damage);
    lua_call(L, 2, 0);
    return 0;
}

static int detected_event_tostring(lua_State *L)
{
    const lua_DetectedEvent *detected_event = (const lua_DetectedEvent *)lua_touserdatatagged(L, 1, UTAG_DETECTED_EVENT);
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

    // Add the methods to the metatable
    lua_pushcfunction(L, detected_event_get_owner, "getOwner");
    lua_setfield(L, -2, "getOwner");

    lua_pushcfunction(L, detected_event_get_name, "getName");
    lua_setfield(L, -2, "getName");

    lua_pushcfunction(L, detected_event_get_key, "getKey");
    lua_setfield(L, -2, "getKey");

    lua_pushcfunction(L, detected_event_get_face, "getFace");
    lua_setfield(L, -2, "getFace");

    lua_pushcfunction(L, detected_event_get_link_number, "getLinkNumber");
    lua_setfield(L, -2, "getLinkNumber");

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
    if (llevents->listeners_tab_ref != -1)
    {
        lua_unref(L, llevents->listeners_tab_ref);
        llevents->listeners_tab_ref = -1;
    }
}

int luaSL_createeventmanager(lua_State *L)
{
    lua_checkstack(L, 2);
    auto *llevents = (lua_LLEvents *)lua_newuserdatataggedwithmetatable(L, sizeof(lua_LLEvents), UTAG_LLEVENTS);

    // Create empty listeners table
    lua_createtable(L, 0, 0);
    llevents->listeners_tab_ref = lua_ref(L, -1);
    llevents->listeners_tab = hvalue(luaA_toobject(L, -1));
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

    luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);

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
    const lua_LLEvents *llevents = (const lua_LLEvents *)lua_touserdatatagged(L, 1, UTAG_LLEVENTS);
    if (!llevents)
        luaL_typeerror(L, 1, "LLEvents");

    const char *event_name = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    // Get the listeners table
    lua_getref(L, llevents->listeners_tab_ref);

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

    // Add the handler to the array
    lua_pushvalue(L, 3); // handler function
    lua_rawseti(L, -2, lua_objlen(L, -2) + 1);

    // Return the handler so it can be unregistered later
    lua_pushvalue(L, 3);
    return 1;
}

static int llevents_off(lua_State *L)
{
    auto *sl_state = LUAU_GET_SL_VM_STATE(lua_mainthread(L));

    const lua_LLEvents *llevents = (const lua_LLEvents *)lua_touserdatatagged(L, 1, UTAG_LLEVENTS);
    if (!llevents)
        luaL_typeerror(L, 1, "LLEvents");

    const char *event_name = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    // Get the listeners table
    lua_getref(L, llevents->listeners_tab_ref);
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

    // Find and remove the handler
    // This might seem jank, but it's basically a merged `table.find()` and `table.remove()`.
    int len = lua_objlen(L, -1);
    bool found = false;

    for (int i = len; i >= 1; i--)
    {
        lua_rawgeti(L, -1, i);
        if (lua_equal(L, -1, 3))
        {
            // pop the handler, we don't need it.
            lua_pop(L, 1);
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
        // pop the old value.
        lua_pop(L, 1);
    }

    // Clear out the empty handler array if we have no more handlers for this event type
    if (found && lua_objlen(L, -1) == 0)
    {
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
    const lua_LLEvents *llevents = (const lua_LLEvents *)lua_touserdatatagged(L, 1, UTAG_LLEVENTS);
    if (!llevents)
        luaL_typeerror(L, 1, "LLEvents");

    luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);

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

static int llevents_listeners(lua_State *L)
{
    const lua_LLEvents *llevents = (const lua_LLEvents *)lua_touserdatatagged(L, 1, UTAG_LLEVENTS);
    if (!llevents)
        luaL_typeerror(L, 1, "LLEvents");

    const char *event_name = luaL_checkstring(L, 2);

    // Get the listeners table
    lua_getref(L, llevents->listeners_tab_ref);
    lua_rawgetfield(L, -1, event_name);

    if (lua_isnil(L, -1))
    {
        lua_createtable(L, 0, 0);
        return 1;
    }

    // Return a clone of the handlers array
    lua_clonetable(L, -1);
    return 1;
}

static int llevents_eventnames(lua_State *L)
{
    const lua_LLEvents *llevents = (const lua_LLEvents *)lua_touserdatatagged(L, 1, UTAG_LLEVENTS);
    if (!llevents)
        luaL_typeerror(L, 1, "LLEvents");

    // Get the listeners table
    lua_getref(L, llevents->listeners_tab_ref);

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
    "damage",
    "final_damage"
};

// Stack indices for yield-safe event handler continuation
enum EventHandlerStack {
    // The handler index serves as a pointer into the (cloned) handlers
    // table,
    HANDLER_INDEX = 1,
    IS_MULTI = 2,
    // How many handlers are in the handlers table (cached for speed)
    HANDLERS_LEN = 3,
    HANDLERS_TABLE = 4,
    // number of args after `ARG_START`
    NARGS = 5,
    // multi-args start after this.
    ARG_START = 6
};

static bool is_multi_event(const char* event_name)
{
    return MULTI_EVENT_NAMES.find(event_name) != MULTI_EVENT_NAMES.end();
}

int llevents_handle_event_cont(lua_State *L, int status)
{
    int handler_index = lua_tointeger(L, HANDLER_INDEX);
    int handlers_len = lua_tointeger(L, HANDLERS_LEN);
    int nargs = lua_tointeger(L, NARGS);
    bool is_multi = lua_toboolean(L, IS_MULTI);

    // We need rather a lot of overhead.
    lua_checkstack(L, lua_gettop(L) * 3);
    // Continue with next handler
    handler_index++;

    while (handler_index <= handlers_len)
    {
        lua_rawgeti(L, HANDLERS_TABLE, handler_index);
        LUAU_ASSERT(lua_type(L, -1) == LUA_TFUNCTION);

        // Push arguments for this handler call
        for (int j = 0; j < nargs; j++)
        {
            lua_pushvalue(L, ARG_START + j);
        }

        // Update handler index on stack for next continuation
        lua_pushinteger(L, handler_index);
        lua_replace(L, HANDLER_INDEX);

        // Call handler, we want errors to bubble up and only handle yield/break.
        lua_call(L, nargs, 0);
        if (L->status != LUA_OK)
            return -1;

        handler_index++;
    }

    // All handlers completed - mark DetectedEvent wrappers as invalid
    if (is_multi)
    {
        // We only care about the first argument with our (readonly) table of wrappers.
        int num_wrappers = lua_objlen(L, ARG_START);

        for (int i = 1; i <= num_wrappers; i++)
        {
            lua_rawgeti(L, ARG_START, i);
            lua_DetectedEvent *detected_event = (lua_DetectedEvent *)lua_touserdatatagged(L, -1, UTAG_DETECTED_EVENT);
            if (detected_event)
            {
                detected_event->valid = false;
            }
            lua_pop(L, 1);
        }
    }

    return 0;
}

static int llevents_handle_event_init(lua_State *L)
{
    // Arguments: llevents_userdata, event_name, ...event_args
    const lua_LLEvents *llevents = (const lua_LLEvents *)lua_touserdatatagged(L, 1, UTAG_LLEVENTS);
    if (!llevents)
        luaL_typeerror(L, 1, "LLEvents");

    const char *event_name = luaL_checkstring(L, 2);
    int nargs = lua_gettop(L) - 2;
    bool is_multi = is_multi_event(event_name);

    // I can't remember how much this needs temporarily, this should be fine.
    lua_checkstack(L, nargs + 10);

    lua_getref(L, llevents->listeners_tab_ref);
    lua_rawgetfield(L, -1, event_name);

    // We have no listeners for this event.
    if (lua_isnil(L, -1))
    {
        return 0;
    }

    auto *sl_state = LUAU_GET_SL_VM_STATE(L);
    if (sl_state->mayCallHandleEventCb != nullptr && !sl_state->mayCallHandleEventCb(L))
    {
        // Not allowed to call `handleEvent()`. This isn't for security reasons or anything,
        // it's mostly to prevent people from using this for their own home-cooked dispatch stuff
        // that we don't want to support in internal APIs.
        luaL_errorL(L, "Not allowed to call LLEvents:handleEvent()");
    }

    // Clone the handlers array so modifications during handling don't affect us
    lua_clonetable(L, -1);
    lua_remove(L, -2);  // Remove original handlers array
    lua_remove(L, -2);  // Remove listeners_tab
    int handlers_len = lua_objlen(L, -1);

    // Empty array isn't valid, val should be nil if no handlers.
    LUAU_ASSERT(handlers_len > 0);

    if (is_multi)
    {
        // First argument should be num_detected for multi-event types,
        // which is of course the _third_ argument for `handleEvent()` itself.
        int num_detected = luaL_checkinteger(L, 3);

        // TODO: some kind of RAII thingy that temporarily sets memcat to 0 here, these should be free.

        // Create DetectedEvent wrappers table
        lua_createtable(L, num_detected, 0);

        bool can_adjust_damage = (strcmp(event_name, "damage") == 0);
        for (int i = 1; i <= num_detected; i++)
        {
            luaSL_pushdetectedevent(L, i, true, can_adjust_damage);
            lua_rawseti(L, -2, i);
        }

        lua_setreadonly(L, -1, true);
        lua_replace(L, 3);
    }

    // Insert context values at their enum positions
    lua_pushinteger(L, 0);
    lua_insert(L, HANDLER_INDEX);

    lua_pushboolean(L, is_multi);
    lua_insert(L, IS_MULTI);

    lua_pushinteger(L, handlers_len);
    lua_insert(L, HANDLERS_LEN);

    // handlers_table already on top
    lua_insert(L, HANDLERS_TABLE);

    lua_pushinteger(L, nargs);
    lua_insert(L, NARGS);

    // Remove llevents and event_name which are now at ARG_START
    lua_remove(L, ARG_START);  // Remove llevents
    lua_remove(L, ARG_START);  // Remove event_name

    return llevents_handle_event_cont(L, LUA_OK);
}

void luaSL_setup_llevents_metatable(lua_State *L)
{
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

    lua_pushcfunction(L, llevents_listeners, "listeners");
    lua_setfield(L, -2, "listeners");

    lua_pushcfunction(L, llevents_eventnames, "eventNames");
    lua_setfield(L, -2, "eventNames");

    lua_pushcclosurek(L, llevents_handle_event_init, "handleEvent", 0, llevents_handle_event_cont);
    lua_setfield(L, -2, "handleEvent");

    // give it a proper name for `typeof()`
    lua_pushstring(L, "LLEvents");
    lua_setfield(L, -2, "__type");

    // Please don't mess with our metatable!
    lua_setreadonly(L, -1, true);
    // Okay, don't need this on the stack anymore
    lua_pop(L, 1);
}
