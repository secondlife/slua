// ServerLua: Macros for writing yieldable C functions with continuations.
//
// This header pulls in the lyieldable types and provides the DEFINE_SLOT,
// DEFINE_YIELDABLE, YIELD_DISPATCH, YIELD_CHECK, YIELD_CALL, and
// YIELD_HELPER macros. Include this in .cpp files that implement yieldable
// functions; include lyieldable.h directly if you only need the types.

#pragma once

#include "lyieldable.h"

using Luau::SlotManager;
using Luau::PrimitiveSlot;
using Luau::YieldGuard;

// Declares a local variable and binds it to a yieldable slot.
// Storage is in always in the local, the slot whisks the local's value
// it into the yield buffer in the yield path via RAII + destructors.
#define DEFINE_SLOT(type, name, ...)                                   \
    type name = __VA_ARGS__;                                           \
    [[maybe_unused]] auto _pslot_##name = slots.allocSlot(&name)


// DEFINE_YIELDABLE_IMPL: base macro parameterized by linkage specifier.
// Generates the init function (name), continuation (name_k), and body
// (name_body) from a single function definition. The body receives
// lua_State* L and bool is_init. Body is always static.
#define DEFINE_YIELDABLE_IMPL(linkage, name, version)                \
    static int name##_v##version##_body(lua_State* L, bool is_init);     \
    linkage int name##_v##version(lua_State* L)                          \
    {                                                       \
        return name##_v##version##_body(L, true);                        \
    }                                                       \
    linkage int name##_v##version##_k(lua_State* L, int status)          \
    {                                                       \
        lua_checkstack(L, LUA_MINSTACK);                    \
        return name##_v##version##_body(L, false);                       \
    }                                                       \
    static int name##_v##version##_body(lua_State* L, bool is_init)

// DEFINE_YIELDABLE: static linkage (single translation unit).
#define DEFINE_YIELDABLE(name, version) DEFINE_YIELDABLE_IMPL(static, name, version)

// DEFINE_YIELDABLE_EXTERN: external linkage (cross translation unit).
#define DEFINE_YIELDABLE_EXTERN(name, version) DEFINE_YIELDABLE_IMPL(LUAI_FUNC, name, version)

// Return value macros: define lambdas for early-return in yield macros.
// Must appear before YIELD_DISPATCH_BEGIN in each function.
// Uses lambdas so `return _yieldable_on_yield()` works for void too,
// _should_ optimize to just a bare `return`.
#define YIELDABLE_RETURNS_INTERNAL(yield_val, error_val)                                     \
    [[maybe_unused]] auto _yieldable_on_yield = [&]() { return (yield_val); };     \
    [[maybe_unused]] auto _yieldable_on_error = [&]() { return (error_val); }

// Standard returns for DEFINE_YIELDABLE bodies: -1 on yield, 0 on error.
#define YIELDABLE_RETURNS_DEFAULT YIELDABLE_RETURNS_INTERNAL(-1, 0)
// Take a single default value for functions that don't return int
#define YIELDABLE_RETURNS(x) YIELDABLE_RETURNS_INTERNAL((x), (x))

// For void helper functions that have no return value.
#define YIELDABLE_RETURNS_VOID                                                      \
    [[maybe_unused]] auto _yieldable_on_yield = []() {};                           \
    [[maybe_unused]] auto _yieldable_on_error = []() {}

// Yield macros: manage the yield state machine with explicit, stable phase
// names & values. Callers must define a function-local `enum class Phase : uint8_t`
// with `DEFAULT = 0` as the first entry.
//
// The dispatch switch at the top maps Phase values to goto labels in the body.
// Body code is _not_ inside the switch, so nested switches work freely.

// Opens the dispatch region: sets up aliases and opens the dispatch switch.
// DEFAULT breaks out of the switch into the body code.
#define YIELD_DISPATCH_BEGIN(phase_slot, slot_mgr)                                  \
    /* goto + label: duplicate label prevents multiple dispatch regions per fn */    \
    goto _yieldable_entered; _yieldable_entered:                                    \
    auto& _yieldable_phase = (phase_slot);                                          \
    [[maybe_unused]] auto& _yieldable_slots = (slot_mgr);                           \
    switch (_yieldable_phase) {                                                     \
    case Phase::DEFAULT: break

// One dispatch case: maps a Phase value to its goto label in the body.
#define YIELD_DISPATCH(phase_name) \
    case Phase::phase_name: goto _yieldable_label_##phase_name

// Closes the dispatch switch.
#define YIELD_DISPATCH_END()                                                        \
    default:                                                                        \
        LUAU_ASSERT(!"Unhandled yieldable phase");                                  \
    } (void)0

// Fires the interrupt handler and yields if the VM requests it.
// On resume, the dispatch goto jumps to the label at the tail of the
// do-while; the while(0) exits and execution continues after the macro.
#define YIELD_CHECK(L, phase_name, reason)                                          \
    do                                                                              \
    {                                                                               \
        (_yieldable_phase) = Phase::phase_name;                                     \
        if ((L)->status == LUA_OK)                                                  \
        {                                                                           \
            void (*_yc_int)(lua_State*, int) = _yieldable_slots.callbacks->interrupt; \
            if (LUAU_LIKELY(!!_yc_int))                                             \
                _yc_int((L), (reason));                                             \
        }                                                                           \
        if ((L)->status != LUA_OK)                                                  \
        {                                                                           \
            if ((L)->status == LUA_YIELD || (L)->status == LUA_BREAK)               \
            {                                                                       \
                _yieldable_slots.flushForYield();                                   \
                return _yieldable_on_yield();                                       \
            }                                                                       \
            return _yieldable_on_error();                                           \
        }                                                                           \
    _yieldable_label_##phase_name: ;                                                \
    } while (0)

// Calls a Lua function that may yield. On resume, execution continues
// after the call.
#define YIELD_CALL(L, nargs, nresults, phase_name)                                  \
    do                                                                              \
    {                                                                               \
        (_yieldable_phase) = Phase::phase_name;                                     \
        lua_call(L, nargs, nresults);                                               \
        if ((L)->status != LUA_OK)                                                  \
        {                                                                           \
            if ((L)->status == LUA_YIELD || (L)->status == LUA_BREAK)               \
            {                                                                       \
                _yieldable_slots.flushForYield();                                   \
                return _yieldable_on_yield();                                       \
            }                                                                       \
            return _yieldable_on_error();                                           \
        }                                                                           \
    _yieldable_label_##phase_name: ;                                                \
    } while (0)

// Calls a helper function that has its own chained SlotManager and dispatch
// region. The goto label is BEFORE the call so the helper is re-called on
// resume. No flushForYield here — the helper already flushed the entire chain.
// NB: you should NOT use any expressions that mutate as arguments to the
// helper. They will be re-run on resume, repeating the mutation!
#define YIELD_HELPER(L, phase_name, ...)                                            \
    do                                                                              \
    {                                                                               \
        (_yieldable_phase) = Phase::phase_name;                                     \
    _yieldable_label_##phase_name:                                                  \
        __VA_ARGS__;                                                                \
        if ((L)->status != LUA_OK)                                                  \
        {                                                                           \
            if ((L)->status == LUA_YIELD || (L)->status == LUA_BREAK)               \
                return _yieldable_on_yield();                                       \
            return _yieldable_on_error();                                           \
        }                                                                           \
    } while (0)
