// ServerLua: Abstractions for writing yieldable C functions with continuations.
//
// Slots are C++ primitives cached locally via PrimitiveSlot<T>. No stack
// interaction on init. On yield, destructors pack cached values into a
// an opaque tagged userdata (UTAG_OPAQUE_BUFFER) at position 1.
//
// This takes a lot of inspiration from UThreadInjector's instrumentation style,
// but makes it explicit via macros.
//
// I was surprised to find such a thing didn't already exist for C++.
// I really didn't want to be in the business of writing a bunch of macros for
// procedure -> state machine conversions, but it seems to slot moderately well
// into Luau's coroutine model.

#pragma once

#include "lua.h"
#include "Luau/Common.h"

#include <cstddef>
#include <cstring>
#include <type_traits>

// DEFINE_SLOT(bool, ...) relies on bool being 1 byte for buffer packing.
static_assert(sizeof(bool) == 1, "DEFINE_SLOT(bool) requires sizeof(bool) == 1");

namespace Luau
{

// Forward declaration — slot destructors access SlotManager members.
class SlotManager;

// Binding between a user's local variable and a position in the yield buffer.
// On yield, the destructor packs the pointed-to value via memcpy.
// Use the DEFINE_SLOT macro to declare locals + bindings together.
template<typename T>
struct PrimitiveSlot
{
    T* data;
    uint16_t offset;
    SlotManager* mgr;

    ~PrimitiveSlot();

    PrimitiveSlot(const PrimitiveSlot&) = delete;
    PrimitiveSlot(PrimitiveSlot&&) = delete;
    PrimitiveSlot& operator=(const PrimitiveSlot&) = delete;
    PrimitiveSlot& operator=(PrimitiveSlot&&) = delete;
};

// Manages slot allocation for yieldable C functions.
// Position 1 holds nil (init) or a Lua buffer (resume). Args occupy
// positions 2+. Slots live only in C++ and are packed into the buffer
// on yield via destructors.
//
// Child SlotManagers chain to a parent, sharing the same buffer.
// The innermost flag tracks which manager is the deepest active one;
// it is serialized into the buffer on yield and read back on resume.
class SlotManager
{
    lua_State* L;

    size_t requiredSize = 0;
    size_t bufferSize = 0;

    ptrdiff_t bufferStackOffset;
    bool initMode;
    bool finalized = false;

    SlotManager* parent = nullptr;
    SlotManager* currentChild = nullptr;
    // Byte 0 is reserved for the version tag; bytes 1-2 for the region length.
    // Slot data starts at offset 3 for the root manager.
    size_t baseOffset = sizeof(uint8_t) + sizeof(uint16_t);
    uint16_t regionLengthOffset = sizeof(uint8_t);
    uint16_t innermostOffset = 0;

public:
    // Set by flushForYield(). Slot destructors check this to decide
    // whether to write cached values into bufferData.
    bool yielding = false;
    // Points into the Lua buffer at position 1.
    // On resume: read pointer (slots deserialize from here).
    // On yield: write pointer (slot destructors flush here).
    char* bufferData = nullptr;
    // Tracks whether this is the deepest active manager in the chain.
    // Serialized into the buffer on yield; read back on resume.
    uint8_t innermost = true;
    // Cached at construction to avoid non-inlinable lua_callbacks() calls
    // in hot loops. Stable for the lifetime of the lua_State's global state.
    lua_Callbacks* callbacks = nullptr;

    // Root constructor: pushes nil at position 1 (init) or reads buffer (resume).
    SlotManager(lua_State* L, bool is_init);

    // Child constructor: chains to parent, shares buffer.
    // Force-inlined: called per inner-loop iteration in match helpers.
    LUAU_FORCEINLINE SlotManager(SlotManager& parent);

    // Force-inlined: called per inner-loop iteration in match helpers.
    LUAU_FORCEINLINE ~SlotManager();

    SlotManager(const SlotManager&) = delete;
    SlotManager(SlotManager&&) = delete;
    SlotManager& operator=(const SlotManager&) = delete;
    SlotManager& operator=(SlotManager&&) = delete;

    bool isInit() const { return initMode; }

    // Lock the slot layout. Must be called after all slots are allocated
    // and before YIELD_DISPATCH_BEGIN.
    // Force-inlined: called per inner-loop iteration in match helpers.
    LUAU_FORCEINLINE void finalize();

    // Creates a Lua buffer at position 1 and sets yielding=true.
    // Slot destructors then write into bufferData on stack unwind.
    void flushForYield();

    template<typename T>
    PrimitiveSlot<T> allocSlot(T* storage);
};

// Base class for RAII guards that need to run logic on yield and resume.
// Provides isInit()/isYielding() accessors and deleted copy/move.
// No vtable — derived classes implement their own constructor/destructor.
class YieldGuard
{
protected:
    SlotManager& slots;

public:
    YieldGuard(SlotManager& slots)
        : slots(slots)
    {
    }

    bool isInit() const { return slots.isInit(); }
    bool isYielding() const { return slots.yielding; }

    YieldGuard(const YieldGuard&) = delete;
    YieldGuard(YieldGuard&&) = delete;
    YieldGuard& operator=(const YieldGuard&) = delete;
    YieldGuard& operator=(YieldGuard&&) = delete;
};

// A very simple kind of slot that just does a straight memcpy in / out of storage
// Note that padding and alignment are important if you're using an aggregate type
// like a `struct`. As such, it's advisable to have some separate step that
// converts between a structure with a fixed size cross-platform and its runtime
// representation. See `MatchState` for an example.
template<typename T>
inline PrimitiveSlot<T> SlotManager::allocSlot(T* storage)
{
    static_assert(std::is_trivially_copyable_v<T>, "PrimitiveSlot<T> requires a trivially copyable type");
    LUAU_ASSERT(!finalized);
    auto off = static_cast<uint16_t>(baseOffset + requiredSize);
    requiredSize += sizeof(T);

    if (!initMode)
    {
        LUAU_ASSERT(off + sizeof(T) <= bufferSize);
        memcpy(storage, bufferData + off, sizeof(T));
    }

    return PrimitiveSlot<T>{storage, off, this};
}

template<typename T>
inline PrimitiveSlot<T>::~PrimitiveSlot()
{
    if (mgr->yielding)
        memcpy(mgr->bufferData + offset, data, sizeof(T));
}

// Child constructor. Chains to parent, sharing the same buffer region.
// initMode is determined by parent.innermost: if parent is innermost
// (no child data in buffer), this is a fresh call; otherwise, resume.
LUAU_FORCEINLINE SlotManager::SlotManager(SlotManager& parent)
    : L(parent.L)
    , bufferStackOffset(parent.bufferStackOffset)
    , initMode(parent.innermost)
    , parent(&parent)
    , baseOffset(parent.baseOffset + parent.requiredSize + sizeof(uint16_t))
    , regionLengthOffset(static_cast<uint16_t>(parent.baseOffset + parent.requiredSize))
{
    LUAU_ASSERT(parent.finalized);
    LUAU_ASSERT(parent.currentChild == nullptr);
    parent.currentChild = this;
    parent.innermost = false;
    callbacks = parent.callbacks;

    if (!initMode)
    {
        bufferData = parent.bufferData;
        bufferSize = parent.bufferSize;
    }
}

LUAU_FORCEINLINE SlotManager::~SlotManager()
{
    // Serialize region length and innermost flag during yield (before RAII restore)
    if (yielding)
    {
        uint16_t regionLength = static_cast<uint16_t>(requiredSize);
        memcpy(bufferData + regionLengthOffset, &regionLength, sizeof(uint16_t));
        memcpy(bufferData + innermostOffset, &innermost, sizeof(uint8_t));
    }

    if (parent)
    {
        // Only restore parent's innermost on non-yield destruction.
        // During yield we leave it false so the serialized state correctly
        // records that parent had a child.
        if (!yielding)
            parent->innermost = true;
        parent->currentChild = nullptr;
    }
}

LUAU_FORCEINLINE void SlotManager::finalize()
{
    LUAU_ASSERT(!finalized);

    // Allocate innermost flag as last byte of this manager's region.
    innermostOffset = static_cast<uint16_t>(baseOffset + requiredSize);
    requiredSize += sizeof(uint8_t);
    LUAU_ASSERT(requiredSize <= UINT16_MAX);
    finalized = true;

    if (!initMode)
    {
        // Validate that the stored region length matches what we expect.
        uint16_t storedLength;
        memcpy(&storedLength, bufferData + regionLengthOffset, sizeof(uint16_t));
        LUAU_ASSERT(storedLength == requiredSize);

        // Read innermost flag from buffer.
        LUAU_ASSERT(innermostOffset + sizeof(uint8_t) <= bufferSize);
        memcpy(&innermost, bufferData + innermostOffset, sizeof(uint8_t));

        // Only the innermost manager validates total size and clears buffer.
        if (innermost)
        {
            LUAU_ASSERT(baseOffset + requiredSize <= bufferSize);
            bufferData = nullptr;
            bufferSize = 0;
            // Leave buffer at position 1 for reuse on next yield.
            // If the function returns normally, it becomes GC garbage.
        }
    }
}

} // namespace Luau
