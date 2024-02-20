// Copyright (c) 2014 Google, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// FarmHash, by Geoff Pike

// Additional modifications for OpenImageIO:
// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// clang-format off

// https://github.com/google/farmhash

#include <OpenImageIO/detail/farmhash.h>

// namespace NAMESPACE_FOR_HASH_FUNCTIONS {

  OIIO_NAMESPACE_BEGIN

    namespace farmhash {

// BASIC STRING HASHING

// Hash function for a byte array.  See also Hash(), below.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
uint32_t Hash32(const char* s, size_t len) {
  return farmhash::inlined::Hash32(s, len);
}

// Hash function for a byte array.  For convenience, a 32-bit seed is also
// hashed into the result.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
uint32_t Hash32WithSeed(const char* s, size_t len, uint32_t seed) {
  return farmhash::inlined::Hash32WithSeed(s, len, seed);
}

// Hash function for a byte array.  For convenience, a 64-bit seed is also
// hashed into the result.  See also Hash(), below.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
uint64_t Hash64(const char* s, size_t len) {
  return farmhash::inlined::Hash64(s, len);
}

// Hash function for a byte array.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
size_t Hash(const char* s, size_t len) {
  return farmhash::inlined::Hash(s, len);
}

// Hash function for a byte array.  For convenience, a 64-bit seed is also
// hashed into the result.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
uint64_t Hash64WithSeed(const char* s, size_t len, uint64_t seed) {
  return farmhash::inlined::Hash64WithSeed(s, len, seed);
}

// Hash function for a byte array.  For convenience, two seeds are also
// hashed into the result.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
uint64_t Hash64WithSeeds(const char* s, size_t len, uint64_t seed0, uint64_t seed1) {
  return farmhash::inlined::Hash64WithSeeds(s, len, seed0, seed1);
}

// Hash function for a byte array.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
uint128_t Hash128(const char* s, size_t len) {
  return farmhash::inlined::Hash128(s, len);
}

// Hash function for a byte array.  For convenience, a 128-bit seed is also
// hashed into the result.
// May change from time to time, may differ on different platforms, may differ
// depending on NDEBUG.
uint128_t Hash128WithSeed(const char* s, size_t len, uint128_t seed) {
  return farmhash::inlined::Hash128WithSeed(s, len, seed);
}

// BASIC NON-STRING HASHING

// FINGERPRINTING (i.e., good, portable, forever-fixed hash functions)

// Fingerprint function for a byte array.  Most useful in 32-bit binaries.
uint32_t Fingerprint32(const char* s, size_t len) {
  return farmhash::inlined::Fingerprint32(s, len);
}

// Fingerprint function for a byte array.
uint64_t Fingerprint64(const char* s, size_t len) {
  return farmhash::inlined::Fingerprint64(s, len);
}

// Fingerprint function for a byte array.
uint128_t Fingerprint128(const char* s, size_t len) {
  return farmhash::inlined::Fingerprint128(s, len);
}

// Older and still available but perhaps not as fast as the above:
//   farmhashns::Hash32{,WithSeed}()

// }  // namespace NAMESPACE_FOR_HASH_FUNCTIONS
} /*end namespace farmhash*/ OIIO_NAMESPACE_END
