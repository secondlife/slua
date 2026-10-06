// ServerLua: little-endian bytestream primitives for the wrappers we place
// around ares-serialized state and in front of bytecode. I regret some of my
// design decisions here and this probably should not need to be part of the
// public API.
#pragma once

#include <cstdint>
#include <cstring>
#include <string>

namespace Luau
{

// Identifies which class a persisted payload belongs to, and which layout it
// used. A reader refuses a different `major` and accepts any `minor`: fields
// are only ever added inside length-prefixed sections, as blocks, so a newer
// minor's extra bytes are skipped. Bump `minor` when adding a block, `major`
// for anything that can't be expressed that way.
struct StateFingerprint
{
    // A FOURCC, so the head of a payload reads as text in a dump
    char tag[4];
    uint32_t major;
    uint32_t minor;
};
static_assert(sizeof(StateFingerprint::tag) == 4);

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

    // Length-prefixed section, returns its pos so the size can be back-patched with `endSection()`.
    size_t beginSection()
    {
        size_t at = out.size();
        writeU32(0);
        return at;
    }

    void endSection(size_t at)
    {
        uint32_t len = (uint32_t)(out.size() - at - 4);
        for (int i = 0; i < 4; ++i)
            out[at + i] = (char)(uint8_t)(len >> (i * 8));
    }

    struct Block
    {
        Block(ByteWriter& writer, uint8_t id)
            : writer(writer)
        {
            writer.writeU8(id);
            at = writer.beginSection();
        }

        ~Block() { writer.endSection(at); }

        Block(const Block&) = delete;
        Block& operator=(const Block&) = delete;

    private:
        ByteWriter& writer;
        size_t at;
    };
};

struct ByteBlocks;

struct ByteReader
{
    const char* data;
    size_t remaining;
    // Set by a block walk that met a header it couldn't parse, the one failure
    // a range-for can't return
    bool failed = false;

    bool ok() const { return !failed; }

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

    // Hands back a reader bounded to the next section and steps over it
    bool readSection(ByteReader& section)
    {
        uint32_t len;
        if (!readU32(len) || len > remaining)
            return false;
        section = ByteReader{data, len};
        data += len;
        remaining -= len;
        return true;
    }

    bool atEnd() const { return remaining == 0; }

    // The section's blocks, see ByteBlocks
    inline ByteBlocks blocks();
};

// A block's body, bounded, with its id
struct ByteBlock : ByteReader
{
    uint8_t id = 0;
};

struct ByteBlocks
{
    explicit ByteBlocks(ByteReader& reader)
        : reader(reader)
    {
    }

    struct Sentinel
    {
    };

    struct Iterator
    {
        ByteBlocks* blocks;

        // By value, so `auto` and `auto&&` both give a reader the body can use
        ByteBlock operator*() const { return blocks->block; }
        Iterator& operator++()
        {
            blocks->next();
            return *this;
        }
        bool operator!=(Sentinel) const { return blocks->on_block; }
    };

    Iterator begin()
    {
        next();
        return Iterator{this};
    }

    Sentinel end() { return {}; }

private:
    void next()
    {
        on_block = false;
        if (reader.atEnd())
            return;
        if (!reader.readU8(block.id) || !reader.readSection(block))
        {
            reader.failed = true;
            return;
        }
        on_block = true;
    }

    ByteReader& reader;
    ByteBlock block{};
    bool on_block = false;
};

inline ByteBlocks ByteReader::blocks()
{
    return ByteBlocks(*this);
}

} // namespace Luau
