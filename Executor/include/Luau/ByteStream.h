// ServerLua: little-endian bytestream primitives for the wrappers we place
// around ares-serialized state. I regret some of my design decisions here
// and this probably should not need to be part of the public API.
#pragma once

#include <cstdint>
#include <cstring>
#include <string>

namespace Luau
{
namespace Executor
{

struct ByteWriter
{
    std::string& out;

    void writeU8(uint8_t value) { out.push_back((char)value); }

    void writeU32(uint32_t value)
    {
        for (int i = 0; i < 4; ++i)
            writeU8((uint8_t)(value >> (i * 8)));
    }

    void writeS32(int32_t value) { writeU32((uint32_t)value); }

    void writeU64(uint64_t value)
    {
        writeU32((uint32_t)value);
        writeU32((uint32_t)(value >> 32));
    }

    void writeF32(float value)
    {
        uint32_t rep;
        memcpy(&rep, &value, sizeof(rep));
        writeU32(rep);
    }

    void writeF64(double value)
    {
        uint64_t rep;
        memcpy(&rep, &value, sizeof(rep));
        writeU64(rep);
    }

    void writeBytes(const char* data, size_t len) { out.append(data, len); }

    // Length-prefixed, so it round-trips embedded nulls
    void writeString(const char* data, size_t len)
    {
        writeU32((uint32_t)len);
        writeBytes(data, len);
    }

    void writeString(const std::string& value) { writeString(value.data(), value.size()); }
};

struct ByteReader
{
    const char* data;
    size_t remaining;

    bool readBytes(void* dest, size_t len)
    {
        if (len > remaining)
            return false;
        memcpy(dest, data, len);
        data += len;
        remaining -= len;
        return true;
    }

    bool readU8(uint8_t& value) { return readBytes(&value, sizeof(value)); }

    bool readU32(uint32_t& value)
    {
        uint8_t buf[4];
        if (!readBytes(buf, sizeof(buf)))
            return false;
        value = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
        return true;
    }

    bool readS32(int32_t& value)
    {
        uint32_t rep;
        if (!readU32(rep))
            return false;
        value = (int32_t)rep;
        return true;
    }

    bool readU64(uint64_t& value)
    {
        uint32_t low;
        uint32_t high;
        if (!readU32(low) || !readU32(high))
            return false;
        value = (uint64_t)low | ((uint64_t)high << 32);
        return true;
    }

    bool readF32(float& value)
    {
        uint32_t rep;
        if (!readU32(rep))
            return false;
        memcpy(&value, &rep, sizeof(value));
        return true;
    }

    bool readF64(double& value)
    {
        uint64_t rep;
        if (!readU64(rep))
            return false;
        memcpy(&value, &rep, sizeof(value));
        return true;
    }

    bool readString(std::string& value)
    {
        uint32_t len;
        if (!readU32(len) || len > remaining)
            return false;
        value.assign(data, len);
        data += len;
        remaining -= len;
        return true;
    }
};

} // namespace Executor
} // namespace Luau
