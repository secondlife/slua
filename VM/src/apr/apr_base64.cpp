/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* base64 encoder/decoder. Originally part of main/util.c
 * but moved here so that support/ab and apr_sha1.c could
 * use it. This meant removing the apr_palloc()s and adding
 * ugly 'len' functions, which is quite a nasty cost.
 */

// ServerLua: These are included from APR's source code to ensure conformance
//   with SL's existing Base64 (en/de)code logic without pulling in all of APR.
//   Slight modifications are included to make things work well without the rest
//   of APR.

#include "apr_base64.h"

#include <stdlib.h>
#include <cassert>
#define APR__ASSERT(cond) assert(cond)

/* Above APR_BASE64_ENCODE_MAX length the encoding can't fit in an int >= 0 */
#define APR_BASE64_ENCODE_MAX 1610612733

/* Above APR_BASE64_DECODE_MAX length the decoding can't fit in an int >= 0 */
#define APR_BASE64_DECODE_MAX 2863311524u

typedef size_t apr_size_t;

/* aaaack but it's fast and const should make it shared text page. */
static const unsigned char pr2six[256] =
    {
        /* ASCII table */
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
        64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
        64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
};

int apr_base64_decode_len(const char *bufcoded)
{
    const unsigned char *bufin;
    apr_size_t nprbytes;

    bufin = (const unsigned char *) bufcoded;
    while (pr2six[*(bufin++)] <= 63);
    nprbytes = (bufin - (const unsigned char *) bufcoded) - 1;
    APR__ASSERT(nprbytes <= APR_BASE64_DECODE_MAX);

    return (int)(((nprbytes + 3u) / 4u) * 3u + 1u);
}

/* This is the same as apr_base64_decode() except:
 * - no \0 is appended
 * - on EBCDIC machines, the conversion of the output to ebcdic is left out
 */
int apr_base64_decode_binary(unsigned char *bufplain,
                             const char *bufcoded)
{
    int nbytesdecoded;
    const unsigned char *bufin;
    unsigned char *bufout;
    apr_size_t nprbytes;

    bufin = (const unsigned char *) bufcoded;
    while (pr2six[*(bufin++)] <= 63);
    nprbytes = (bufin - (const unsigned char *) bufcoded) - 1;
    APR__ASSERT(nprbytes <= APR_BASE64_DECODE_MAX);
    nbytesdecoded = (int)(((nprbytes + 3u) / 4u) * 3u);

    bufout = (unsigned char *) bufplain;
    bufin = (const unsigned char *) bufcoded;

    while (nprbytes >= 4) {
        *(bufout++) =
            (unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
        *(bufout++) =
            (unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
        *(bufout++) =
            (unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
        bufin += 4;
        nprbytes -= 4;
    }

    /* Note: (nprbytes == 1) would be an error, so just ignore that case */
    if (nprbytes > 1) {
        *(bufout++) =
            (unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
    }
    if (nprbytes > 2) {
        *(bufout++) =
            (unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
    }

    return nbytesdecoded - (int)((4u - nprbytes) & 3u);
}

static const char basis_64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int apr_base64_encode_len(int len)
{
    APR__ASSERT(len >= 0 && len <= APR_BASE64_ENCODE_MAX);

    return ((len + 2) / 3 * 4) + 1;
}

/* This is the same as apr_base64_encode() except on EBCDIC machines, where
 * the conversion of the input to ascii is left out.
 */
int apr_base64_encode_binary(char *encoded,
                             const unsigned char *string, int len)
{
    int i;
    char *p;

    APR__ASSERT(len >= 0 && len <= APR_BASE64_ENCODE_MAX);

    p = encoded;
    for (i = 0; i < len - 2; i += 3) {
        *p++ = basis_64[(string[i] >> 2) & 0x3F];
        *p++ = basis_64[((string[i] & 0x3) << 4) |
                        ((int) (string[i + 1] & 0xF0) >> 4)];
        *p++ = basis_64[((string[i + 1] & 0xF) << 2) |
                        ((int) (string[i + 2] & 0xC0) >> 6)];
        *p++ = basis_64[string[i + 2] & 0x3F];
    }
    if (i < len) {
        *p++ = basis_64[(string[i] >> 2) & 0x3F];
        if (i == (len - 1)) {
            *p++ = basis_64[((string[i] & 0x3) << 4)];
            *p++ = '=';
        }
        else {
            *p++ = basis_64[((string[i] & 0x3) << 4) |
                            ((int) (string[i + 1] & 0xF0) >> 4)];
            *p++ = basis_64[((string[i + 1] & 0xF) << 2)];
        }
        *p++ = '=';
    }

    *p++ = '\0';
    return (unsigned int)(p - encoded);
}
