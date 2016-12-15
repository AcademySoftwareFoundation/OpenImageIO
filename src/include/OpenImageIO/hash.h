/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/


/// \file
///
/// Wrapper so that hash_map and hash_set mean what we want regardless
/// of the compiler.
///

#ifndef OPENIMAGEIO_HASH_H
#define OPENIMAGEIO_HASH_H

#include <vector>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>   // for memcpy and memset
#include <utility>
#include <unordered_map>

#include "export.h"
#include "oiioversion.h"
#include "fmath.h"   /* for endian */
#include "string_view.h"
#include "array_view.h"



OIIO_NAMESPACE_BEGIN

using std::unordered_map;
using std::hash;


namespace xxhash {

// xxhash:  http://code.google.com/p/xxhash/
// It's BSD licensed.

unsigned int       OIIO_API XXH32 (const void* input, size_t length,
                                   unsigned seed=1771);
unsigned long long OIIO_API XXH64 (const void* input, size_t length,
                                   unsigned long long seed=1771);

inline size_t xxhash (const void* input, size_t length, size_t seed=1771)
{
    return size_t (XXH64 (input, length, (unsigned long long)seed));
}

template <typename Str>
inline size_t xxhash (const Str& s, size_t seed=1771) {
    assert(sizeof(s[0]) == 1);
    return xxhash (s.data(), s.length(), seed);
}

}   // end namespace xxhash




namespace bjhash {

// Bob Jenkins "lookup3" hashes:  http://burtleburtle.net/bob/c/lookup3.c
// It's in the public domain.

// Mix up the bits of a, b, and c (changing their values in place).
inline void bjmix (uint32_t &a, uint32_t &b, uint32_t &c)
{
    a -= c;  a ^= rotl32(c, 4);  c += b;
    b -= a;  b ^= rotl32(a, 6);  a += c;
    c -= b;  c ^= rotl32(b, 8);  b += a;
    a -= c;  a ^= rotl32(c,16);  c += b;
    b -= a;  b ^= rotl32(a,19);  a += c;
    c -= b;  c ^= rotl32(b, 4);  b += a;
}

// Mix up and combine the bits of a, b, and c (doesn't change them, but
// returns a hash of those three original values).  21 ops
inline uint32_t bjfinal (uint32_t a, uint32_t b, uint32_t c=0xdeadbeef)
{
    c ^= b; c -= rotl32(b,14);
    a ^= c; a -= rotl32(c,11);
    b ^= a; b -= rotl32(a,25);
    c ^= b; c -= rotl32(b,16);
    a ^= c; a -= rotl32(c,4);
    b ^= a; b -= rotl32(a,14);
    c ^= b; c -= rotl32(b,24);
    return c;
}

// Mix up 4 64-bit inputs (non-destructively), and return a 64 bit hash.
// Adapted from http://burtleburtle.net/bob/c/SpookyV2.h  33 ops
inline uint64_t bjfinal64 (uint64_t h0, uint64_t h1, uint64_t h2, uint64_t h3)
{
    h3 ^= h2;  h2 = rotl64(h2,15);  h3 += h2;
    h0 ^= h3;  h3 = rotl64(h3,52);  h0 += h3;
    h1 ^= h0;  h0 = rotl64(h0,26);  h1 += h0;
    h2 ^= h1;  h1 = rotl64(h1,51);  h2 += h1;
    h3 ^= h2;  h2 = rotl64(h2,28);  h3 += h2;
    h0 ^= h3;  h3 = rotl64(h3,9);   h0 += h3;
    h1 ^= h0;  h0 = rotl64(h0,47);  h1 += h0;
    h2 ^= h1;  h1 = rotl64(h1,54);  h2 += h1;
    h3 ^= h2;  h2 = rotl64(h2,32);  h3 += h2;
    h0 ^= h3;  h3 = rotl64(h3,25);  h0 += h3;
    h1 ^= h0;  h0 = rotl64(h0,63);  h1 += h0;
    return h1;
}

// Standard "lookup3" hash, arbitrary length in bytes.
uint32_t OIIO_API hashlittle (const void *key, size_t length,
                              uint32_t seed=1771);

// Hash an array of 32 bit words -- faster than hashlittle if you know
// it's a whole number of 4-byte words.
uint32_t OIIO_API hashword (const uint32_t *key, size_t nwords,
                            uint32_t seed=1771);

// Hash a string without pre-known length.  We use the Jenkins
// one-at-a-time hash (http://en.wikipedia.org/wiki/Jenkins_hash_function),
// which seems to be a good speed/quality/requirements compromise.
inline size_t
strhash (const char *s)
{
    if (! s) return 0;
    unsigned int h = 0;
    while (*s) {
        h += (unsigned char)(*s);
        h += h << 10;
        h ^= h >> 6;
        ++s;
    }
    h += h << 3;
    h ^= h >> 11;
    h += h << 15;
    return h;
}


// Hash a string_view.  We use the Jenkins
// one-at-a-time hash (http://en.wikipedia.org/wiki/Jenkins_hash_function),
// which seems to be a good speed/quality/requirements compromise.
inline size_t
strhash (string_view s)
{
    size_t len = s.length();
    if (! len) return 0;
    unsigned int h = 0;
    for (size_t i = 0;  i < len;  ++i) {
        h += (unsigned char)(s[i]);
        h += h << 10;
        h ^= h >> 6;
    }
    h += h << 3;
    h ^= h >> 11;
    h += h << 15;
    return h;
}

}   // end namespace bjhash


namespace murmur {

// These functions were lifted from the public domain Murmurhash3.  We
// don't bother using Murmurhash -- in my tests, it was slower than
// xxhash in all cases, and comparable to bjhash.  But these two fmix
// functions are useful for scrambling the bits of a single 32 or 64 bit
// value.

inline uint32_t fmix (uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

inline uint64_t fmix (uint64_t k)
{
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}

}  // end namespace murmur



namespace farmhash {

// Copyright (c) 2014 Google, Inc.
// http://code.google.com/p/farmhash/
// See OpenImageIO's hashes.cpp for the MIT license for this code.


#if defined(FARMHASH_UINT128_T_DEFINED)
inline uint64_t Uint128Low64(const uint128_t x) {
  return static_cast<uint64_t>(x);
}
inline uint64_t Uint128High64(const uint128_t x) {
  return static_cast<uint64_t>(x >> 64);
}
inline uint128_t Uint128(uint64_t lo, uint64_t hi) {
  return lo + (((uint128_t)hi) << 64);
}
#else
typedef std::pair<uint64_t, uint64_t> uint128_t;
inline uint64_t Uint128Low64(const uint128_t x) { return x.first; }
inline uint64_t Uint128High64(const uint128_t x) { return x.second; }
inline uint128_t Uint128(uint64_t lo, uint64_t hi) { return uint128_t(lo, hi); }
#endif


// BASIC STRING HASHING

// Hash function for a byte array.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
size_t OIIO_API Hash(const char* s, size_t len);

// Hash function for a byte array.  Most useful in 32-bit binaries.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
uint32_t OIIO_API Hash32(const char* s, size_t len);

// Hash function for a byte array.  For convenience, a 32-bit seed is also
// hashed into the result.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
uint32_t OIIO_API Hash32WithSeed(const char* s, size_t len, uint32_t seed);

// Hash 128 input bits down to 64 bits of output.
// Hash function for a byte array.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
uint64_t OIIO_API Hash64(const char* s, size_t len);

// Hash function for a byte array.  For convenience, a 64-bit seed is also
// hashed into the result.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
uint64_t OIIO_API Hash64WithSeed(const char* s, size_t len, uint64_t seed);

// Hash function for a byte array.  For convenience, two seeds are also
// hashed into the result.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
uint64_t OIIO_API Hash64WithSeeds(const char* s, size_t len,
                       uint64_t seed0, uint64_t seed1);

// Hash function for a byte array.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
uint128_t OIIO_API Hash128(const char* s, size_t len);

// Hash function for a byte array.  For convenience, a 128-bit seed is also
// hashed into the result.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
uint128_t OIIO_API Hash128WithSeed(const char* s, size_t len, uint128_t seed);

// BASIC NON-STRING HASHING

// This is intended to be a reasonably good hash function.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
inline uint64_t Hash128to64(uint128_t x) {
  // Murmur-inspired hashing.
  const uint64_t kMul = 0x9ddfea08eb382d69ULL;
  uint64_t a = (Uint128Low64(x) ^ Uint128High64(x)) * kMul;
  a ^= (a >> 47);
  uint64_t b = (Uint128High64(x) ^ a) * kMul;
  b ^= (b >> 47);
  b *= kMul;
  return b;
}

// FINGERPRINTING (i.e., good, portable, forever-fixed hash functions)

// Fingerprint function for a byte array.  Most useful in 32-bit binaries.
uint32_t OIIO_API Fingerprint32(const char* s, size_t len);

// Fingerprint function for a byte array.
uint64_t OIIO_API Fingerprint64(const char* s, size_t len);

// Fingerprint function for a byte array.
uint128_t OIIO_API Fingerprint128(const char* s, size_t len);

// This is intended to be a good fingerprinting primitive.
// See below for more overloads.
inline uint64_t Fingerprint(uint128_t x) {
  // Murmur-inspired hashing.
  const uint64_t kMul = 0x9ddfea08eb382d69ULL;
  uint64_t a = (Uint128Low64(x) ^ Uint128High64(x)) * kMul;
  a ^= (a >> 47);
  uint64_t b = (Uint128High64(x) ^ a) * kMul;
  b ^= (b >> 44);
  b *= kMul;
  b ^= (b >> 41);
  b *= kMul;
  return b;
}

// This is intended to be a good fingerprinting primitive.
inline uint64_t Fingerprint(uint64_t x) {
  // Murmur-inspired hashing.
  const uint64_t kMul = 0x9ddfea08eb382d69ULL;
  uint64_t b = x * kMul;
  b ^= (b >> 44);
  b *= kMul;
  b ^= (b >> 41);
  b *= kMul;
  return b;
}

#ifndef FARMHASH_NO_CXX_STRING

// Convenience functions to hash or fingerprint C++ strings.
// These require that Str::data() return a pointer to the first char
// (as a const char*) and that Str::length() return the string's length;
// they work with std::string, for example.

// Hash function for a byte array.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
template <typename Str>
inline size_t Hash(const Str& s) {
  assert(sizeof(s[0]) == 1);
  return Hash(s.data(), s.length());
}

// Hash function for a byte array.  Most useful in 32-bit binaries.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
template <typename Str>
inline uint32_t Hash32(const Str& s) {
  assert(sizeof(s[0]) == 1);
  return Hash32(s.data(), s.length());
}

// Hash function for a byte array.  For convenience, a 32-bit seed is also
// hashed into the result.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
template <typename Str>
inline uint32_t Hash32WithSeed(const Str& s, uint32_t seed) {
  assert(sizeof(s[0]) == 1);
  return Hash32WithSeed(s.data(), s.length(), seed);
}

// Hash 128 input bits down to 64 bits of output.
// Hash function for a byte array.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
template <typename Str>
inline uint64_t Hash64(const Str& s) {
  assert(sizeof(s[0]) == 1);
  return Hash64(s.data(), s.length());
}

// Hash function for a byte array.  For convenience, a 64-bit seed is also
// hashed into the result.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
template <typename Str>
inline uint64_t Hash64WithSeed(const Str& s, uint64_t seed) {
  assert(sizeof(s[0]) == 1);
  return Hash64WithSeed(s.data(), s.length(), seed);
}

// Hash function for a byte array.  For convenience, two seeds are also
// hashed into the result.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
template <typename Str>
inline uint64_t Hash64WithSeeds(const Str& s, uint64_t seed0, uint64_t seed1) {
  assert(sizeof(s[0]) == 1);
  return Hash64WithSeeds(s.data(), s.length(), seed0, seed1);
}

// Hash function for a byte array.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
template <typename Str>
inline uint128_t Hash128(const Str& s) {
  assert(sizeof(s[0]) == 1);
  return Hash128(s.data(), s.length());
}

// Hash function for a byte array.  For convenience, a 128-bit seed is also
// hashed into the result.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
template <typename Str>
inline uint128_t Hash128WithSeed(const Str& s, uint128_t seed) {
  assert(sizeof(s[0]) == 1);
  return Hash128(s.data(), s.length(), seed);
}

// FINGERPRINTING (i.e., good, portable, forever-fixed hash functions)

// Fingerprint function for a byte array.  Most useful in 32-bit binaries.
template <typename Str>
inline uint32_t Fingerprint32(const Str& s) {
  assert(sizeof(s[0]) == 1);
  return Fingerprint32(s.data(), s.length());
}

// Fingerprint 128 input bits down to 64 bits of output.
// Fingerprint function for a byte array.
template <typename Str>
inline uint64_t Fingerprint64(const Str& s) {
  assert(sizeof(s[0]) == 1);
  return Fingerprint64(s.data(), s.length());
}

// Fingerprint function for a byte array.
template <typename Str>
inline uint128_t Fingerprint128(const Str& s) {
  assert(sizeof(s[0]) == 1);
  return Fingerprint128(s.data(), s.length());
}

#endif

}  // namespace farmhash




class CSHA1;  // opaque forward declaration


/// Class that encapsulates SHA-1 hashing, a crypticographic-strength
/// 160-bit hash function.  It's not as fast as our other hashing
/// methods, but has an extremely low chance of having collisions.
class OIIO_API SHA1 {
public:
    /// Create SHA1, optionally read data
    SHA1 (const void *data=NULL, size_t size=0);
    ~SHA1 ();

    /// Append more data
    void append (const void *data, size_t size);
    /// Append more data from an array_view, without thinking about sizes.
    template<class T> void append (array_view<T> v) {
        append (v.data(), v.size()*sizeof(T));
    }

    template<class T> OIIO_DEPRECATED("Use append(). [1.6]")
    void appendvec (array_view<T> v) {
        append (v.data(), v.size()*sizeof(T));
    }

    /// Type for storing the raw bits of the hash
    struct Hash {
        unsigned char hash[20];
    };

    /// Get the digest and store it in Hash h.
    void gethash (Hash &h);

    /// Get the digest and store it in h (must point to enough storage
    /// to accommodate 20 bytes).
    void gethash (void *h) { gethash (*(Hash *)h); }

    /// Return the digest as a hex string
    std::string digest ();

    /// Roll the whole thing into one functor, return the string digest.
    static std::string digest (const void *data, size_t size) {
        SHA1 s (data, size);  return s.digest();
    }

private:
    CSHA1 *m_csha1;
    bool m_final;
};


OIIO_NAMESPACE_END

#endif // OPENIMAGEIO_HASH_H
