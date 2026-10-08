#pragma once

// "License": Public Domain
// I, Mathias PanzenbAck, place this file hereby into the public domain. Use it at your own risk for whatever you like.
// In case there are jurisdictions that don't support putting things in the public domain you can also consider it to
// be "dual licensed" under the BSD, MIT and Apache licenses, if you want to. This code is trivial anyway. Consider it
// an example on how to get the endian conversion functions on different platforms.

#if (defined(_WIN16) || defined(_WIN32) || defined(_WIN64)) && !defined(__WINDOWS__)
# define __WINDOWS__
#endif

#if defined(__linux__) || defined(__CYGWIN__)

# include <endian.h>

#elif defined(__APPLE__)

# include <libkern/OSByteOrder.h>

# define htobe16(x) OSSwapHostToBigInt16(x)
# define htole16(x) OSSwapHostToLittleInt16(x)
# define be16toh(x) OSSwapBigToHostInt16(x)
# define le16toh(x) OSSwapLittleToHostInt16(x)

# define htobe32(x) OSSwapHostToBigInt32(x)
# define htole32(x) OSSwapHostToLittleInt32(x)
# define be32toh(x) OSSwapBigToHostInt32(x)
# define le32toh(x) OSSwapLittleToHostInt32(x)

# define htobe64(x) OSSwapHostToBigInt64(x)
# define htole64(x) OSSwapHostToLittleInt64(x)
# define be64toh(x) OSSwapBigToHostInt64(x)
# define le64toh(x) OSSwapLittleToHostInt64(x)

# define __BYTE_ORDER    BYTE_ORDER
# define __BIG_ENDIAN    BIG_ENDIAN
# define __LITTLE_ENDIAN LITTLE_ENDIAN
# define __PDP_ENDIAN    PDP_ENDIAN

#elif defined(__OpenBSD__)

# include <sys/endian.h>

#elif defined(__NetBSD__) || defined(__FreeBSD__) || defined(__DragonFly__)

# include <sys/endian.h>

# define be16toh(x) betoh16(x)
# define le16toh(x) letoh16(x)

# define be32toh(x) betoh32(x)
# define le32toh(x) letoh32(x)

# define be64toh(x) betoh64(x)
# define le64toh(x) letoh64(x)

#elif defined(__WINDOWS__)
# define NOMINMAX
# ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>

# if BYTE_ORDER == LITTLE_ENDIAN

#   if defined(_MSC_VER)
#     include <stdlib.h>
#     define htobe16(x) _byteswap_ushort(x)
#     define htole16(x) (x)
#     define be16toh(x) _byteswap_ushort(x)
#     define le16toh(x) (x)

#     define htobe32(x) _byteswap_ulong(x)
#     define htole32(x) (x)
#     define be32toh(x) _byteswap_ulong(x)
#     define le32toh(x) (x)

#     define htobe64(x) _byteswap_uint64(x)
#     define htole64(x) (x)
#     define be64toh(x) _byteswap_uint64(x)
#     define le64toh(x) (x)

#   elif defined(__GNUC__) || defined(__clang__)

#     define htobe16(x) __builtin_bswap16(x)
#     define htole16(x) (x)
#     define be16toh(x) __builtin_bswap16(x)
#     define le16toh(x) (x)

#     define htobe32(x) __builtin_bswap32(x)
#     define htole32(x) (x)
#     define be32toh(x) __builtin_bswap32(x)
#     define le32toh(x) (x)

#     define htobe64(x) __builtin_bswap64(x)
#     define htole64(x) (x)
#     define be64toh(x) __builtin_bswap64(x)
#     define le64toh(x) (x)
#   else
#     error platform not supported
#   endif

# else

#   error byte order not supported

# endif

# define __BYTE_ORDER    BYTE_ORDER
# define __BIG_ENDIAN    BIG_ENDIAN
# define __LITTLE_ENDIAN LITTLE_ENDIAN
# define __PDP_ENDIAN    PDP_ENDIAN

#else

# error platform not supported

#endif

#ifndef __GNUC__
// everything but GCC supports `__forceinline`.
#define _FORCE_INLINE __forceinline
#else
#define _FORCE_INLINE inline __attribute__((always_inline))
#endif

namespace Tailslide {
uint16_t _FORCE_INLINE tail_htole(uint16_t val) { return htole16(val); }
uint32_t _FORCE_INLINE tail_htole(uint32_t val) { return htole32(val); }
uint64_t _FORCE_INLINE tail_htole(uint64_t val) { return htole64(val); }

uint16_t _FORCE_INLINE tail_htobe(uint16_t val) { return htobe16(val); }
uint32_t _FORCE_INLINE tail_htobe(uint32_t val) { return htobe32(val); }
uint64_t _FORCE_INLINE tail_htobe(uint64_t val) { return htobe64(val); }

uint16_t _FORCE_INLINE tail_betoh(uint16_t val) { return be16toh(val); }
uint32_t _FORCE_INLINE tail_betoh(uint32_t val) { return be32toh(val); }
uint64_t _FORCE_INLINE tail_betoh(uint64_t val) { return be64toh(val); }

uint16_t _FORCE_INLINE tail_letoh(uint16_t val) { return le16toh(val); }
uint32_t _FORCE_INLINE tail_letoh(uint32_t val) { return le32toh(val); }
uint64_t _FORCE_INLINE tail_letoh(uint64_t val) { return le64toh(val); }
}

#undef _FORCE_INLINE
