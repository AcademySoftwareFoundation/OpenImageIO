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

#include <boost/version.hpp>

#define OIIO_HAVE_BOOST_UNORDERED_MAP
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#include "export.h"
#include "oiioversion.h"
#include "fmath.h"   /* for endian */


OIIO_NAMESPACE_ENTER {

namespace xxhash {

// xxhash:  http://code.google.com/p/xxhash/
// It's BSD licensed.

// Calculate the 32-bits hash of "input", of length "len"
// "seed" can be used to alter the result
unsigned int OIIO_API XXH_fast32 (const void* input, int len,
                                  unsigned int seed=1771);

// Same as XXH_fast(), but the resulting hash has stronger properties
unsigned int OIIO_API XXH_strong32 (const void* input, int len,
                                    unsigned int seed=1771);
}   // end namespace xxhash




namespace bjhash {

// Bob Jenkins "lookup3" hashes:  http://burtleburtle.net/bob/c/lookup3.c
// It's in the public domain.

inline uint32_t rotl32 (uint32_t x, int k) { return (x<<k) | (x>>(32-k)); }
inline uint64_t rotl64 (uint64_t x, int k) { return (x<<k) | (x>>(64-k)); }

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
    /// Append more data from a vector, without thinking about sizes.
    template<class T> void appendvec (const std::vector<T> &v) {
        append (&v[0], v.size()*sizeof(T));
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


} OIIO_NAMESPACE_EXIT

#endif // OPENIMAGEIO_HASH_H
