#pragma once
#include <stddef.h>

struct lua_State;

struct FluentParamDescriptor
{
    const char* name;     // effective property name (pretty alias or strict fallback)
    char        semantic; // 'i','f','s','v','r','b','a','k'
    int         tag;      // PSYS_* constant integer value
};

struct FluentFlagDescriptor
{
    const char* name;       // boolean property name, e.g. "interp_color"
    int         mask;       // bitmask, e.g. 0x01
    int         field_tag;  // tag of the integer field holding the bits, e.g. 0 for "flags"
};

// Opaque handle — definition lives in llfluent_builder.cpp.
struct FluentBuilderDef;

// Build a FluentBuilderDef from an array of descriptors.
// Deep-copies all strings. Caller owns the returned pointer (process lifetime expected).
// Descriptors are sorted by tag internally; caller order does not matter.
FluentBuilderDef* fluent_builder_def_build(
    const char*                  apply_fn_name,       // e.g. "ParticleSystem"
    const char*                  apply_link_fn_name,  // e.g. "LinkParticleSystem"
    const FluentParamDescriptor* descs,
    size_t                       count
);

// Attach flag-bit boolean properties to an existing def.
// Each descriptor maps a property name to a bitmask within the integer field at field_tag.
// Deep-copies all strings. Call after fluent_builder_def_build().
void fluent_builder_def_add_flags(
    FluentBuilderDef*           def,
    const FluentFlagDescriptor* descs,
    size_t                      count
);

// Register a fluent builder module into L.
// Creates a global table named module_name containing a type table named type_name.
// def must remain valid for the lifetime of L.
// Call this on mConstsState before luaL_sandbox(), alongside luaL_register_noclobber.
void slua_open_fluent_builder(
    lua_State*              L,
    const char*             module_name,  // e.g. "llparticle"
    const char*             type_name,    // e.g. "ParticleParams"
    const FluentBuilderDef* def
);
