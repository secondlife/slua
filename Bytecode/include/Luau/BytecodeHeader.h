// ServerLua: the asset format, a BytecodeHeader followed by Luau bytecode.
#pragma once

#include "Luau/ByteStream.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Luau
{

// What a host stores in front of compiled bytecode in a script asset. Carries
// what has to be known about the script before an image exists, so nothing is
// rediscovered from the VM. The codec is declared just below.
struct BytecodeHeader
{
    bool isLSL = false;
    uint32_t apiVersion = 0;
    // Per-state LSL handler masks, indexed by state number, bit (event index - 1)
    // with the index being the event's position in builtins.txt (see
    // LSLBuiltins.h). Empty for SLua.
    std::vector<uint64_t> stateHandlerMasks;
    // Bytes charged per script for the bytecode, so we can swap bytecode behind
    // people's backs without moving reported memory. Zero charges the real length.
    uint32_t chargedBytecodeSize = 0;
};

// Numbered above the header versions the server wrote privately before this
// codec existed, so the two never read as each other in a log line. Fields
// only ever get appended inside the section, so any minor under the major
// parses and the reader defaults what an older writer left out.
constexpr StateFingerprint kBytecodeHeaderFingerprint{{'L', 'U', 'A', 'U'}, 6, 0};

// Appends the header to `out`; the raw bytecode follows it
void writeBytecodeHeader(std::string& out, const BytecodeHeader& header);

// Parses the header off the front of an asset. `bytecode_start` is where the
// raw bytecode begins. False for a wrong tag, a different major, or truncation.
bool readBytecodeHeader(const char* data, size_t len, BytecodeHeader& header, size_t& bytecode_start);

} // namespace Luau
