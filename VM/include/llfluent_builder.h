#pragma once
#include <cstddef>

struct lua_State;
typedef int (*lua_CFunction)(lua_State* L);

// Default link target: apply to the prim running the script.
static const int SLUA_LINK_THIS = -4;

// A note about C++20 porting.
// The const char* name fields in the descriptors must have static storage duration 
// (e.g. string literals).
// This is not enforced at compile time,
// but when moving to C++20 we can do something like the following:
//
// struct LiteralString
// {
//    consteval LiteralString(const char* p) noexcept : ptr(p) {}
//    operator const char*() const noexcept { return ptr; }
//    const char* ptr;
// };
// The consteval constructor ensures that only string literals can be used 
// to construct a LiteralString.

struct FluentParamDescriptor
{
    const char* name;     // effective property name (pretty alias or strict fallback)
    char        semantic; // scalar: 'i'=integer 'f'=float 's'=string 'v'=vector 'r'=rotation 'b'=boolean 'a'=asset 'k'=key
                          // collection: 'C'=string-csv ({string} array → escaped comma-joined string, one tag/value pair)
                          //             'M'=string-map ({[string]:string} table → one tag/key/value triple per entry)
                          //             'N'=string-multi ({string} array → one tag/value pair per element, preserving order)
    int         tag;      // PSYS_* constant integer value
};

struct FluentFlagDescriptor
{
    const char* name;       // boolean property name, e.g. "color_interp"
    int         mask;       // bitmask, e.g. 0x01
    int         field_tag;  // tag of the integer field holding the bits, e.g. 0 for "flags"
};

struct FluentBuilderDef;

// Build a FluentBuilderDef from an array of descriptors.
// The .name pointers in descs must have static storage duration (e.g. string literals).
// Caller owns the returned pointer (process lifetime expected).
// Descriptors are sorted by tag internally; caller order does not matter.
FluentBuilderDef* fluent_builder_def_build(
    const FluentParamDescriptor* descs,
    size_t                       count
);

// Attach flag-bit boolean properties to an existing def.
// Each descriptor maps a property name to a bitmask within the integer field at field_tag.
// The .name pointers in descs must have static storage duration (e.g. string literals).
// Call after fluent_builder_def_build().
void fluent_builder_def_add_flags(
    FluentBuilderDef*           def,
    const FluentFlagDescriptor* descs,
    size_t                      count
);

// Serialize a params table into a flat tag/value rules list and push it onto the stack.
// params_idx is the stack index of the params table (may be nil — emits an empty list).
// Flag boolean properties are merged into their backing integer field before emission.
void slua_fluent_serialize(lua_State* L, int params_idx, const FluentBuilderDef* def);

// Register fn as module_name.fn_name in L's globals, with def stored as upvalue 1.
// Creates the module table if it does not yet exist; adds to it if it does.
// Sets the module table readonly after each call.
void slua_register_fluent_fn(
    lua_State*              L,
    const char*             module_name,
    const char*             fn_name,
    lua_CFunction           fn,
    const FluentBuilderDef* def
);
