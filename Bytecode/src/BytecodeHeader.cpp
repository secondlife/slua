// ServerLua: asset header codec, see `BytecodeHeader` in BytecodeHeader.h
#include "Luau/BytecodeHeader.h"

#include <cstring>

namespace Luau
{

void writeBytecodeHeader(std::string& out, const BytecodeHeader& header)
{
    ByteWriter writer{out};
    writer.writeBytes(kBytecodeHeaderFingerprint.tag, sizeof(kBytecodeHeaderFingerprint.tag));
    writer.writeU32(kBytecodeHeaderFingerprint.major);
    writer.writeU32(kBytecodeHeaderFingerprint.minor);

    size_t section = writer.beginSection();
    writer.writeU8((uint8_t)header.isLSL);
    writer.writeU32(header.apiVersion);
    writer.writeU32((uint32_t)header.stateHandlerMasks.size());
    for (uint64_t mask : header.stateHandlerMasks)
        writer.writeU64(mask);
    writer.writeU32(header.chargedBytecodeSize);
    // New fields go here, and bump kBytecodeHeaderFingerprint.minor
    writer.endSection(section);
}

bool readBytecodeHeader(const char* data, size_t len, BytecodeHeader& header, size_t& bytecode_start)
{
    if (data == nullptr)
        return false;

    ByteReader reader{data, len};

    char tag[sizeof(kBytecodeHeaderFingerprint.tag)];
    if (!reader.readBytes(tag, sizeof(tag)) || memcmp(tag, kBytecodeHeaderFingerprint.tag, sizeof(tag)) != 0)
        return false;

    uint32_t major = 0;
    uint32_t minor = 0;
    if (!reader.readU32(major) || !reader.readU32(minor) || major != kBytecodeHeaderFingerprint.major)
        return false;

    ByteReader section{nullptr, 0};
    if (!reader.readSection(section))
        return false;

    uint8_t is_lsl = 0;
    uint32_t num_states = 0;
    if (!section.readU8(is_lsl) || !section.readU32(header.apiVersion) || !section.readU32(num_states))
        return false;
    // Eight bytes each, so a count the section can't hold is a corrupt header
    // rather than something to allocate for
    if (num_states > section.remaining / sizeof(uint64_t))
        return false;
    header.isLSL = is_lsl != 0;
    header.stateHandlerMasks.resize(num_states);
    for (uint64_t& mask : header.stateHandlerMasks)
    {
        if (!section.readU64(mask))
            return false;
    }
    if (!section.readU32(header.chargedBytecodeSize))
        return false;
    // Fields appended after 6.0 are read here only if the section has them

    bytecode_start = len - reader.remaining;
    return true;
}

} // namespace Luau
