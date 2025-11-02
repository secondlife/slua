#pragma once

// Must match lscript's definition!
enum class LSLIType : uint8_t {
    LST_NULL          = 0,
    LST_INTEGER       = 1,
    LST_FLOATINGPOINT = 2,
    LST_STRING        = 3,
    LST_KEY           = 4,
    LST_VECTOR        = 5,
    LST_QUATERNION    = 6,
    LST_LIST          = 7,
    LST_ERROR         = 8,   // special value so processing can continue without throwing bogus errors
    LST_MAX           = 9,
};

enum class YieldableStatus : uint8_t {
    OK = 0,
    BAD_NCALLS = 1,
    BAD_CI = 2,
    NOT_LUA = 3,
    INVALID_PC = 4,
    UNSUPPORTED_INSTR = 5,
};

#define UTAG_QUATERNION 25
#define UTAG_UUID 26
#define UTAG_DETECTED_EVENT 28
#define UTAG_LLEVENTS 29
#define UTAG_LLTIMERS 30

struct TString;

// Udatas are 16-byte aligned so don't need to pack this, we use 16 no matter what.
typedef struct lua_LSLUUID {
    // If the UUID is compressible, it can be packed into a more compact form
    uint8_t compressed;
    TString *str;
} lua_LSLUUID;

typedef struct lua_DetectedEvent {
    int32_t index;
    bool valid;
    bool can_change_damage;
} lua_DetectedEvent;

struct LuaTable;
typedef struct lua_LLEvents {
    // to be used with lua_ref(), be sure to un-pin appropriately on destruction.
    // should be a reference to a table of `event_name_str -> {table of handler closures}`
    int listeners_tab_ref;
    // Having an actual pointer is valuable for quick traversal.
    LuaTable * listeners_tab;
} lua_LLEvents;

typedef struct lua_LLTimers {
    // to be used with lua_ref(), be sure to un-pin appropriately on destruction.
    // should be a reference to an array of timer data tables
    int timers_tab_ref;
    // Having an actual pointer is valuable for quick traversal.
    LuaTable * timers_tab;
    // Reference to LLEvents instance for timer integration
    int llevents_ref;
    // Reference to timer wrapper closure for event system integration
    int timer_wrapper_ref;
} lua_LLTimers;

LUA_API int luaSL_pushuuidlstring(lua_State *L, const char *str, size_t len);
LUA_API int luaSL_pushuuidstring(lua_State *L, const char *str);
LUA_API int luaSL_pushuuidbytes(lua_State *L, const uint8_t *bytes);
LUA_API int luaSL_pushquaternion(lua_State *L, double x, double y, double z, double s);
LUA_API int luaSL_pushdetectedevent(lua_State *L, int index, bool valid, bool can_change_damage);
LUA_API int luaSL_createeventmanager(lua_State *L);
LUA_API int luaSL_createtimermanager(lua_State *L);
LUA_API const char *luaSL_checkuuid(lua_State *L, int num_arg, bool * compressed);
LUA_API const float *luaSL_checkquaternion(lua_State *L, int num_arg);
#define luaSL_pushfloat(L, d) lua_pushnumber((L), (float)(d))
#define luaSL_pushinteger(L, v) lua_pushlightuserdatatagged((L), (void *)((size_t)(v)), LU_TAG_LSL_INTEGER)
/// push whatever is the native type for this VM
LUA_API int luaSL_pushnativeinteger(lua_State *L, int val);
LUA_API void luaSL_pushindexlike(lua_State *L, int index);
LUA_API int luaSL_checkindexlike(lua_State *L, int index);
LUA_API int luaSL_checkobjectindex(lua_State *L, int len, int idx, bool compat_mode);
LUA_API void luaSL_pushboollike(lua_State *L, int val);
LUA_API uint8_t luaSL_lsl_type(lua_State *L, int idx);
/// Should only be called in an interrupt handler!
LUA_API YieldableStatus luaSL_may_interrupt(lua_State *L);
/// Returns 1 if the function at idx was defined with : syntax (has implicit self parameter), 0 otherwise
LUA_API int luaSL_ismethodstyle(lua_State *L, int idx);

typedef struct lua_TValue TValue;
uint8_t lua_lsl_type(const TValue *tv);
int lsl_cast(lua_State *L);
int lsl_cast_list_elem(lua_State *L);
int lsl_cast_list_elem_poszero(lua_State *L);

void conj_quat(float *quat);
void rot_vec(const float *vec, const float *rot, float *res);
void copy_quat(float *dest, const float *src);
