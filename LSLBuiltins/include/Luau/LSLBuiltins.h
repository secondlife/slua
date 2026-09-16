#pragma once

// ServerLua: builtins.txt as data, the constants the compiler folds and the
// events the runtime dispatches by. Depends on neither the VM nor the
// compiler; the constant folder hook lives in luacode.h and the globals
// pusher in llsl.h.

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Luau
{

// Same numbering as LSLIType in the VM and lscript
enum class SLConstantType : uint8_t
{
    Void = 0,
    Integer = 1,
    Float = 2,
    String = 3,
    Key = 4,
    Vector = 5,
    Quaternion = 6,
    List = 7,
    Error = 255,
};

struct SLConstant
{
    SLConstantType type = SLConstantType::Error;
    size_t stringLength = 0;

    union
    {
        int32_t valueInteger;
        double valueNumber;
        float valueVector[3];
        float valueQuat[4];
        // Length stored in stringLength
        const char* valueString = nullptr;
    };
};

// Every `const` in the loaded builtins.txt, by name
const std::unordered_map<std::string, SLConstant>& luauSL_constants();
// Null for a name the file doesn't define
const SLConstant* luauSL_find_constant(const char* name);

// Events are numbered by their position in builtins.txt, which is also the
// server's enum order and the bit order of the asset header's handler masks.
// These are the ones every builtins.txt is expected to carry, in this order;
// anything appended to the file after them is reachable by index only.
#define LUAU_LSL_EVENTS(X) \
    X(StateEntry, "state_entry") \
    X(StateExit, "state_exit") \
    X(TouchStart, "touch_start") \
    X(Touch, "touch") \
    X(TouchEnd, "touch_end") \
    X(CollisionStart, "collision_start") \
    X(Collision, "collision") \
    X(CollisionEnd, "collision_end") \
    X(LandCollisionStart, "land_collision_start") \
    X(LandCollision, "land_collision") \
    X(LandCollisionEnd, "land_collision_end") \
    X(Timer, "timer") \
    X(Listen, "listen") \
    X(OnRez, "on_rez") \
    X(Sensor, "sensor") \
    X(NoSensor, "no_sensor") \
    X(Control, "control") \
    X(Money, "money") \
    X(Email, "email") \
    X(AtTarget, "at_target") \
    X(NotAtTarget, "not_at_target") \
    X(AtRotTarget, "at_rot_target") \
    X(NotAtRotTarget, "not_at_rot_target") \
    X(RunTimePermissions, "run_time_permissions") \
    X(Changed, "changed") \
    X(Attach, "attach") \
    X(Dataserver, "dataserver") \
    X(LinkMessage, "link_message") \
    X(MovingStart, "moving_start") \
    X(MovingEnd, "moving_end") \
    X(ObjectRez, "object_rez") \
    X(RemoteData, "remote_data") \
    X(HttpResponse, "http_response") \
    X(HttpRequest, "http_request") \
    X(ExperiencePermissions, "experience_permissions") \
    X(TransactionResult, "transaction_result") \
    X(PathUpdate, "path_update") \
    X(ExperiencePermissionsDenied, "experience_permissions_denied") \
    X(LinksetData, "linkset_data") \
    X(GameControl, "game_control") \
    X(OnDeath, "on_death") \
    X(OnDamage, "on_damage") \
    X(FinalDamage, "final_damage")

// The event's number, starting at 1. Plain ints rather than an enum class:
// a builtins.txt may carry events past these, so every API takes an int and
// these are just the names for the ones known at compile time.
namespace LSLEvent
{
enum : int
{
    None = 0,
#define LUAU_LSL_EVENT_ENUM(name, str) name,
    LUAU_LSL_EVENTS(LUAU_LSL_EVENT_ENUM)
#undef LUAU_LSL_EVENT_ENUM
    // One past the last event the names cover
    KnownCount,
};
} // namespace LSLEvent

// The event's bit in a handler mask, zero for an index a mask can't hold
inline uint64_t lslEventBit(int index)
{
    return index >= 1 && index <= 64 ? (uint64_t)1 << (index - 1) : 0;
}

// The same bits by name, for the events the enum knows
namespace LSLEventBit
{
#define LUAU_LSL_EVENT_BIT(name, str) constexpr uint64_t name = (uint64_t)1 << (LSLEvent::name - 1);
LUAU_LSL_EVENTS(LUAU_LSL_EVENT_BIT)
#undef LUAU_LSL_EVENT_BIT
} // namespace LSLEventBit

// Replaces the registry with a builtins.txt's events, in file order. Returns
// false and changes nothing unless the enum's events are all present at their
// positions.
bool setLSLEventNames(const std::vector<std::string>& names);
// In file order, so names[i] is event i + 1. Holds the enum's events until a
// builtins.txt is loaded.
const std::vector<std::string>& getLSLEventNames();
// Zero for a name the registry doesn't know
int lslEventIndex(const char* name);
// Null for an index the registry doesn't know
const char* lslEventName(int index);

} // namespace Luau

// Called once at startup, not thread-safe. Loads the constants and the event
// registry, from the embedded builtins.txt when `builtins_file` is null.
void luauSL_init_global_builtins(const char* builtins_file);
