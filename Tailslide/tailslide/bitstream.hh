#pragma once

/*
Based on https://github.com/kaimast/bitstream/blob/master/include/bitstream.h

BSD 3-Clause License

Copyright (c) 2018, Kai Mast
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <type_traits>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <fstream>
#include <stdexcept>

#include "portable_endian.hh"


namespace Tailslide {

enum Endianness {
  ENDIAN_LITTLE,
  ENDIAN_BIG,
};

template <typename T>
using SameSizedUInt =
typename std::conditional<sizeof(T) == 1, std::uint8_t,
    typename std::conditional<sizeof(T) == 2, std::uint16_t,
        typename std::conditional<sizeof(T) == 4, std::uint32_t,
            typename std::conditional<sizeof(T) == 8, std::uint64_t,
                // cause an error if a weird-sized int is used.
                void
            >::type
        >::type
    >::type
>::type;

class BitStream {
  private:
    static constexpr uint32_t MIN_REALLOC = 10;

  public:
    explicit BitStream(Endianness endian=ENDIAN_BIG) : _mPos(0), _mSize(0), _mAllocSize(0), _mData(nullptr),
                                                       _mEndianness(endian), _mReadOnly(false) {
    }

    /**
     * @brief BitStream
     * @param data
     * @param length
     *
     * NOTE: Use assign() if you want to prevent memcpy
     */
    BitStream(const uint8_t *data, const uint32_t length, Endianness endian=ENDIAN_BIG)
        : _mPos(0), _mSize(0), _mAllocSize(MIN_REALLOC), _mData(nullptr), _mEndianness(endian), _mReadOnly(false) {
      _mData = reinterpret_cast<uint8_t *>(malloc(_mAllocSize));
      if (_mData == nullptr) {
        throw std::runtime_error("Failed to allocated data");
      }
      memset(_mData, 0, _mAllocSize);

      if (length > 0) {
        writeRawData(data, length, false);
      }
    }

    BitStream(BitStream &&other) noexcept
        : _mPos(other._mPos), _mSize(other._mSize), _mAllocSize(other._mAllocSize), _mData(other._mData),
          _mEndianness(other._mEndianness), _mReadOnly(other._mReadOnly) {
      other._mData = nullptr;
    }

    BitStream(const BitStream &other) = delete;

    ~BitStream() {
      clear();
    }

    bool isReadOnly() const {
      return _mReadOnly;
    }

    /// Create read-only BitStream that accesses this BitStream's data
    BitStream makeView() const {
      BitStream view;
      view.assign(data(), (uint32_t)size(), true);
      return view;
    }

    /**
     * Allocate initial buffer space so we reduce the number of mallocs
     */
    void preAlloc(uint32_t size) {
      if (size < _mAllocSize) {
        throw std::runtime_error("Can only increase alloc size");
      }

      size_t old_size = _mAllocSize;
      _mAllocSize = size;
      _mData = reinterpret_cast<uint8_t *>(realloc(_mData, _mAllocSize));

      if (_mData == nullptr) {
        throw std::runtime_error("Failed to allocated data");
      }
      // zero out any new data added.
      memset(_mData + old_size, 0, size - old_size);
    }

    /**
     * Create an identical copy of this BitStream
     *
     * \param force_copy [optional]
     *      Ensure the copy has it's own dedicated copy of the data
     */
    BitStream duplicate(bool force_copy = false) const {
      BitStream result;

      if (_mReadOnly && !force_copy) {
        result.assign(_mData, _mSize, true);
      } else {
        result = BitStream(_mData, _mSize);
      }

      result.moveTo(pos());
      return result;
    }

    void clear() {
      // NOLINTNEXTLINE
      if (!_mReadOnly && _mData != nullptr) {
        // NOLINTNEXTLINE
        free(_mData);
      }

      _mReadOnly = false;
      _mData = nullptr;
      _mPos = _mSize = _mAllocSize = 0;
    }

    BitStream& operator=(BitStream &&other)  noexcept {
      clear();

      _mData = other._mData;
      _mReadOnly = other._mReadOnly;
      _mPos = other._mPos;
      _mSize = other._mSize;
      _mAllocSize = other._mAllocSize;

      other._mData = nullptr;
      other._mPos = other._mSize = other._mAllocSize = 0;
      return *this;
    }

    void resize(uint32_t new_size) {
      if (_mReadOnly) {
        throw std::runtime_error("Cannot resize: is read-only!");
      }

      if (_mData == nullptr) {
        _mSize = _mAllocSize = new_size;
        if (new_size > 0) {
          _mData = reinterpret_cast<uint8_t *>(malloc(new_size));
          memset(_mData, 0, new_size);
        }
        return;
      }

      if (new_size < _mSize) {
        _mSize = new_size;
        _mPos = std::min(new_size, _mPos);
        //no reason to change alloc size?
        return;
      }

      if (new_size <= _mAllocSize) {
        _mSize = new_size;
        _mPos = std::min(new_size, _mPos);
        return;
      }

      uint32_t step = std::max(MIN_REALLOC, new_size - _mSize);
      preAlloc(_mAllocSize + step);

      _mSize = new_size;
      _mPos = std::min(new_size, _mPos);
    }

    /**
     * Write a raw buffer to the BitStream
     *
     * @note This will raise an exception if data is a nullpointer or length is of size 0
     */
    BitStream &writeRawData(const uint8_t *data, uint32_t length, bool advance = true) {
      if (data == nullptr || length == 0) {
        return *this;
      }

      if (_mPos + length > size()) {
        resize(_mPos + length);
      }

      memcpy(&_mData[_mPos], data, length);

      if (advance) {
        _mPos += length;
      }

      return *this;
    }

    void makeSpace(uint32_t increase) {
      if (_mReadOnly) {
        throw std::runtime_error("Cannot make space in read only mode");
      }

      auto remain = remainingSize();
      resize((uint32_t)(size() + increase));

      // had data after the cursor and we're increasing in size
      if (increase > 0 && remain) {
        // shift all data after the cursor up to account for the new space
        memmove(current() + increase, current(), remain);
        // zero out the space we just created
        memset(current(), 0, increase);
      }
    }

    void removeSpace(uint32_t decrease) {
      if (decrease > remainingSize()) {
        throw std::runtime_error("Not enough space left");
      }

      memmove(current(), current() + decrease, remainingSize() - decrease);
      resize((uint32_t)(size() - decrease));
    }

    void readRawData(uint8_t **data, uint32_t length) {
      if (_mPos + length > size()) {
        throw std::runtime_error("Cannot read from BitStream. Already at the end.");
      }

      *data = &_mData[_mPos];
      _mPos += length;
    }

    /**
     * Write virtually any kind of data to the stream
     *
     * @note data is expected not to be a pointer. you might have to dereference it
     */
    template<typename T>
    BitStream &operator<<(const T &data) {
      static_assert(std::is_trivially_copyable<T>(), "Need a specialized serialized function for non-POD types");

      if (_mPos + sizeof(T) > size()) {
        uint32_t new_size = _mPos + sizeof(T);
        resize(new_size);
      }

      // need special endian swapping logic if this is an integral larger than a byte
      if constexpr ((std::is_arithmetic<T>::value || std::is_enum<T>::value) && sizeof(T) > 1) {
        // Copy data into an integral type of the appropriate size and byte-swap,
        // should optimize to a bswap instruction for the given integer width.
        SameSizedUInt<T> val;
        memcpy(&val, &data, sizeof(T));
        if (_mEndianness == ENDIAN_BIG)
          val = tail_htobe(val);
        else
          val = tail_htole(val);
        memcpy(&_mData[_mPos], &val, sizeof(T));
      } else {
        memcpy(&_mData[_mPos], &data, sizeof(T));
      }
      _mPos += sizeof(T);

      return *this;
    }

    /**
     * Read virtually any kind of data from the stream
     *
     * @note data is expected not to be a pointer. you might have to dereference it
     */
    template<typename T>
    BitStream &operator>>(T &data) {
      if (_mPos + sizeof(T) > size()) {
        throw std::runtime_error("Cannot read from BitStream: Already at the end.");
      }

      // need special endian-swapping logic if this is an integral larger than a byte
      if constexpr ((std::is_arithmetic<T>::value || std::is_enum<T>::value) && sizeof(T) > 1) {
        // Copy out into an integral type of the appropriate size and byte-swap, the
        // stream position is rarely aligned for T. Should optimize to a bswap instruction.
        SameSizedUInt<T> val;
        memcpy(&val, &_mData[_mPos], sizeof(T));
        if (_mEndianness == ENDIAN_BIG)
          val = tail_betoh(val);
        else
          val = tail_letoh(val);
        memcpy(&data, &val, sizeof(T));
      } else {
        memcpy(&data, &_mData[_mPos], sizeof(T));
      }
      _mPos += sizeof(T);

      return *this;
    }

    /**
    * @brief Place a bitstream's entire contents in this bitstream, ignoring stream position.
    * @param bstream
    */
    BitStream &writeBitStream(const BitStream &other) {
      return writeRawData(other.data(), (uint32_t)other.size());
    }

    /**
     * @brief Access raw content of the stream
     * @note This should only be used internally to write to the socket
     */
    const uint8_t *data() const {
      return _mData;
    }

    uint8_t *data() {
      return _mData;
    }

    void detach(uint8_t *&out, uint32_t &len) {
      if (_mSize == 0) {
        out = nullptr;
        // there might be pre-allocated buffer
        free(_mData);
      } else if (_mSize < _mAllocSize) {
        // no need to zero out because array is shrinking
        out = reinterpret_cast<uint8_t *>(realloc(_mData, _mSize));
      } else if (_mSize == _mAllocSize) {
        out = _mData;
      } else {
        throw std::runtime_error("invalid state: m_size > m_alloc_size");
      }

      len = _mSize;

      _mData = nullptr;
      _mSize = 0;
      _mAllocSize = 0;
    }

    int64_t hash() const {
      int64_t result = 0;
      auto *ptr = reinterpret_cast<uint8_t *>(&result);

      for (uint32_t i = 0; i < this->size(); ++i) {
        ptr[i % 8] = ptr[i % 8] ^ _mData[i];
      }

      return result;
    }

    void assign(const uint8_t *buffer, uint32_t len, bool read_only) {
      if (read_only) {
        assign(const_cast<uint8_t *>(buffer), len, true);
      } else {
        throw std::runtime_error("Cannot use const reference and writable buffer");
      }
    }

    void assign(uint8_t *buffer, uint32_t len, bool read_only = false) {
      clear();

      _mData = buffer;
      _mSize = _mAllocSize = len;
      _mPos = 0;
      _mReadOnly = read_only;
    }

    /**
     * @brief Return the current size of the stream
     */
    size_t size() const {
      return _mSize;
    }

    bool empty() const {
      return _mSize == 0;
    }

    size_t remainingSize() const {
      return size() - pos();
    }

    size_t allocatedSize() const {
      return _mAllocSize;
    }

    /**
     * @brief Did we read until the end of the stream
     * @note This only makes sense when reading. Writing dynamically expands the stream
     */
    bool atEnd() const {
      return _mPos == _mSize;
    }

    /**
     * @brief Get the currenty position
     * @return Returns the position (in Bytes)
     */
    uint32_t pos() const {
      return _mPos;
    }

    bool moveTo(uint32_t pos, bool allocate = false) {
      if (pos > _mSize) {
        if (allocate) {
          resize(pos);
        } else {
          return false;
        }
      }

      _mPos = pos;
      return true;
    }

    bool moveBy(int32_t offset, bool allocate = false) {
      // make sure we don't overflow
      if (offset < 0 && static_cast<uint32_t>((-1) * offset) > _mPos) {
        throw std::runtime_error("Can't move. Would overflow buffer!");
      }

      return moveTo(_mPos + offset, allocate);
    }

    /**
     * @brief Get raw pointer to the current position in the stream
     * @return
     */
    uint8_t *current() {
      return &_mData[_mPos];
    }

    const uint8_t *current() const {
      return &_mData[_mPos];
    }

  private:
    uint32_t _mPos;
    uint32_t _mSize, _mAllocSize;

    uint8_t *_mData;

    Endianness _mEndianness;

    // a read only BitStream cannot modify the underlying data
    // useful to avoid copying and delete
    bool _mReadOnly;
};


inline bool operator==(const BitStream &first, const BitStream &second) {
  if (first.size() != second.size()) {
    return false;
  } else {
    return memcmp(first.data(), second.data(), first.size()) == 0;
  }
}

inline bool operator!=(const BitStream &first, const BitStream &second) {
  return !(first == second);
}

class ScopedBitStreamSeek {
  public:
    ScopedBitStreamSeek(BitStream &bstream, uint32_t pos, bool allocate=false) {
      _mOldPos = bstream.pos();
      _mBStream = &bstream;
      _mBStream->moveTo(pos, allocate);
    };
    ~ScopedBitStreamSeek() {
      _mBStream->moveTo(_mOldPos);
    }
  protected:
    uint32_t _mOldPos;
    BitStream *_mBStream;
};

}
