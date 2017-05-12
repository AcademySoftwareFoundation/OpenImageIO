/*
Copyright (c) 2014 Larry Gritz et al.
All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
* Neither the name of Sony Pictures Imageworks nor the names of its
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
*/

/// @file  simd.h
///
/// @brief Classes for SIMD processing.
///
/// Nice references for all the Intel intrinsics (SSE*, AVX*, etc.):
///   https://software.intel.com/sites/landingpage/IntrinsicsGuide/
///
/// It helped me a lot to peruse the source of these packages:
///   Syrah:     https://github.com/boulos/syrah
///   Embree:    https://github.com/embree
///   Vectorial: https://github.com/scoopr/vectorial
///
/// To find out which CPU features you have:
///   Linux: cat /proc/cpuinfo
///   OSX:   sysctl machdep.cpu.features
///
/// Additional web resources:
///   http://www.codersnotes.com/notes/maths-lib-2016/


#pragma once
#ifndef OIIO_SIMD_H
#define OIIO_SIMD_H 1

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/missing_math.h>
#include <OpenImageIO/platform.h>
#include <OpenEXR/ImathVec.h>
#include <OpenEXR/ImathMatrix.h>
#include <algorithm>
#include <cstring>


//////////////////////////////////////////////////////////////////////////
// Sort out which SIMD capabilities we have and set definitions
// appropriately. This is mostly for internal (within this file) use,
// but client applications using this header may find a few of the macros
// we define to be useful:
//
// OIIO_SIMD : Will be 0 if no hardware SIMD support is specified. If SIMD
//             hardware is available, this will hold the width in number of
//             float SIMD "lanes" of widest SIMD registers available. For
//             example, OIIO_SIMD will be 4 if float4/int4/bool4 are
//             hardware accelerated, 8 if float8/int8/bool8 are accelerated,
//             etc. Using SIMD classes wider than this should work (will be
//             emulated with narrower SIMD or scalar operations), but is not
//             expected to have high performance.
// OIIO_SIMD_SSE : if Intel SSE is supported, this will be nonzero,
//             specifically 2 for SSE2, 3 for SSSE3, 4 for SSE4.1 or
//             higher (including AVX).
// OIIO_SIMD_AVX : If Intel AVX is supported, this will be nonzero, and
//             specifically 1 for AVX (1.0), 2 for AVX2, 512 for AVX512f.
// OIIO_SIMD_NEON : If ARM NEON is supported, this will be nonzero.
// OIIO_SIMD_MAX_SIZE : holds the width in bytes of the widest SIMD
//             available (generally will be OIIO_SIMD*4).
// OIIO_SIMD4_ALIGN : macro for best alignment of 4-wide SIMD values in mem.
// OIIO_SIMD8_ALIGN : macro for best alignment of 8-wide SIMD values in mem.
// OIIO_SIMD_HAS_MATRIX4 : nonzero if matrix44 is defined
// OIIO_SIMD_HAS_FLOAT8 : nonzero if float8, int8, bool8 are defined

#if (defined(__SSE2__) || (_MSC_VER >= 1300 && !_M_CEE_PURE)) && !defined(OIIO_NO_SSE)
#  include <immintrin.h>
#  if (defined(__i386__) || defined(__x86_64__)) && (OIIO_GNUC_VERSION > 40400 || __clang__)
#    include <x86intrin.h>
#  endif
#  if (defined(__SSE4_1__) || defined(__SSE4_2__))
#    define OIIO_SIMD_SSE 4
      /* N.B. We consider both SSE4.1 and SSE4.2 to be "4". There are a few
       * instructions specific to 4.2, but they are all related to string
       * comparisons and CRCs, which don't currently seem relevant to OIIO,
       * so for simplicity, we sweep this difference under the rug.
       */
#  elif defined(__SSSE3__)
#    define OIIO_SIMD_SSE 3
     /* N.B. We only use OIIO_SIMD_SSE = 3 when fully at SSSE3. In theory,
      * there are a few older architectures that are SSE3 but not SSSE3,
      * and this simplification means that these particular old platforms
      * will only get SSE2 goodness out of our code. So be it. Anybody who
      * cares about performance is probably using a 64 bit machine that's
      * SSE 4.x or AVX by now.
      */
#  else
#    define OIIO_SIMD_SSE 2
#  endif
#  define OIIO_SIMD 4
#  define OIIO_SIMD_MAX_SIZE_BYTES 16
#  define OIIO_SIMD_ALIGN OIIO_ALIGN(16)
#  define OIIO_SIMD4_ALIGN OIIO_ALIGN(16)
#  define OIIO_SSE_ALIGN OIIO_ALIGN(16)
#endif

#if defined(__AVX__) && !defined(OIIO_NO_AVX)
   // N.B. Any machine with AVX will also have SSE
#  if defined(__AVX2__) && !defined(OIIO_NO_AVX2)
#    define OIIO_SIMD_AVX 2
#  else
#    define OIIO_SIMD_AVX 1
#  endif
#  undef OIIO_SIMD
#  define OIIO_SIMD 8
#  undef OIIO_SIMD_MAX_SIZE_BYTES
#  define OIIO_SIMD_MAX_SIZE_BYTES 32
#  define OIIO_SIMD8_ALIGN OIIO_ALIGN(32)
#  define OIIO_AVX_ALIGN OIIO_ALIGN(32)
#  if defined(__AVX512__) && !defined(OIIO_NO_AVX512)
#    define OIIO_SIMD_AVX 512
#    undef OIIO_SIMD_MAX_SIZE_BYTES
#    define OIIO_SIMD_MAX_SIZE_BYTES 64
#    define OIIO_SIMD16_ALIGN OIIO_ALIGN(64)
#    define OIIO_AVX512_ALIGN OIIO_ALIGN(64)
#  endif
#endif

#if defined(__FMA__)
#  define OIIO_FMA_ENABLED 1
#endif

// FIXME Future: support ARM Neon
// Uncomment this when somebody with Neon can verify it works
#if 0 && defined(__ARM_NEON__) && !defined(OIIO_NO_NEON)
#  include <arm_neon.h>
#  define OIIO_SIMD 1
#  define OIIO_SIMD_NEON 1
#  define OIIO_SIMD_MAX_SIZE_BYTES 16
#  define OIIO_SIMD_ALIGN OIIO_ALIGN(16)
#  define OIIO_SIMD4_ALIGN OIIO_ALIGN(16)
#  define OIIO_SSE_ALIGN OIIO_ALIGN(16)
#endif

#ifndef OIIO_SIMD
   // No SIMD available
#  define OIIO_SIMD 0
#  define OIIO_SIMD_ALIGN
#  define OIIO_SIMD4_ALIGN
#  define OIIO_SIMD8_ALIGN
#  define OIIO_SIMD_MAX_SIZE_BYTES 16
#endif

// General features that client apps may want to test for, for conditional
// compilation. Will add to this over time as needed. Note that just
// because a feature is present doesn't mean it's fast -- HAS_FLOAT8 means
// the float8 class (and friends) are in this version of simd.h, but that's
// different from OIIO_SIMD >= 8, which means it's supported in hardware.
#define OIIO_SIMD_HAS_MATRIX4 1  /* matrix44 defined */
#define OIIO_SIMD_HAS_FLOAT8 1   /* float8, int8, bool8 defined */




OIIO_NAMESPACE_BEGIN

namespace simd {

//////////////////////////////////////////////////////////////////////////
// Forward declarations of our main SIMD classes

class bool4;
class int4;
class float4;
class float3;
class matrix44;
typedef bool4 mask4;    // old name
class bool8;
class int8;
class float8;


//////////////////////////////////////////////////////////////////////////
// Template magic to determine the raw SIMD types involved, and other
// things helpful for metaprogramming.

template <typename T, int N> struct simd_raw_t { struct type { T val[N]; }; };
template <int N> struct simd_bool_t { struct type { int val[N]; }; };

#if OIIO_SIMD_SSE
template<> struct simd_raw_t<int,4> { typedef __m128i type; };
template<> struct simd_raw_t<float,4> { typedef __m128 type; };
template<> struct simd_bool_t<4> { typedef __m128 type; };
#endif

#if OIIO_SIMD_AVX
template<> struct simd_raw_t<int,8> { typedef __m256i type; };
template<> struct simd_raw_t<float,8> { typedef __m256 type; };
template<> struct simd_bool_t<8> { typedef __m256 type; };
#endif

#if OIIO_SIMD_AVX >= 512
template<> struct simd_raw_t<int,16> { typedef __m512i type; };
template<> struct simd_raw_t<float,16> { typedef __m512 type; };
template<> struct simd_bool_t<16> { typedef __m512 type; };
#endif

#if OIIO_SIMD_NEON
template<> struct simd_raw_t<int,4> { typedef int32x4 type; };
template<> struct simd_raw_t<float,4> { typedef float32x4_t type; };
template<> struct simd_bool_t<4> { typedef int32x4 type; };
#endif


/// Template to retrieve the vector type from the scalar. For example,
/// simd::VecType<int,4> will be float4.
template<typename T,int elements> struct VecType {};
template<> struct VecType<int,1>   { typedef int type; };
template<> struct VecType<float,1> { typedef float type; };
template<> struct VecType<int,4>   { typedef int4 type; };
template<> struct VecType<float,3> { typedef float3 type; };
template<> struct VecType<bool,4>  { typedef bool4 type; };
template<> struct VecType<int,8>   { typedef int8 type; };
template<> struct VecType<float,8> { typedef float8 type; };
template<> struct VecType<bool,8>  { typedef bool8 type; };

/// Template to retrieve the SIMD size of a SIMD type. Rigged to be 1 for
/// anything but our SIMD types.
template<typename T> struct SimdSize { static const int size = 1; };
template<> struct SimdSize<int4>     { static const int size = 4; };
template<> struct SimdSize<float4>   { static const int size = 4; };
template<> struct SimdSize<float3>   { static const int size = 4; };
template<> struct SimdSize<bool4>    { static const int size = 4; };
template<> struct SimdSize<int8>     { static const int size = 8; };
template<> struct SimdSize<float8>   { static const int size = 8; };
template<> struct SimdSize<bool8>    { static const int size = 8; };

/// Template to retrieve the number of elements size of a SIMD type. Rigged
/// to be 1 for anything but our SIMD types.
template<typename T> struct SimdElements { static const int size = SimdSize<T>::size; };
template<> struct SimdElements<float3>   { static const int size = 3; };

/// Template giving a printable name for each type
template<typename T> struct SimdTypeName { static const char *name() { return "unknown"; } };
template<> struct SimdTypeName<float4>   { static const char *name() { return "float4"; } };
template<> struct SimdTypeName<int4>     { static const char *name() { return "int4"; } };
template<> struct SimdTypeName<bool4>    { static const char *name() { return "bool4"; } };
template<> struct SimdTypeName<float8>   { static const char *name() { return "float8"; } };
template<> struct SimdTypeName<int8>     { static const char *name() { return "int8"; } };
template<> struct SimdTypeName<bool8>    { static const char *name() { return "bool8"; } };


//////////////////////////////////////////////////////////////////////////
// Macros helpful for making static constants in code.

# define OIIO_SIMD_FLOAT4_CONST(name,val) \
    static const OIIO_SIMD4_ALIGN float name[4] = { (val), (val), (val), (val) }
# define OIIO_SIMD_FLOAT4_CONST4(name,v0,v1,v2,v3) \
    static const OIIO_SIMD4_ALIGN float name[4] = { (v0), (v1), (v2), (v3) }
# define OIIO_SIMD_INT4_CONST(name,val) \
    static const OIIO_SIMD4_ALIGN int name[4] = { (val), (val), (val), (val) }
# define OIIO_SIMD_INT4_CONST4(name,v0,v1,v2,v3) \
    static const OIIO_SIMD4_ALIGN int name[4] = { (v0), (v1), (v2), (v3) }
# define OIIO_SIMD_UINT4_CONST(name,val) \
    static const OIIO_SIMD4_ALIGN uint32_t name[4] = { (val), (val), (val), (val) }
# define OIIO_SIMD_UINT4_CONST4(name,v0,v1,v2,v3) \
    static const OIIO_SIMD4_ALIGN uint32_t name[4] = { (v0), (v1), (v2), (v3) }

# define OIIO_SIMD_FLOAT8_CONST(name,val) \
    static const OIIO_SIMD8_ALIGN float name[8] = { (val), (val), (val), (val), \
                                                    (val), (val), (val), (val) }
# define OIIO_SIMD_FLOAT8_CONST8(name,v0,v1,v2,v3,v4,v5,v6,v7) \
    static const OIIO_SIMD8_ALIGN float name[8] = { (v0), (v1), (v2), (v3), \
                                                    (v4), (v5), (v6), (v7) }
# define OIIO_SIMD_INT8_CONST(name,val) \
    static const OIIO_SIMD8_ALIGN int name[8] = { (val), (val), (val), (val), \
                                                  (val), (val), (val), (val) }
# define OIIO_SIMD_INT8_CONST8(name,v0,v1,v2,v3,v4,v5,v6,v7) \
    static const OIIO_SIMD8_ALIGN int name[8] = { (v0), (v1), (v2), (v3), \
                                                  (v4), (v5), (v6), (v7) }
# define OIIO_SIMD_UINT8_CONST(name,val) \
    static const OIIO_SIMD8_ALIGN uint32_t name[8] = { (val), (val), (val), (val), \
                                                       (val), (val), (val), (val) }
# define OIIO_SIMD_UINT8_CONST8(name,v0,v1,v2,v3,v4,v5,v6,v7) \
    static const OIIO_SIMD8_ALIGN uint32_t name[8] = { (v0), (v1), (v2), (v3), \
                                                       (v4), (v5), (v6), (v7) }



//////////////////////////////////////////////////////////////////////////
// Some macros just for use in this file (#undef-ed at the end) making
// it more succinct to express per-element operations.

#define SIMD_DO(x) for (int i = 0; i < elements; ++i) x
#define SIMD_CONSTRUCT(x) for (int i = 0; i < elements; ++i) m_val[i] = (x)
#define SIMD_CONSTRUCT_PAD(x) for (int i = 0; i < elements; ++i) m_val[i] = (x); \
                              for (int i = elements; i < paddedelements; ++i) m_val[i] = 0
#define SIMD_RETURN(T,x) T r; for (int i = 0; i < r.elements; ++i) r[i] = (x); return r
#define SIMD_RETURN_REDUCE(T,init,op) T r = init; for (int i = 0; i < v.elements; ++i) op; return r



//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// The public declarations of the main SIMD classes follow: boolN, intN,
// floatN, matrix44.
//
// These class declarations are intended to be brief and self-documenting,
// and give all the information that users or client applications need to
// know to use these classes.
//
// No implementations are given inline except for the briefest, completely
// generic methods that don't have any architecture-specific overloads.
// After the class defintions, there will be an immense pile of full
// implementation definitions, which casual users are not expected to
// understand.
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


/// bool4: An 4-vector whose elements act mostly like bools, accelerated by
/// SIMD instructions when available. This is what is naturally produced by
/// SIMD comparison operators on the float4 and int4 types.
class bool4 {
public:
    static const char* type_name() { return "bool4"; }
    typedef bool value_t;        ///< Underlying equivalent scalar value type
    enum { elements = 4 };       ///< Number of scalar elements
    enum { paddedelements = 4 }; ///< Number of scalar elements for full pad
    enum { bits = elements*32 }; ///< Total number of bits
    typedef simd_bool_t<4>::type simd_t;  ///< the native SIMD type used

    /// Default constructor (contents undefined)
    bool4 () { }

    /// Construct from a single value (store it in all slots)
    bool4 (bool a) { load(a); }

    explicit bool4 (bool *a);

    /// Construct from 4 values
    bool4 (bool a, bool b, bool c, bool d) { load (a, b, c, d); }

    /// Copy construct from another bool4
    bool4 (const bool4 &other) { m_simd = other.m_simd; }

    /// Construct from a SIMD int (is each element nonzero?)
    bool4 (const int4 &i);

    /// Construct from the underlying SIMD type
    bool4 (const simd_t& m) : m_simd(m) { }

    /// Return the raw SIMD type
    operator simd_t () const { return m_simd; }
    simd_t simd () const { return m_simd; }

    /// Set all components to false
    void clear ();

    /// Return a bool4 the is 'false' for all values
    static const bool4 False ();

    /// Return a bool4 the is 'true' for all values
    static const bool4 True ();

    /// Assign one value to all components
    const bool4 & operator= (bool a) { load(a); return *this; }

    /// Assignment of another bool4
    const bool4 & operator= (const bool4 & other);

    /// Component access (get)
    int operator[] (int i) const;

    /// Component access (set).
    /// NOTE: use with caution. The implementation sets the integer
    /// value, which may not have the same bit pattern as the bool returned
    /// by operator[]const.
    int& operator[] (int i);

    /// Helper: load a single value into all components.
    void load (bool a);

    /// Helper: load separate values into each component.
    void load (bool a, bool b, bool c, bool d);

    /// Helper: store the values into memory as bools.
    void store (bool *values) const;

    /// Store the first n values into memory.
    void store (bool *values, int n) const;

    /// Logical/bitwise operators, component-by-component
    friend bool4 operator! (const bool4& a);
    friend bool4 operator& (const bool4& a, const bool4& b);
    friend bool4 operator| (const bool4& a, const bool4& b);
    friend bool4 operator^ (const bool4& a, const bool4& b);
    friend bool4 operator~ (const bool4& a);
    friend const bool4& operator&= (bool4& a, const bool4& b);
    friend const bool4& operator|= (bool4& a, const bool4& b);
    friend const bool4& operator^= (bool4& a, const bool4& b);

    /// Comparison operators, component by component
    friend bool4 operator== (const bool4& a, const bool4& b);
    friend bool4 operator!= (const bool4& a, const bool4& b);

    /// Stream output
    friend std::ostream& operator<< (std::ostream& cout, const bool4 & a);

private:
    // The actual data representation
    union {
        simd_t m_simd;
        int m_val[paddedelements];
    };
};



/// Helper: shuffle/swizzle with constant (templated) indices.
/// Example: shuffle<1,1,2,2>(bool4(a,b,c,d)) returns (b,b,c,c)
template<int i0, int i1, int i2, int i3> bool4 shuffle (const bool4& a);

/// shuffle<i>(a) is the same as shuffle<i,i,i,i>(a)
template<int i> bool4 shuffle (const bool4& a);

/// Helper: as rapid as possible extraction of one component, when the
/// index is fixed.
template<int i> bool extract (const bool4& a);

/// Helper: substitute val for a[i]
template<int i> bool4 insert (const bool4& a, bool val);

/// Logical reduction across all components.
bool reduce_and (const bool4& v);
bool reduce_or (const bool4& v);

// Are all/any/no components true?
bool all (const bool4& v);
bool any (const bool4& v);
bool none (const bool4& v);





/// bool8: An 8-vector whose elements act mostly like bools, accelerated by
/// SIMD instructions when available. This is what is naturally produced by
/// SIMD comparison operators on the float4 and int4 types.
class bool8 {
public:
    static const char* type_name() { return "bool8"; }
    typedef bool value_t;        ///< Underlying equivalent scalar value type
    enum { elements = 8 };       ///< Number of scalar elements
    enum { paddedelements = 8 }; ///< Number of scalar elements for full pad
    enum { bits = elements*32 }; ///< Total number of bits
    typedef simd_bool_t<8>::type simd_t;  ///< the native SIMD type used

    /// Default constructor (contents undefined)
    bool8 () { }

    /// Construct from a single value (store it in all slots)
    bool8 (bool a) { load (a); }

    explicit bool8 (bool *values);

    /// Construct from 8 values
    bool8 (bool a, bool b, bool c, bool d, bool e, bool f, bool g, bool h);

    /// Copy construct from another bool8
    bool8 (const bool8 &other) { m_simd = other.m_simd; }

    /// Construct from a SIMD int (is each element nonzero?)
    bool8 (const int8 &i);

    /// Construct from two bool4's
    bool8 (const bool4 &lo, const bool4 &hi);

    /// Construct from the underlying SIMD type
    bool8 (const simd_t& m) : m_simd(m) { }

    /// Return the raw SIMD type
    operator simd_t () const { return m_simd; }
    simd_t simd () const { return m_simd; }

    /// Set all components to false
    void clear ();

    /// Return a bool8 the is 'false' for all values
    static const bool8 False ();

    /// Return a bool8 the is 'true' for all values
    static const bool8 True ();

    /// Assign one value to all components
    const bool8 & operator= (bool a);

    /// Assignment of another bool8
    const bool8 & operator= (const bool8 & other);

    /// Component access (get)
    int operator[] (int i) const;

    /// Component access (set).
    /// NOTE: use with caution. The implementation sets the integer
    /// value, which may not have the same bit pattern as the bool returned
    /// by operator[]const.
    int& operator[] (int i);

    /// Extract the lower percision bool4
    bool4 lo () const;

    /// Extract the higher percision bool4
    bool4 hi () const;

    /// Helper: load a single value into all components.
    void load (bool a);

    /// Helper: load separate values into each component.
    void load (bool a, bool b, bool c, bool d,
               bool e, bool f, bool g, bool h);

    /// Helper: store the values into memory as bools.
    void store (bool *values) const;

    /// Store the first n values into memory.
    void store (bool *values, int n) const;

    /// Logical/bitwise operators, component-by-component
    friend bool4 operator! (const bool4& a);
    friend bool8 operator! (const bool8& a);
    friend bool8 operator& (const bool8& a, const bool8& b);
    friend bool8 operator| (const bool8& a, const bool8& b);
    friend bool8 operator^ (const bool8& a, const bool8& b);
    friend bool8 operator~ (const bool8& a);
    friend const bool8& operator&= (bool8& a, const bool8& b);
    friend const bool8& operator|= (bool8& a, const bool8& b);
    friend const bool8& operator^= (bool8& a, const bool8& b);

    /// Comparison operators, component by component
    friend bool8 operator== (const bool8& a, const bool8& b);
    friend bool8 operator!= (const bool8& a, const bool8& b);

    /// Stream output
    friend std::ostream& operator<< (std::ostream& cout, const bool8 & a);

private:
    // The actual data representation
    union {
        simd_t m_simd;
        int m_val[paddedelements];
        simd_bool_t<4>::type m_4[2];
    };
};



/// Helper: shuffle/swizzle with constant (templated) indices.
/// Example: shuffle<1,1,2,2>(bool4(a,b,c,d)) returns (b,b,c,c)
template<int i0, int i1, int i2, int i3, int i4, int i5, int i6, int i7>
bool8 shuffle (const bool8& a);

/// shuffle<i>(a) is the same as shuffle<i,i,i,i>(a)
template<int i> bool8 shuffle (const bool8& a);

/// Helper: as rapid as possible extraction of one component, when the
/// index is fixed.
template<int i> bool extract (const bool8& a);

/// Helper: substitute val for a[i]
template<int i> bool8 insert (const bool8& a, bool val);

/// Logical reduction across all components.
bool reduce_and (const bool8& v);
bool reduce_or (const bool8& v);

// Are all/any/no components true?
bool all (const bool8& v);
bool any (const bool8& v);
bool none (const bool8& v);





/// Integer 4-vector, accelerated by SIMD instructions when available.
class int4 {
public:
    static const char* type_name() { return "int4"; }
    typedef int value_t;      ///< Underlying equivalent scalar value type
    enum { elements = 4 };    ///< Number of scalar elements
    enum { paddedelements =4 }; ///< Number of scalar elements for full pad
    enum { bits = 128 };      ///< Total number of bits
    typedef simd_raw_t<int,elements>::type simd_t;  ///< the native SIMD type used
    typedef bool4 bool_t; ///< bool type of the same length
    typedef float4 float_t; ///< float type of the same length

    /// Default constructor (contents undefined)
    int4 () { }

    /// Construct from a single value (store it in all slots)
    int4 (int a);

    /// Construct from 2 values -- (a,a,b,b)
    int4 (int a, int b);

    /// Construct from 4 values
    int4 (int a, int b, int c, int d);

    /// Construct from a pointer to values
    int4 (const int *vals);

    /// Construct from a pointer to unsigned short values
    explicit int4 (const unsigned short *vals);

    /// Construct from a pointer to signed short values
    explicit int4 (const short *vals);

    /// Construct from a pointer to unsigned char values (0 - 255)
    explicit int4 (const unsigned char *vals);

    /// Construct from a pointer to signed char values (-128 - 127)
    explicit int4 (const char *vals);

    /// Copy construct from another int4
    int4 (const int4 & other) { m_simd = other.m_simd; }

    /// Convert a float_t to an int4. Equivalent to i = (int)f;
    explicit int4 (const float_t& f); // implementation below

    /// Construct from the underlying SIMD type
    int4 (const simd_t& m) : m_simd(m) { }

    /// Return the raw SIMD type
    operator simd_t () const { return m_simd; }
    simd_t simd () const { return m_simd; }

    /// Sset all components to 0
    void clear () ;

    /// Return an int4 with all components set to 0
    static const int4 Zero ();

    /// Return an int4 with all components set to 1
    static const int4 One ();

    /// Return an int4 with all components set to -1 (aka 0xffffffff)
    static const int4 NegOne ();

    /// Return an int4 with incremented components (e.g., 0,1,2,3).
    /// Optional argument can give a non-zero starting point.
    static const int4 Iota (int start=0, int step=1);

    /// Assign one value to all components.
    const int4 & operator= (int a);

    /// Assignment from another int4
    const int4 & operator= (const int4& other) ;

    /// Component access (set)
    int& operator[] (int i) ;

    /// Component access (get)
    int operator[] (int i) const;

    value_t x () const;
    value_t y () const;
    value_t z () const;
    value_t w () const;
    void set_x (value_t val);
    void set_y (value_t val);
    void set_z (value_t val);
    void set_w (value_t val);

    /// Helper: load a single int into all components
    void load (int a);

    /// Helper: load separate values into each component.
    void load (int a, int b, int c, int d);

    /// Load from an array of 4 values
    void load (const int *values);

    void load (const int *values, int n) ;

    /// Load from an array of 4 unsigned short values, convert to int4
    void load (const unsigned short *values) ;

    /// Load from an array of 4 unsigned short values, convert to int4
    void load (const short *values);

    /// Load from an array of 4 unsigned char values, convert to int4
    void load (const unsigned char *values);

    /// Load from an array of 4 unsigned char values, convert to int4
    void load (const char *values);

    /// Store the values into memory
    void store (int *values) const;

    /// Store the first n values into memory
    void store (int *values, int n) const;

    /// Store the least significant 16 bits of each element into adjacent
    /// unsigned shorts.
    void store (unsigned short *values) const;

    /// Store the least significant 8 bits of each element into adjacent
    /// unsigned chars.
    void store (unsigned char *values) const;

    // Arithmetic operators (component-by-component)
    friend int4 operator+ (const int4& a, const int4& b);
    friend int4 operator- (const int4& a);
    friend int4 operator- (const int4& a, const int4& b);
    friend int4 operator* (const int4& a, const int4& b);
    friend int4 operator/ (const int4& a, const int4& b);
    friend int4 operator% (const int4& a, const int4& b);
    friend const int4 & operator+= (int4& a, const int4& b);
    friend const int4 & operator-= (int4& a, const int4& b);
    friend const int4 & operator*= (int4& a, const int4& b);
    friend const int4 & operator/= (int4& a, const int4& b);
    friend const int4 & operator%= (int4& a, const int4& b);
    // Bitwise operators (component-by-component)
    friend int4 operator& (const int4& a, const int4& b);
    friend int4 operator| (const int4& a, const int4& b);
    friend int4 operator^ (const int4& a, const int4& b);
    friend const int4& operator&= (int4& a, const int4& b);
    friend const int4& operator|= (int4& a, const int4& b);
    friend const int4& operator^= (int4& a, const int4& b);
    friend int4 operator~ (const int4& a);
    friend int4 operator<< (const int4& a, unsigned int bits);
    friend int4 operator>> (const int4& a, unsigned int bits);
    friend const int4& operator<<= (int4& a, unsigned int bits);
    friend const int4& operator>>= (int4& a, unsigned int bits);
    // Comparison operators (component-by-component)
    friend bool4 operator== (const int4& a, const int4& b);
    friend bool4 operator!= (const int4& a, const int4& b);
    friend bool4 operator<  (const int4& a, const int4& b);
    friend bool4 operator>  (const int4& a, const int4& b);
    friend bool4 operator>= (const int4& a, const int4& b);
    friend bool4 operator<= (const int4& a, const int4& b);

    /// Stream output
    friend std::ostream& operator<< (std::ostream& cout, const int4 & a);

private:
    // The actual data representation
    union {
        simd_t  m_simd;
        value_t m_val[elements];
    };
};



// Shift right logical -- unsigned shift. This differs from operator>>
// in how it handles the sign bit.  (1<<31) >> 1 == (1<<31), but
// srl((1<<31),1) == 1<<30.
int4 srl (const int4& val, const unsigned int bits);

/// Helper: shuffle/swizzle with constant (templated) indices.
/// Example: shuffle<1,1,2,2>(bool4(a,b,c,d)) returns (b,b,c,c)
template<int i0, int i1, int i2, int i3> int4 shuffle (const int4& a);

/// shuffle<i>(a) is the same as shuffle<i,i,i,i>(a)
template<int i> int4 shuffle (const int4& a);

/// Helper: as rapid as possible extraction of one component, when the
/// index is fixed.
template<int i> int extract (const int4& v);

/// The sum of all components, returned in all components.
int4 vreduce_add (const int4& v);

// Reduction across all components
int reduce_add (const int4& v);
int reduce_and (const int4& v);
int reduce_or (const int4& v);

/// Use a bool mask to select between components of a (if mask[i] is false)
/// and b (if mask[i] is true), i.e., mask[i] ? b[i] : a[i].
int4 blend (const int4& a, const int4& b, const bool4& mask);

/// Use a bool mask to select between `a` (if mask[i] is true) or 0 if
/// mask[i] is false), i.e., mask[i] ? a[i] : 0. Equivalent to
/// blend(0,a,mask).
int4 blend0 (const int4& a, const bool4& mask);

/// Use a bool mask to select between components of a (if mask[i] is false)
/// or 0 (if mask[i] is true), i.e., mask[i] ? 0 : a[i]. Equivalent to
/// blend(0,a,!mask), or blend(a,0,mask).
int4 blend0not (const int4& a, const bool4& mask);

/// Select 'a' where mask is true, 'b' where mask is false. Sure, it's a
/// synonym for blend with arguments rearranged, but this is more clear
/// because the arguments are symmetric to scalar (cond ? a : b).
int4 select (const bool4& mask, const int4& a, const int4& b);

// Per-element math
int4 abs (const int4& a);
int4 min (const int4& a, const int4& b);
int4 max (const int4& a, const int4& b);

// Circular bit rotate by k bits, for N values at once.
int4 rotl32 (const int4& x, const unsigned int k);

/// andnot(a,b) returns ((~a) & b)
int4 andnot (const int4& a, const int4& b);

/// Bitcast back and forth to intN (not a convert -- move the bits!)
int4 bitcast_to_int (const bool4& x);
int4 bitcast_to_int (const float4& x);
float4 bitcast_to_float (const int4& x);

void transpose (int4 &a, int4 &b, int4 &c, int4 &d);
void transpose (const int4& a, const int4& b, const int4& c, const int4& d,
                int4 &r0, int4 &r1, int4 &r2, int4 &r3);

int4 AxBxCxDx (const int4& a, const int4& b, const int4& c, const int4& d);




/// Integer 8-vector, accelerated by SIMD instructions when available.
class int8 {
public:
    static const char* type_name() { return "int8"; }
    typedef int value_t;      ///< Underlying equivalent scalar value type
    enum { elements = 8 };    ///< Number of scalar elements
    enum { paddedelements =8 }; ///< Number of scalar elements for full pad
    enum { bits = 128 };      ///< Total number of bits
    typedef simd_raw_t<int,elements>::type simd_t;  ///< the native SIMD type used
    typedef bool8 bool_t; ///< bool type of the same length
    typedef float8 float_t; ///< float type of the same length

    /// Default constructor (contents undefined)
    int8 () { }

    /// Construct from a single value (store it in all slots)
    int8 (int a);

    /// Construct from 2 values -- (a,a,b,b)
    int8 (int a, int b);

    /// Construct from 8 values (won't work for int8)
    int8 (int a, int b, int c, int d, int e, int f, int g, int h);

    /// Construct from a pointer to values
    int8 (const int *vals);

    /// Construct from a pointer to unsigned short values
    explicit int8 (const unsigned short *vals);

    /// Construct from a pointer to signed short values
    explicit int8 (const short *vals);

    /// Construct from a pointer to unsigned char values (0 - 255)
    explicit int8 (const unsigned char *vals);

    /// Construct from a pointer to signed char values (-128 - 127)
    explicit int8 (const char *vals);

    /// Copy construct from another int8
    int8 (const int8 & other) { m_simd = other.m_simd; }

    /// Convert a float8 to an int8. Equivalent to i = (int)f;
    explicit int8 (const float8& f); // implementation below

    /// Construct from two int4's
    int8 (const int4 &lo, const int4 &hi);

    /// Construct from the underlying SIMD type
    int8 (const simd_t& m) : m_simd(m) { }

    /// Return the raw SIMD type
    operator simd_t () const { return m_simd; }
    simd_t simd () const { return m_simd; }

    /// Sset all components to 0
    void clear () ;

    /// Return an int8 with all components set to 0
    static const int8 Zero ();

    /// Return an int8 with all components set to 1
    static const int8 One ();

    /// Return an int8 with all components set to -1 (aka 0xffffffff)
    static const int8 NegOne ();

    /// Return an int8 with incremented components (e.g., 0,1,2,3).
    /// Optional argument can give a non-zero starting point.
    static const int8 Iota (int start=0, int step=1);

    /// Assign one value to all components.
    const int8 & operator= (int a);

    /// Assignment from another int8
    const int8 & operator= (const int8& other) ;

    /// Component access (set)
    int& operator[] (int i) ;

    /// Component access (get)
    int operator[] (int i) const;

    value_t x () const;
    value_t y () const;
    value_t z () const;
    value_t w () const;
    void set_x (value_t val);
    void set_y (value_t val);
    void set_z (value_t val);
    void set_w (value_t val);

    /// Extract the lower percision int4
    int4 lo () const;

    /// Extract the higher percision int4
    int4 hi () const;

    /// Helper: load a single int into all components
    void load (int a);

    /// Load separate values into each component. (doesn't work for int8)
    void load (int a, int b, int c, int d, int e, int f, int g, int h);

    /// Load from an array of 8 values
    void load (const int *values);

    void load (const int *values, int n) ;

    /// Load from an array of 8 unsigned short values, convert to int8
    void load (const unsigned short *values) ;

    /// Load from an array of 8 unsigned short values, convert to int8
    void load (const short *values);

    /// Load from an array of 8 unsigned char values, convert to int8
    void load (const unsigned char *values);

    /// Load from an array of 8 unsigned char values, convert to int8
    void load (const char *values);

    /// Store the values into memory
    void store (int *values) const;

    /// Store the first n values into memory
    void store (int *values, int n) const;

    /// Store the least significant 16 bits of each element into adjacent
    /// unsigned shorts.
    void store (unsigned short *values) const;

    /// Store the least significant 8 bits of each element into adjacent
    /// unsigned chars.
    void store (unsigned char *values) const;

    // Arithmetic operators (component-by-component)
    friend int8 operator+ (const int8& a, const int8& b);
    friend int8 operator- (const int8& a);
    friend int8 operator- (const int8& a, const int8& b);
    friend int8 operator* (const int8& a, const int8& b);
    friend int8 operator/ (const int8& a, const int8& b);
    friend int8 operator% (const int8& a, const int8& b);
    friend const int8 & operator+= (int8& a, const int8& b);
    friend const int8 & operator-= (int8& a, const int8& b);
    friend const int8 & operator*= (int8& a, const int8& b);
    friend const int8 & operator/= (int8& a, const int8& b);
    friend const int8 & operator%= (int8& a, const int8& b);
    // Bitwise operators (component-by-component)
    friend int8 operator& (const int8& a, const int8& b);
    friend int8 operator| (const int8& a, const int8& b);
    friend int8 operator^ (const int8& a, const int8& b);
    friend const int8& operator&= (int8& a, const int8& b);
    friend const int8& operator|= (int8& a, const int8& b);
    friend const int8& operator^= (int8& a, const int8& b);
    friend int8 operator~ (const int8& a);
    friend int8 operator<< (const int8& a, unsigned int bits);
    friend int8 operator>> (const int8& a, unsigned int bits);
    friend const int8& operator<<= (int8& a, unsigned int bits);
    friend const int8& operator>>= (int8& a, unsigned int bits);
    // Comparison operators (component-by-component)
    friend bool8 operator== (const int8& a, const int8& b);
    friend bool8 operator!= (const int8& a, const int8& b);
    friend bool8 operator<  (const int8& a, const int8& b);
    friend bool8 operator>  (const int8& a, const int8& b);
    friend bool8 operator>= (const int8& a, const int8& b);
    friend bool8 operator<= (const int8& a, const int8& b);

    /// Stream output
    friend std::ostream& operator<< (std::ostream& cout, const int8& a);

private:
    // The actual data representation
    union {
        simd_t  m_simd;
        value_t m_val[elements];
        simd_raw_t<int,4>::type m_4[2];
    };
};



// Shift right logical -- unsigned shift. This differs from operator>>
// in how it handles the sign bit.  (1<<31) >> 1 == (1<<31), but
// srl((1<<31),1) == 1<<30.
int8 srl (const int8& val, const unsigned int bits);

/// Helper: shuffle/swizzle with constant (templated) indices.
/// Example: shuffle<1,1,2,2>(bool4(a,b,c,d)) returns (b,b,c,c)
template<int i0, int i1, int i2, int i3,
         int i4, int i5, int i6, int i7> int8 shuffle (const int8& a);

/// shuffle<i>(a) is the same as shuffle<i,i,i,i>(a)
template<int i> int8 shuffle (const int8& a);

/// Helper: as rapid as possible extraction of one component, when the
/// index is fixed.
template<int i> int extract (const int8& v);

/// Helper: substitute val for a[i]
template<int i> int8 insert (const int8& a, int val);

/// The sum of all components, returned in all components.
int8 vreduce_add (const int8& v);

// Reduction across all components
int reduce_add (const int8& v);
int reduce_and (const int8& v);
int reduce_or (const int8& v);

/// Use a bool mask to select between components of a (if mask[i] is false)
/// and b (if mask[i] is true), i.e., mask[i] ? b[i] : a[i].
int8 blend (const int8& a, const int8& b, const bool8& mask);

/// Use a bool mask to select between `a` (if mask[i] is true) or 0 if
/// mask[i] is false), i.e., mask[i] ? a[i] : 0. Equivalent to
/// blend(0,a,mask).
int8 blend0 (const int8& a, const bool8& mask);

/// Use a bool mask to select between components of a (if mask[i] is false)
/// or 0 (if mask[i] is true), i.e., mask[i] ? 0 : a[i]. Equivalent to
/// blend(0,a,!mask), or blend(a,0,mask).
int8 blend0not (const int8& a, const bool8& mask);

/// Select 'a' where mask is true, 'b' where mask is false. Sure, it's a
/// synonym for blend with arguments rearranged, but this is more clear
/// because the arguments are symmetric to scalar (cond ? a : b).
int8 select (const bool8& mask, const int8& a, const int8& b);

// Per-element math
int8 abs (const int8& a);
int8 min (const int8& a, const int8& b);
int8 max (const int8& a, const int8& b);

// Circular bit rotate by k bits, for N values at once.
int8 rotl32 (const int8& x, const unsigned int k);

/// andnot(a,b) returns ((~a) & b)
int8 andnot (const int8& a, const int8& b);

/// Bitcast back and forth to intN (not a convert -- move the bits!)
int8 bitcast_to_int (const bool8& x);
int8 bitcast_to_int (const float8& x);
float8 bitcast_to_float (const int8& x);





/// Floating point 4-vector, accelerated by SIMD instructions when
/// available.
class float4 {
public:
    static const char* type_name() { return "float4"; }
    typedef float value_t;    ///< Underlying equivalent scalar value type
    typedef int4 int_t;       ///< SIMD int type
    typedef bool4 bool_t;     ///< SIMD bool type
    enum { elements = 4 };    ///< Number of scalar elements
    enum { paddedelements = 4 }; ///< Number of scalar elements for full pad
    enum { bits = elements*32 }; ///< Total number of bits
    typedef simd_raw_t<float,4>::type simd_t;  ///< the native SIMD type used

    /// Default constructor (contents undefined)
    float4 () { }

    /// Construct from a single value (store it in all slots)
    float4 (float a) { load(a); }

    /// Construct from 3 or 4 values
    float4 (float a, float b, float c, float d=0.0f) { load(a,b,c,d); }

    /// Construct from a pointer to 4 values
    float4 (const float *f) { load (f); }

    /// Copy construct from another float4
    float4 (const float4 &other) { m_simd = other.m_simd; }

    /// Construct from an int4 (promoting all components to float)
    explicit float4 (const int4& ival);

    /// Construct from the underlying SIMD type
    float4 (const simd_t& m) : m_simd(m) { }

    /// Return the raw SIMD type
    operator simd_t () const { return m_simd; }
    simd_t simd () const { return m_simd; }

    /// Construct from a Imath::V3f
    float4 (const Imath::V3f &v) { load (v[0], v[1], v[2]); }

    /// Cast to a Imath::V3f
    const Imath::V3f& V3f () const { return *(const Imath::V3f*)this; }

#if defined(ILMBASE_VERSION_MAJOR) && ILMBASE_VERSION_MAJOR >= 2
    // V4f is not defined for older Ilmbase. It's certainly safe for 2.x.

    /// Construct from a Imath::V4f
    float4 (const Imath::V4f &v) { load ((const float *)&v); }

    /// Cast to a Imath::V4f
    const Imath::V4f& V4f () const { return *(const Imath::V4f*)this; }
#endif

    /// Construct from a pointer to 4 unsigned short values
    explicit float4 (const unsigned short *vals) { load(vals); }

    /// Construct from a pointer to 4 short values
    explicit float4 (const short *vals) { load(vals); }

    /// Construct from a pointer to 4 unsigned char values
    explicit float4 (const unsigned char *vals) { load(vals); }

    /// Construct from a pointer to 4 char values
    explicit float4 (const char *vals) { load(vals); }

#ifdef _HALF_H_
    /// Construct from a pointer to 4 half (16 bit float) values
    explicit float4 (const half *vals) { load(vals); }
#endif

    /// Assign a single value to all components
    const float4 & operator= (float a) { load(a); return *this; }

    /// Assign a float4
    const float4 & operator= (float4 other) {
        m_simd = other.m_simd;
        return *this;
    }

    /// Return a float4 with all components set to 0.0
    static const float4 Zero ();

    /// Return a float4 with all components set to 1.0
    static const float4 One ();

    /// Return a float4 with incremented components (e.g., 0.0,1.0,2.0,3.0).
    /// Optional argument can give a non-zero starting point and non-1 step.
    static const float4 Iota (float start=0.0f, float step=1.0f);

    /// Set all components to 0.0
    void clear ();

#if defined(ILMBASE_VERSION_MAJOR) && ILMBASE_VERSION_MAJOR >= 2
    /// Assign from a Imath::V4f
    const float4 & operator= (const Imath::V4f &v);
#endif

    /// Assign from a Imath::V3f
    const float4 & operator= (const Imath::V3f &v);

    /// Component access (set)
    float& operator[] (int i);
    /// Component access (get)
    float operator[] (int i) const;

    value_t x () const;
    value_t y () const;
    value_t z () const;
    value_t w () const;
    void set_x (value_t val);
    void set_y (value_t val);
    void set_z (value_t val);
    void set_w (value_t val);

    /// Helper: load a single value into all components
    void load (float val);

    /// Helper: load 3 or 4 values. (If 3 are supplied, the 4th will be 0.)
    void load (float a, float b, float c, float d=0.0f);

    /// Load from an array of 4 values
    void load (const float *values);

    /// Load from a partial array of <=4 values. Unassigned values are
    /// undefined.
    void load (const float *values, int n);

    /// Load from an array of 4 unsigned short values, convert to float
    void load (const unsigned short *values);

    /// Load from an array of 4 short values, convert to float
    void load (const short *values);

    /// Load from an array of 4 unsigned char values, convert to float
    void load (const unsigned char *values);

    /// Load from an array of 4 char values, convert to float
    void load (const char *values);

#ifdef _HALF_H_
    /// Load from an array of 4 half values, convert to float
    void load (const half *values);
#endif /* _HALF_H_ */

    void store (float *values) const;

    /// Store the first n values into memory
    void store (float *values, int n) const;

#ifdef _HALF_H_
    void store (half *values) const;
#endif

    // Arithmetic operators
    friend float4 operator+ (const float4& a, const float4& b);
    const float4 & operator+= (const float4& a);
    float4 operator- () const;
    friend float4 operator- (const float4& a, const float4& b);
    const float4 & operator-= (const float4& a);
    friend float4 operator* (const float4& a, const float4& b);
    const float4 & operator*= (const float4& a);
    const float4 & operator*= (float val);
    friend float4 operator/ (const float4& a, const float4& b);
    const float4 & operator/= (const float4& a);
    const float4 & operator/= (float val);

    // Comparison operations
    friend bool4 operator== (const float4& a, const float4& b);
    friend bool4 operator!= (const float4& a, const float4& b);
    friend bool4 operator<  (const float4& a, const float4& b);
    friend bool4 operator>  (const float4& a, const float4& b);
    friend bool4 operator>= (const float4& a, const float4& b);
    friend bool4 operator<= (const float4& a, const float4& b);

    // Some oddball items that are handy

    /// Combine the first two components of A with the first two components
    /// of B.
    friend float4 AxyBxy (const float4& a, const float4& b);

    /// Combine the first two components of A with the first two components
    /// of B, but interleaved.
    friend float4 AxBxAyBy (const float4& a, const float4& b);

    /// Return xyz components, plus 0 for w
    float4 xyz0 () const;

    /// Return xyz components, plus 1 for w
    float4 xyz1 () const;

    /// Stream output
    friend inline std::ostream& operator<< (std::ostream& cout, const float4& val);

protected:
    // The actual data representation
    union {
        simd_t  m_simd;
        value_t m_val[paddedelements];
    };
};


/// Helper: shuffle/swizzle with constant (templated) indices.
/// Example: shuffle<1,1,2,2>(bool4(a,b,c,d)) returns (b,b,c,c)
template<int i0, int i1, int i2, int i3> float4 shuffle (const float4& a);

/// shuffle<i>(a) is the same as shuffle<i,i,i,i>(a)
template<int i> float4 shuffle (const float4& a);

/// Helper: as rapid as possible extraction of one component, when the
/// index is fixed.
template<int i> float extract (const float4& a);

/// Helper: substitute val for a[i]
template<int i> float4 insert (const float4& a, float val);

/// The sum of all components, returned in all components.
float4 vreduce_add (const float4& v);

/// The sum of all components, returned as a scalar.
float reduce_add (const float4& v);

/// Return the float dot (inner) product of a and b in every component.
float4 vdot (const float4 &a, const float4 &b);

/// Return the float dot (inner) product of a and b.
float dot (const float4 &a, const float4 &b);

/// Return the float 3-component dot (inner) product of a and b in
/// all components.
float4 vdot3 (const float4 &a, const float4 &b);

/// Return the float 3-component dot (inner) product of a and b.
float dot3 (const float4 &a, const float4 &b);

/// Use a bool mask to select between components of a (if mask[i] is false)
/// and b (if mask[i] is true), i.e., mask[i] ? b[i] : a[i].
float4 blend (const float4& a, const float4& b, const bool4& mask);

/// Use a bool mask to select between `a` (if mask[i] is true) or 0 if
/// mask[i] is false), i.e., mask[i] ? a[i] : 0. Equivalent to
/// blend(0,a,mask).
float4 blend0 (const float4& a, const bool4& mask);

/// Use a bool mask to select between components of a (if mask[i] is false)
/// or 0 (if mask[i] is true), i.e., mask[i] ? 0 : a[i]. Equivalent to
/// blend(0,a,!mask), or blend(a,0,mask).
float4 blend0not (const float4& a, const bool4& mask);

/// "Safe" divide of float4/float4 -- for any component of the divisor
/// that is 0, return 0 rather than Inf.
float4 safe_div (const float4 &a, const float4 &b);

/// Homogeneous divide to turn a float4 into a float3.
float3 hdiv (const float4 &a);

/// Select 'a' where mask is true, 'b' where mask is false. Sure, it's a
/// synonym for blend with arguments rearranged, but this is more clear
/// because the arguments are symmetric to scalar (cond ? a : b).
float4 select (const bool4& mask, const float4& a, const float4& b);

// Per-element math
float4 abs (const float4& a);    ///< absolute value (float)
float4 sign (const float4& a);   ///< 1.0 when value >= 0, -1 when negative
float4 ceil (const float4& a);
float4 floor (const float4& a);
int4 floori (const float4& a);    ///< (int)floor

/// Per-element round to nearest integer (rounding away from 0 in cases
/// that are exactly half way).
float4 round (const float4& a);

/// Per-element round to nearest integer (rounding away from 0 in cases
/// that are exactly half way).
int4 rint (const float4& a);

float4 sqrt (const float4 &a);
float4 rsqrt (const float4 &a);   ///< Fully accurate 1/sqrt
float4 rsqrt_fast (const float4 &a);  ///< Fast, approximate 1/sqrt
float4 min (const float4& a, const float4& b); ///< Per-element min
float4 max (const float4& a, const float4& b); ///< Per-element max
template <typename T> T exp (const T& v);  // template for all SIMD variants
template <typename T> T log (const T& v);

/// andnot(a,b) returns ((~a) & b)
float4 andnot (const float4& a, const float4& b);

// Fused multiply and add (or subtract):
float4 madd (const float4& a, const float4& b, const float4& c); // a*b + c
float4 msub (const float4& a, const float4& b, const float4& c); // a*b - c
float4 nmadd (const float4& a, const float4& b, const float4& c); // -a*b + c
float4 nmsub (const float4& a, const float4& b, const float4& c); // -a*b - c

/// Transpose the rows and columns of the 4x4 matrix [a b c d].
/// In the end, a will have the original (a[0], b[0], c[0], d[0]),
/// b will have the original (a[1], b[1], c[1], d[1]), and so on.
void transpose (float4 &a, float4 &b, float4 &c, float4 &d);
void transpose (const float4& a, const float4& b, const float4& c, const float4& d,
                float4 &r0, float4 &r1, float4 &r2, float4 &r3);

/// Make a float4 consisting of the first element of each of 4 float4's.
float4 AxBxCxDx (const float4& a, const float4& b,
                 const float4& c, const float4& d);



/// Floating point 3-vector, aligned to be internally identical to a float4.
/// The way it differs from float4 is that all of he load functions only
/// load three values, and all the stores only store 3 values. The vast
/// majority of ops just fall back to the float4 version, and so will
/// operate on the 4th component, but we won't care about that results.
class float3 : public float4 {
public:
    static const char* type_name() { return "float3"; }
    enum { elements = 3 };    ///< Number of scalar elements
    enum { paddedelements = 4 }; ///< Number of scalar elements for full pad

    /// Default constructor (contents undefined)
    float3 () { }

    /// Construct from a single value (store it in all slots)
    float3 (float a) { load(a); }

    /// Construct from 3 or 4 values
    float3 (float a, float b, float c) { float4::load(a,b,c); }

    /// Construct from a pointer to 4 values
    float3 (const float *f) { load (f); }

    /// Copy construct from another float3
    float3 (const float3 &other);

    explicit float3 (const float4 &other);

#if OIIO_SIMD
    /// Construct from the underlying SIMD type
    explicit float3 (const simd_t& m) : float4(m) { }
#endif

    /// Construct from a Imath::V3f
    float3 (const Imath::V3f &v) : float4(v) { }

    /// Cast to a Imath::V3f
    const Imath::V3f& V3f () const { return *(const Imath::V3f*)this; }

    /// Construct from a pointer to 4 unsigned short values
    explicit float3 (const unsigned short *vals) { load(vals); }

    /// Construct from a pointer to 4 short values
    explicit float3 (const short *vals) { load(vals); }

    /// Construct from a pointer to 4 unsigned char values
    explicit float3 (const unsigned char *vals) { load(vals); }

    /// Construct from a pointer to 4 char values
    explicit float3 (const char *vals) { load(vals); }

#ifdef _HALF_H_
    /// Construct from a pointer to 4 half (16 bit float) values
    explicit float3 (const half *vals) { load(vals); }
#endif

    /// Assign a single value to all components
    const float3 & operator= (float a) { load(a); return *this; }

    /// Return a float3 with all components set to 0.0
    static const float3 Zero ();

    /// Return a float3 with all components set to 1.0
    static const float3 One ();

    /// Return a float3 with incremented components (e.g., 0.0,1.0,2.0).
    /// Optional argument can give a non-zero starting point and non-1 step.
    static const float3 Iota (float start=0.0f, float step=1.0f);

    /// Helper: load a single value into all components
    void load (float val);

    /// Load from an array of 4 values
    void load (const float *values);

    /// Load from an array of 4 values
    void load (const float *values, int n);

    /// Load from an array of 4 unsigned short values, convert to float
    void load (const unsigned short *values);

    /// Load from an array of 4 short values, convert to float
    void load (const short *values);

    /// Load from an array of 4 unsigned char values, convert to float
    void load (const unsigned char *values);

    /// Load from an array of 4 char values, convert to float
    void load (const char *values);

#ifdef _HALF_H_
    /// Load from an array of 4 half values, convert to float
    void load (const half *values);
#endif /* _HALF_H_ */

    void store (float *values) const;

    void store (float *values, int n) const;

#ifdef _HALF_H_
    void store (half *values) const;
#endif

    /// Store into an Imath::V3f reference.
    void store (Imath::V3f &vec) const;

    // Math operators -- define in terms of float3.
    friend float3 operator+ (const float3& a, const float3& b);
    const float3 & operator+= (const float3& a);
    float3 operator- () const;
    friend float3 operator- (const float3& a, const float3& b);
    const float3 & operator-= (const float3& a);
    friend float3 operator* (const float3& a, const float3& b);
    const float3 & operator*= (const float3& a);
    const float3 & operator*= (float a);
    friend float3 operator/ (const float3& a, const float3& b);
    const float3 & operator/= (const float3& a);
    const float3 & operator/= (float a);

    float3 normalized () const;
    float3 normalized_fast () const;

    /// Stream output
    friend inline std::ostream& operator<< (std::ostream& cout, const float3& val);
};




/// SIMD-based 4x4 matrix. This is guaranteed to have memory layout (when
/// not in registers) isomorphic to Imath::M44f.
class matrix44 {
public:
    // Uninitialized
    OIIO_FORCEINLINE matrix44 ()
#ifndef OIIO_SIMD_SSE
        : m_mat(Imath::UNINITIALIZED)
#endif
    { }

    /// Construct from a reference to an Imath::M44f
    OIIO_FORCEINLINE matrix44 (const Imath::M44f &M) {
#if OIIO_SIMD_SSE
        m_row[0].load (M[0]);
        m_row[1].load (M[1]);
        m_row[2].load (M[2]);
        m_row[3].load (M[3]);
#else
        m_mat = M;
#endif
    }

    /// Construct from a float array
    OIIO_FORCEINLINE explicit matrix44 (const float *f) {
#if OIIO_SIMD_SSE
        m_row[0].load (f+0);
        m_row[1].load (f+4);
        m_row[2].load (f+8);
        m_row[3].load (f+12);
#else
        memcpy (&m_mat, f, 16*sizeof(float));
#endif
    }

    /// Construct from 4 float4 rows
    OIIO_FORCEINLINE explicit matrix44 (const float4& a, const float4& b,
                                        const float4& c, const float4& d) {
#if OIIO_SIMD_SSE
        m_row[0] = a; m_row[1] = b; m_row[2] = c; m_row[3] = d;
#else
        a.store (m_mat[0]);
        b.store (m_mat[1]);
        c.store (m_mat[2]);
        d.store (m_mat[3]);
#endif
    }
    /// Construct from 4 float[4] rows
    OIIO_FORCEINLINE explicit matrix44 (const float *a, const float *b,
                                        const float *c, const float *d) {
#if OIIO_SIMD_SSE
        m_row[0].load(a); m_row[1].load(b); m_row[2].load(c); m_row[3].load(d);
#else
        memcpy (m_mat[0], a, 4*sizeof(float));
        memcpy (m_mat[1], b, 4*sizeof(float));
        memcpy (m_mat[2], c, 4*sizeof(float));
        memcpy (m_mat[3], d, 4*sizeof(float));
#endif
    }

    /// Present as an Imath::M44f
    const Imath::M44f& M44f() const;

    /// Return one row
    float4 operator[] (int i) const;

    /// Return the transposed matrix
    matrix44 transposed () const;

    /// Transform 3-point V by 4x4 matrix M.
    float3 transformp (const float3 &V) const;

    /// Transform 3-vector V by 4x4 matrix M.
    float3 transformv (const float3 &V) const;

    /// Transform 3-vector V by the transpose of 4x4 matrix M.
    float3 transformvT (const float3 &V) const;

    bool operator== (const matrix44& m) const;

    bool operator== (const Imath::M44f& m) const ;
    friend bool operator== (const Imath::M44f& a, const matrix44 &b);

    bool operator!= (const matrix44& m) const;

    bool operator!= (const Imath::M44f& m) const;
    friend bool operator!= (const Imath::M44f& a, const matrix44 &b);

    /// Return the inverse of the matrix.
    matrix44 inverse() const;

    /// Stream output
    friend inline std::ostream& operator<< (std::ostream& cout, const matrix44 &M);

private:
#if OIIO_SIMD_SSE
    float4 m_row[4];
#else
    Imath::M44f m_mat;
#endif
};

/// Transform 3-point V by 4x4 matrix M.
float3 transformp (const matrix44 &M, const float3 &V);
float3 transformp (const Imath::M44f &M, const float3 &V);

/// Transform 3-vector V by 4x4 matrix M.
float3 transformv (const matrix44 &M, const float3 &V);
float3 transformv (const Imath::M44f &M, const float3 &V);

// Transform 3-vector by the transpose of 4x4 matrix M.
float3 transformvT (const matrix44 &M, const float3 &V);
float3 transformvT (const Imath::M44f &M, const float3 &V);




/// Floating point 8-vector, accelerated by SIMD instructions when
/// available.
class float8 {
public:
    enum { elements = 8 };    ///< Number of scalar elements
    enum { paddedelements = 8 }; ///< Number of scalar elements for full pad
    enum { bits = elements*32 }; ///< Total number of bits
    typedef float value_t;    ///< Underlying equivalent scalar value type
    typedef simd_raw_t<float,8>::type simd_t;  ///< the native SIMD type used
    typedef bool8 bool_t; ///< bool type of the same length
    typedef int8 int_t;    ///< int type of the same length
    static const char* type_name() { return "float8"; }

    /// Default constructor (contents undefined)
    float8 () { }

    /// Construct from a single value (store it in all slots)
    float8 (float a) { load(a); }

    /// Construct from 3 or 4 values
    float8 (float a, float b, float c, float d,
            float e, float f, float g, float h) { load(a,b,c,d,e,f,g,h); }

    /// Construct from a pointer to 4 values
    float8 (const float *f) { load (f); }

    /// Copy construct from another float8
    float8 (const float8 &other) { m_simd = other.m_simd; }

    /// Construct from an int vector (promoting all components to float)
    explicit float8 (const int8& ival);

    /// Construct from two float4's
    float8 (const float4 &lo, const float4 &hi);

    /// Construct from the underlying SIMD type
    float8 (const simd_t& m) : m_simd(m) { }

    /// Return the raw SIMD type
    operator simd_t () const { return m_simd; }
    simd_t simd () const { return m_simd; }

    /// Construct from a pointer to unsigned short values
    explicit float8 (const unsigned short *vals) { load(vals); }

    /// Construct from a pointer to short values
    explicit float8 (const short *vals) { load(vals); }

    /// Construct from a pointer to unsigned char values
    explicit float8 (const unsigned char *vals) { load(vals); }

    /// Construct from a pointer to char values
    explicit float8 (const char *vals) { load(vals); }

#ifdef _HALF_H_
    /// Construct from a pointer to half (16 bit float) values
    explicit float8 (const half *vals) { load(vals); }
#endif

    /// Assign a single value to all components
    const float8& operator= (float a) { load(a); return *this; }

    /// Assign a float8
    const float8& operator= (float8 other) {
        m_simd = other.m_simd;
        return *this;
    }

    /// Return a float8 with all components set to 0.0
    static const float8 Zero ();

    /// Return a float8 with all components set to 1.0
    static const float8 One ();

    /// Return a float8 with incremented components (e.g., 0,1,2,3,...)
    /// Optional argument can give a non-zero starting point and non-1 step.
    static const float8 Iota (float start=0.0f, float step=1.0f);

    /// Set all components to 0.0
    void clear ();

    /// Component access (set)
    float& operator[] (int i);
    /// Component access (get)
    float operator[] (int i) const;

    value_t x () const;
    value_t y () const;
    value_t z () const;
    value_t w () const;
    void set_x (value_t val);
    void set_y (value_t val);
    void set_z (value_t val);
    void set_w (value_t val);

    /// Extract the lower percision float4
    float4 lo () const;

    /// Extract the higher percision float4
    float4 hi () const;

    /// Helper: load a single value into all components
    void load (float val);

    /// Helper: load 3 or 4 values. (If 3 are supplied, the 4th will be 0.)
    void load (float a, float b, float c, float d,
               float e, float f, float g, float h);

    /// Load from an array of values
    void load (const float *values);

    /// Load from a partial array of <=4 values. Unassigned values are
    /// undefined.
    void load (const float *values, int n);

    /// Load from an array of 4 unsigned short values, convert to float
    void load (const unsigned short *values);

    /// Load from an array of 4 short values, convert to float
    void load (const short *values);

    /// Load from an array of 4 unsigned char values, convert to float
    void load (const unsigned char *values);

    /// Load from an array of 4 char values, convert to float
    void load (const char *values);

#ifdef _HALF_H_
    /// Load from an array of 4 half values, convert to float
    void load (const half *values);
#endif /* _HALF_H_ */

    void store (float *values) const;

    /// Store the first n values into memory
    void store (float *values, int n) const;

#ifdef _HALF_H_
    void store (half *values) const;
#endif

    // Arithmetic operators
    friend float8 operator+ (const float8& a, const float8& b);
    const float8 & operator+= (const float8& a);
    float8 operator- () const;
    friend float8 operator- (const float8& a, const float8& b);
    const float8 & operator-= (const float8& a);
    friend float8 operator* (const float8& a, const float8& b);
    const float8 & operator*= (const float8& a);
    const float8 & operator*= (float val);
    friend float8 operator/ (const float8& a, const float8& b);
    const float8 & operator/= (const float8& a);
    const float8 & operator/= (float val);

    // Comparison operations
    friend bool8 operator== (const float8& a, const float8& b);
    friend bool8 operator!= (const float8& a, const float8& b);
    friend bool8 operator<  (const float8& a, const float8& b);
    friend bool8 operator>  (const float8& a, const float8& b);
    friend bool8 operator>= (const float8& a, const float8& b);
    friend bool8 operator<= (const float8& a, const float8& b);

    // Some oddball items that are handy

    /// Stream output
    friend inline std::ostream& operator<< (std::ostream& cout, const float8& val);

protected:
    // The actual data representation
    union {
        simd_t  m_simd;
        value_t m_val[paddedelements];
        simd_raw_t<float,4>::type m_4[2];
    };
};


/// Helper: shuffle/swizzle with constant (templated) indices.
/// Example: shuffle<1,1,2,2>(bool4(a,b,c,d)) returns (b,b,c,c)
template<int i0, int i1, int i2, int i3, int i4, int i5, int i6, int i7>
float8 shuffle (const float8& a);

/// shuffle<i>(a) is the same as shuffle<i,i,i,i,...>(a)
template<int i> float8 shuffle (const float8& a);

/// Helper: as rapid as possible extraction of one component, when the
/// index is fixed.
template<int i> float extract (const float8& a);

/// Helper: substitute val for a[i]
template<int i> float8 insert (const float8& a, float val);

/// The sum of all components, returned in all components.
float8 vreduce_add (const float8& v);

/// The sum of all components, returned as a scalar.
float reduce_add (const float8& v);

/// Return the float dot (inner) product of a and b in every component.
float8 vdot (const float8 &a, const float8 &b);

/// Return the float dot (inner) product of a and b.
float dot (const float8 &a, const float8 &b);

/// Return the float 3-component dot (inner) product of a and b in
/// all components.
float8 vdot3 (const float8 &a, const float8 &b);

/// Return the float 3-component dot (inner) product of a and b.
float dot3 (const float8 &a, const float8 &b);

/// Use a bool mask to select between components of a (if mask[i] is false)
/// and b (if mask[i] is true), i.e., mask[i] ? b[i] : a[i].
float8 blend (const float8& a, const float8& b, const bool4& mask);

/// Use a bool mask to select between `a` (if mask[i] is true) or 0 if
/// mask[i] is false), i.e., mask[i] ? a[i] : 0. Equivalent to
/// blend(0,a,mask).
float8 blend0 (const float8& a, const bool4& mask);

/// Use a bool mask to select between components of a (if mask[i] is false)
/// or 0 (if mask[i] is true), i.e., mask[i] ? 0 : a[i]. Equivalent to
/// blend(0,a,!mask), or blend(a,0,mask).
float8 blend0not (const float8& a, const bool4& mask);

/// "Safe" divide of float8/float8 -- for any component of the divisor
/// that is 0, return 0 rather than Inf.
float8 safe_div (const float8 &a, const float8 &b);

/// Select 'a' where mask is true, 'b' where mask is false. Sure, it's a
/// synonym for blend with arguments rearranged, but this is more clear
/// because the arguments are symmetric to scalar (cond ? a : b).
float8 select (const bool8& mask, const float8& a, const float8& b);

// Per-element math
float8 abs (const float8& a);    ///< absolute value (float)
float8 sign (const float8& a);   ///< 1.0 when value >= 0, -1 when negative
float8 ceil (const float8& a);
float8 floor (const float8& a);
int8 floori (const float8& a);    ///< (int)floor

/// Per-element round to nearest integer (rounding away from 0 in cases
/// that are exactly half way).
float8 round (const float8& a);

/// Per-element round to nearest integer (rounding away from 0 in cases
/// that are exactly half way).
int8 rint (const float8& a);

float8 sqrt (const float8 &a);
float8 rsqrt (const float8 &a);   ///< Fully accurate 1/sqrt
float8 rsqrt_fast (const float8 &a);  ///< Fast, approximate 1/sqrt
float8 min (const float8& a, const float8& b); ///< Per-element min
float8 max (const float8& a, const float8& b); ///< Per-element max
// float8 exp (const float8& v);  // See template with float4
// float8 log (const float8& v);  // See template with float4

/// andnot(a,b) returns ((~a) & b)
float8 andnot (const float8& a, const float8& b);

// Fused multiply and add (or subtract):
float8 madd (const float8& a, const float8& b, const float8& c); // a*b + c
float8 msub (const float8& a, const float8& b, const float8& c); // a*b - c
float8 nmadd (const float8& a, const float8& b, const float8& c); // -a*b + c
float8 nmsub (const float8& a, const float8& b, const float8& c); // -a*b - c






//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// Gory implementation details follow.
//
// ^^^ All declarations and documention is above ^^^
//
// vvv Below is the implementation, often considerably cluttered with
//     #if's for each architeture, and unapologitic use of intrinsics and
//     every manner of dirty trick we can think of to make things fast.
//     Some of this isn't pretty. We won't recapitulate comments or
//     documentation of what the functions are supposed to do, please
//     consult the declarations above for that.
//
//     Here be dragons.
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////
// bool4 implementation


OIIO_FORCEINLINE int bool4::operator[] (int i) const {
    DASSERT(i >= 0 && i < elements);
#if OIIO_SIMD_SSE
    return ((_mm_movemask_ps(m_simd) >> i) & 1) ? -1 : 0;
#else
    return m_val[i];
#endif
}

OIIO_FORCEINLINE int& bool4::operator[] (int i) {
    DASSERT(i >= 0 && i < elements);
    return m_val[i];
}


OIIO_FORCEINLINE std::ostream& operator<< (std::ostream& cout, const bool4& a) {
    cout << a[0];
    for (int i = 1; i < a.elements; ++i)
        cout << ' ' << a[i];
    return cout;
}


OIIO_FORCEINLINE void bool4::load (bool a) {
#if OIIO_SIMD_SSE
    m_simd = _mm_castsi128_ps(_mm_set1_epi32(-int(a)));
#else
    int val = -int(a);
    SIMD_CONSTRUCT (val);
#endif
}


OIIO_FORCEINLINE void bool4::load (bool a, bool b, bool c, bool d) {
#if OIIO_SIMD_SSE
    // N.B. -- we need to reverse the order because of our convention
    // of storing a,b,c,d in the same order in memory.
    m_simd = _mm_castsi128_ps(_mm_set_epi32(-int(d), -int(c), -int(b), -int(a)));
#else
    m_val[0] = -int(a);
    m_val[1] = -int(b);
    m_val[2] = -int(c);
    m_val[3] = -int(d);
#endif
}

OIIO_FORCEINLINE bool4::bool4 (bool *a) {
    load (a[0], a[1], a[2], a[3]);
}

OIIO_FORCEINLINE const bool4& bool4::operator= (const bool4 & other) {
    m_simd = other.m_simd;
    return *this;
}



OIIO_FORCEINLINE void bool4::clear () {
#if OIIO_SIMD_SSE
    m_simd = _mm_setzero_ps();
#else
    *this = false;
#endif
}


OIIO_FORCEINLINE const bool4 bool4::False () {
#if OIIO_SIMD_SSE
    return _mm_setzero_ps();
#else
    return false;
#endif
}

OIIO_FORCEINLINE const bool4 bool4::True () {
    // Fastest way to fill with all 1 bits is to cmp any value to itself.
#if OIIO_SIMD_SSE
# if OIIO_SIMD_AVX && (OIIO_GNUC_VERSION > 50000)
    __m128i anyval = _mm_undefined_si128();
# else
    __m128i anyval = _mm_setzero_si128();
# endif
    return _mm_castsi128_ps (_mm_cmpeq_epi8 (anyval, anyval));
#else
    return true;
#endif
}

OIIO_FORCEINLINE void bool4::store (bool *values) const {
    SIMD_DO (values[i] = m_val[i] ? true : false);
}

OIIO_FORCEINLINE void bool4::store (bool *values, int n) const {
    DASSERT (n >= 0 && n <= elements);
    for (int i = 0; i < n; ++i)
        values[i] = m_val[i] ? true : false;
}



OIIO_FORCEINLINE bool4 operator! (const bool4 & a) {
#if OIIO_SIMD_SSE
    return _mm_xor_ps (a.simd(), bool4::True());
#else
    SIMD_RETURN (bool4, a[i] ^ (-1));
#endif
}

OIIO_FORCEINLINE bool4 operator& (const bool4 & a, const bool4 & b) {
#if OIIO_SIMD_SSE
    return _mm_and_ps (a.simd(), b.simd());
#else
    SIMD_RETURN (bool4, a[i] & b[i]);
#endif
}

OIIO_FORCEINLINE bool4 operator| (const bool4 & a, const bool4 & b) {
#if OIIO_SIMD_SSE
    return _mm_or_ps (a.simd(), b.simd());
#else
    SIMD_RETURN (bool4, a[i] | b[i]);
#endif
}

OIIO_FORCEINLINE bool4 operator^ (const bool4& a, const bool4& b) {
#if OIIO_SIMD_SSE
    return _mm_xor_ps (a.simd(), b.simd());
#else
    SIMD_RETURN (bool4, a[i] ^ b[i]);
#endif
}


OIIO_FORCEINLINE const bool4& operator&= (bool4& a, const bool4 &b) {
    return a = a & b;
}

OIIO_FORCEINLINE const bool4& operator|= (bool4& a, const bool4& b) {
    return a = a | b;
}

OIIO_FORCEINLINE const bool4& operator^= (bool4& a, const bool4& b) {
    return a = a ^ b;
}

OIIO_FORCEINLINE bool4 operator~ (const bool4& a) {
#if OIIO_SIMD_SSE
    // Fastest way to bit-complement in SSE is to xor with 0xffffffff.
    return _mm_xor_ps (a.simd(), bool4::True());
#else
    SIMD_RETURN (bool4, ~a[i]);
#endif
}

OIIO_FORCEINLINE bool4 operator== (const bool4 & a, const bool4 & b) {
#if OIIO_SIMD_SSE
    return _mm_castsi128_ps (_mm_cmpeq_epi32 (_mm_castps_si128 (a), _mm_castps_si128(b)));
#else
    SIMD_RETURN (bool4, a[i] == b[i] ? -1 : 0);
#endif
}

OIIO_FORCEINLINE bool4 operator!= (const bool4 & a, const bool4 & b) {
#if OIIO_SIMD_SSE
    return _mm_xor_ps (a, b);
#else
    SIMD_RETURN (bool4, a[i] != b[i] ? -1 : 0);
#endif
}




#if OIIO_SIMD_SSE
// Shuffling. Use like this:  x = shuffle<3,2,1,0>(b)
template<int i0, int i1, int i2, int i3>
OIIO_FORCEINLINE __m128i shuffle_sse (__m128i v) {
    return _mm_shuffle_epi32(v, _MM_SHUFFLE(i3, i2, i1, i0));
}
#endif

#if OIIO_SIMD_SSE >= 3
// SSE3 has intrinsics for a few special cases
template<> OIIO_FORCEINLINE __m128i shuffle_sse<0, 0, 2, 2> (__m128i a) {
    return _mm_castps_si128(_mm_moveldup_ps(_mm_castsi128_ps(a)));
}
template<> OIIO_FORCEINLINE __m128i shuffle_sse<1, 1, 3, 3> (__m128i a) {
    return _mm_castps_si128(_mm_movehdup_ps(_mm_castsi128_ps(a)));
}
template<> OIIO_FORCEINLINE __m128i shuffle_sse<0, 1, 0, 1> (__m128i a) {
    return _mm_castpd_si128(_mm_movedup_pd(_mm_castsi128_pd(a)));
}
#endif

#if OIIO_SIMD_SSE
template<int i0, int i1, int i2, int i3>
OIIO_FORCEINLINE __m128 shuffle_sse (__m128 a) {
    return _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(a), _MM_SHUFFLE(i3, i2, i1, i0)));
}
#endif

#if OIIO_SIMD_SSE >= 3
// SSE3 has intrinsics for a few special cases
template<> OIIO_FORCEINLINE __m128 shuffle_sse<0, 0, 2, 2> (__m128 a) {
    return _mm_moveldup_ps(a);
}
template<> OIIO_FORCEINLINE __m128 shuffle_sse<1, 1, 3, 3> (__m128 a) {
    return _mm_movehdup_ps(a);
}
template<> OIIO_FORCEINLINE __m128 shuffle_sse<0, 1, 0, 1> (__m128 a) {
    return _mm_castpd_ps(_mm_movedup_pd(_mm_castps_pd(a)));
}
#endif


/// Helper: shuffle/swizzle with constant (templated) indices.
/// Example: shuffle<1,1,2,2>(bool4(a,b,c,d)) returns (b,b,c,c)
template<int i0, int i1, int i2, int i3>
OIIO_FORCEINLINE bool4 shuffle (const bool4& a) {
#if OIIO_SIMD_SSE
    return shuffle_sse<i0,i1,i2,i3> (a.simd());
#else
    return bool4 (a[i0], a[i1], a[i2], a[i3]);
#endif
}

/// shuffle<i>(a) is the same as shuffle<i,i,i,i>(a)
template<int i> OIIO_FORCEINLINE bool4 shuffle (const bool4& a) {
    return shuffle<i,i,i,i>(a);
}


/// Helper: as rapid as possible extraction of one component, when the
/// index is fixed.
template<int i>
OIIO_FORCEINLINE bool extract (const bool4& a) {
#if OIIO_SIMD_SSE >= 4
    return _mm_extract_epi32(_mm_castps_si128(a.simd()), i);  // SSE4.1 only
#else
    return a[i];
#endif
}

/// Helper: substitute val for a[i]
template<int i>
OIIO_FORCEINLINE bool4 insert (const bool4& a, bool val) {
#if OIIO_SIMD_SSE >= 4
    int ival = -int(val);
    return _mm_castsi128_ps (_mm_insert_epi32 (_mm_castps_si128(a), ival, i));
#else
    bool4 tmp = a;
    tmp[i] = -int(val);
    return tmp;
#endif
}

OIIO_FORCEINLINE bool reduce_and (const bool4& v) {
#if OIIO_SIMD_AVX
    return _mm_testc_ps (v, bool4(true)) != 0;
#elif OIIO_SIMD_SSE
    return _mm_movemask_ps(v.simd()) == 0xf;
#else
    SIMD_RETURN_REDUCE (bool, true, r &= (v[i] != 0));
#endif
}

OIIO_FORCEINLINE bool reduce_or (const bool4& v) {
#if OIIO_SIMD_AVX
    return ! _mm_testz_ps (v, v);
#elif OIIO_SIMD_SSE
    return _mm_movemask_ps(v) != 0;
#else
    SIMD_RETURN_REDUCE (bool, false, r |= (v[i] != 0));
#endif
}

OIIO_FORCEINLINE bool all (const bool4& v) { return reduce_and(v) == true; }
OIIO_FORCEINLINE bool any (const bool4& v) { return reduce_or(v) == true; }
OIIO_FORCEINLINE bool none (const bool4& v) { return reduce_or(v) == false; }



//////////////////////////////////////////////////////////////////////
// bool8 implementation


OIIO_FORCEINLINE int bool8::operator[] (int i) const {
    DASSERT(i >= 0 && i < elements);
#if OIIO_SIMD_AVX
    return ((_mm256_movemask_ps(m_simd) >> i) & 1) ? -1 : 0;
#else
    return m_val[i];
#endif
}

OIIO_FORCEINLINE int& bool8::operator[] (int i) {
    DASSERT(i >= 0 && i < elements);
    return m_val[i];
}


OIIO_FORCEINLINE std::ostream& operator<< (std::ostream& cout, const bool8& a) {
    cout << a[0];
    for (int i = 1; i < a.elements; ++i)
        cout << ' ' << a[i];
    return cout;
}


OIIO_FORCEINLINE void bool8::load (bool a) {
#if OIIO_SIMD_AVX
    m_simd = _mm256_castsi256_ps(_mm256_set1_epi32(-int(a)));
#else
    int val = -int(a);
    SIMD_CONSTRUCT (val);
#endif
}


OIIO_FORCEINLINE void bool8::load (bool a, bool b, bool c, bool d,
                                   bool e, bool f, bool g, bool h) {
#if OIIO_SIMD_AVX
    // N.B. -- we need to reverse the order because of our convention
    // of storing a,b,c,d in the same order in memory.
    m_simd = _mm256_castsi256_ps(_mm256_set_epi32(-int(h), -int(g), -int(f), -int(e),
                                                  -int(d), -int(c), -int(b), -int(a)));
#else
    m_val[0] = -int(a);
    m_val[1] = -int(b);
    m_val[2] = -int(c);
    m_val[3] = -int(d);
    m_val[4] = -int(e);
    m_val[5] = -int(f);
    m_val[6] = -int(g);
    m_val[7] = -int(h);
#endif
}

OIIO_FORCEINLINE bool8::bool8 (bool a, bool b, bool c, bool d,
                               bool e, bool f, bool g, bool h) {
    load (a, b, c, d, e, f, g, h);
}

OIIO_FORCEINLINE bool8::bool8 (bool *a) {
    load (a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
}


OIIO_FORCEINLINE const bool8& bool8::operator= (bool a) {
    load(a);
    return *this;
}

OIIO_FORCEINLINE const bool8& bool8::operator= (const bool8 & other) {
    m_simd = other.m_simd;
    return *this;
}

OIIO_FORCEINLINE void bool8::clear () {
#if OIIO_SIMD_AVX
    m_simd = _mm256_setzero_ps();
#else
    *this = false;
#endif
}

OIIO_FORCEINLINE const bool8 bool8::False () {
#if OIIO_SIMD_AVX
    return _mm256_setzero_ps();
#else
    return false;
#endif
}


OIIO_FORCEINLINE const bool8 bool8::True () {
#if OIIO_SIMD_AVX
# if OIIO_SIMD_AVX >= 2 && (OIIO_GNUC_VERSION > 50000)
    // Fastest way to fill with all 1 bits is to cmp any value to itself.
    __m256i anyval = _mm256_undefined_si256();
    return _mm256_castsi256_ps (_mm256_cmpeq_epi8 (anyval, anyval));
# else
    return _mm256_castsi256_ps (_mm256_set1_epi32 (-1));
# endif
#else
    return true;
#endif
}


OIIO_FORCEINLINE void bool8::store (bool *values) const {
    SIMD_DO (values[i] = m_val[i] ? true : false);
}

OIIO_FORCEINLINE void bool8::store (bool *values, int n) const {
    DASSERT (n >= 0 && n <= elements);
    for (int i = 0; i < n; ++i)
        values[i] = m_val[i] ? true : false;
}


OIIO_FORCEINLINE bool4 bool8::lo () const {
#if OIIO_SIMD_AVX
    return _mm256_castps256_ps128 (simd());
#else
    return m_4[0];
#endif
}

OIIO_FORCEINLINE bool4 bool8::hi () const {
#if OIIO_SIMD_AVX
    return _mm256_extractf128_ps (simd(), 1);
#else
    return m_4[1];
#endif
}


OIIO_FORCEINLINE bool8::bool8 (const bool4& lo, const bool4 &hi) {
#if OIIO_SIMD_AVX
    __m256 r = _mm256_castps128_ps256 (lo);
    m_simd = _mm256_insertf128_ps (r, hi, 1);
    // N.B. equivalent, if available: m_simd = _mm256_set_m128 (hi, lo);
#else
    m_4[0] = lo;
    m_4[1] = hi;
#endif
}


OIIO_FORCEINLINE bool8 operator! (const bool8 & a) {
#if OIIO_SIMD_AVX
    return _mm256_xor_ps (a.simd(), bool8::True());
#else
    SIMD_RETURN (bool8, a[i] ^ (-1));
#endif
}

OIIO_FORCEINLINE bool8 operator& (const bool8 & a, const bool8 & b) {
#if OIIO_SIMD_AVX
    return _mm256_and_ps (a.simd(), b.simd());
#else
    SIMD_RETURN (bool8, a[i] & b[i]);
#endif
}

OIIO_FORCEINLINE bool8 operator| (const bool8 & a, const bool8 & b) {
#if OIIO_SIMD_AVX
    return _mm256_or_ps (a.simd(), b.simd());
#else
    SIMD_RETURN (bool8, a[i] | b[i]);
#endif
}

OIIO_FORCEINLINE bool8 operator^ (const bool8& a, const bool8& b) {
#if OIIO_SIMD_AVX
    return _mm256_xor_ps (a.simd(), b.simd());
#else
    SIMD_RETURN (bool8, a[i] ^ b[i]);
#endif
}


OIIO_FORCEINLINE const bool8& operator&= (bool8& a, const bool8 &b) {
    return a = a & b;
}

OIIO_FORCEINLINE const bool8& operator|= (bool8& a, const bool8& b) {
    return a = a | b;
}

OIIO_FORCEINLINE const bool8& operator^= (bool8& a, const bool8& b) {
    return a = a ^ b;
}


OIIO_FORCEINLINE bool8 operator~ (const bool8& a) {
#if OIIO_SIMD_AVX
    // Fastest way to bit-complement in SSE is to xor with 0xffffffff.
    return _mm256_xor_ps (a.simd(), bool8::True());
#else
    SIMD_RETURN (bool8, ~a[i]);
#endif
}


OIIO_FORCEINLINE bool8 operator== (const bool8 & a, const bool8 & b) {
#if OIIO_SIMD_AVX >= 2
    return _mm256_castsi256_ps (_mm256_cmpeq_epi32 (_mm256_castps_si256 (a), _mm256_castps_si256(b)));
#elif OIIO_SIMD_AVX
    return _mm256_cmp_ps (a, b, _CMP_EQ_UQ);
#else
    SIMD_RETURN (bool8, a[i] == b[i] ? -1 : 0);
#endif
}

OIIO_FORCEINLINE bool8 operator!= (const bool8 & a, const bool8 & b) {
#if OIIO_SIMD_AVX
    return _mm256_xor_ps (a, b);
#else
    SIMD_RETURN (bool8, a[i] != b[i] ? -1 : 0);
#endif
}


template<int i0, int i1, int i2, int i3, int i4, int i5, int i6, int i7>
OIIO_FORCEINLINE bool8 shuffle (const bool8& a) {
#if OIIO_SIMD_AVX >= 2
    int8 index (i0, i1, i2, i3, i4, i5, i6, i7);
    return _mm256_permutevar8x32_ps (a.simd(), index.simd());
#else
    return bool8 (a[i0], a[i1], a[i2], a[i3], a[i4], a[i5], a[i6], a[i7]);
#endif
}

template<int i> OIIO_FORCEINLINE bool8 shuffle (const bool8& a) {
    return shuffle<i,i,i,i,i,i,i,i>(a);
}


template<int i>
OIIO_FORCEINLINE bool extract (const bool8& a) {
#if OIIO_SIMD_AVX && !_WIN32
    return _mm256_extract_epi32(_mm256_castps_si256(a.simd()), i);  // SSE4.1 only
#else
    return a[i];
#endif
}

template<int i>
OIIO_FORCEINLINE bool8 insert (const bool8& a, bool val) {
#if OIIO_SIMD_AVX && !_WIN32
    int ival = -int(val);
    return _mm256_castsi256_ps (_mm256_insert_epi32 (_mm256_castps_si256(a.simd()), ival, i));
#else
    bool8 tmp = a;
    tmp[i] = -int(val);
    return tmp;
#endif
}


OIIO_FORCEINLINE bool reduce_and (const bool8& v) {
#if OIIO_SIMD_AVX
    return _mm256_testc_ps (v, bool8(true)) != 0;
    // return _mm256_movemask_ps(v.simd()) == 0xff;
#else
    SIMD_RETURN_REDUCE (bool, true, r &= bool(v[i]));
#endif
}

OIIO_FORCEINLINE bool reduce_or (const bool8& v) {
#if OIIO_SIMD_AVX
    return ! _mm256_testz_ps (v, v);   // FIXME? Not in all immintrin.h !
    // return _mm256_movemask_ps(v) != 0;
#else
    SIMD_RETURN_REDUCE (bool, false, r |= bool(v[i]));
#endif
}


OIIO_FORCEINLINE bool all (const bool8& v) { return reduce_and(v) == true; }
OIIO_FORCEINLINE bool any (const bool8& v) { return reduce_or(v) == true; }
OIIO_FORCEINLINE bool none (const bool8& v) { return reduce_or(v) == false; }



//////////////////////////////////////////////////////////////////////
// int4 implementation

OIIO_FORCEINLINE const int4 & int4::operator= (const int4& other) {
    m_simd = other.m_simd;
    return *this;
}

OIIO_FORCEINLINE int& int4::operator[] (int i) {
    DASSERT(i<elements);
    return m_val[i];
}

OIIO_FORCEINLINE int int4::operator[] (int i) const {
    DASSERT(i<elements);
    return m_val[i];
}


OIIO_FORCEINLINE void int4::load (int a) {
#if OIIO_SIMD_SSE
    m_simd = _mm_set1_epi32 (a);
#else
    SIMD_CONSTRUCT (a);
#endif
}



OIIO_FORCEINLINE void int4::load (int a, int b, int c, int d) {
#if OIIO_SIMD_SSE
    m_simd = _mm_set_epi32 (d, c, b, a);
#else
    m_val[0] = a;
    m_val[1] = b;
    m_val[2] = c;
    m_val[3] = d;
#endif
}


// OIIO_FORCEINLINE void int4::load (int a, int b, int c, int d,
//                                   int e, int f, int g, int h) {
//     load (a, b, c, d);
// }



OIIO_FORCEINLINE void int4::load (const int *values) {
#if OIIO_SIMD_SSE
    m_simd = _mm_loadu_si128 ((const simd_t *)values);
#else
    SIMD_CONSTRUCT (values[i]);
#endif
}


OIIO_FORCEINLINE void int4::load (const int *values, int n)
{
#if OIIO_SIMD_SSE
    switch (n) {
    case 1:
        m_simd = _mm_castps_si128 (_mm_load_ss ((const float *)values));
        break;
    case 2:
        // Trickery: load one double worth of bits!
        m_simd = _mm_castpd_si128 (_mm_load_sd ((const double*)values));
        break;
    case 3:
        // Trickery: load one double worth of bits, then a float,
        // and combine, casting to ints.
        m_simd = _mm_castps_si128 (_mm_movelh_ps(_mm_castpd_ps(_mm_load_sd((const double*)values)),
                                                _mm_load_ss ((const float *)values + 2)));
        break;
    case 4:
        m_simd = _mm_loadu_si128 ((const simd_t *)values);
        break;
    default:
        break;
    }
#else
    for (int i = 0; i < n; ++i)
        m_val[i] = values[i];
    for (int i = n; i < elements; ++i)
        m_val[i] = 0;
#endif
}


OIIO_FORCEINLINE void int4::load (const unsigned short *values) {
#if OIIO_SIMD_SSE >= 4
    // Trickery: load one double worth of bits = 4 uint16's!
    simd_t a = _mm_castpd_si128 (_mm_load_sd ((const double *)values));
    m_simd = _mm_cvtepu16_epi32 (a);
#else
    SIMD_CONSTRUCT (values[i]);
#endif
}


OIIO_FORCEINLINE void int4::load (const short *values) {
#if OIIO_SIMD_SSE >= 4
    // Trickery: load one double worth of bits = 4 int16's!
    simd_t a = _mm_castpd_si128 (_mm_load_sd ((const double *)values));
    m_simd = _mm_cvtepi16_epi32 (a);
#else
    SIMD_CONSTRUCT (values[i]);
#endif
}


OIIO_FORCEINLINE void int4::load (const unsigned char *values) {
#if OIIO_SIMD_SSE >= 4
    // Trickery: load one float worth of bits = 4 uint8's!
    simd_t a = _mm_castps_si128 (_mm_load_ss ((const float *)values));
    m_simd = _mm_cvtepu8_epi32 (a);
#else
    SIMD_CONSTRUCT (values[i]);
#endif
}


OIIO_FORCEINLINE void int4::load (const char *values) {
#if OIIO_SIMD_SSE >= 4
    // Trickery: load one float worth of bits = 4 uint8's!
    simd_t a = _mm_castps_si128 (_mm_load_ss ((const float *)values));
    m_simd = _mm_cvtepi8_epi32 (a);
#else
    SIMD_CONSTRUCT (values[i]);
#endif
}


OIIO_FORCEINLINE int4::int4 (int a) { load(a); }

OIIO_FORCEINLINE int4::int4 (int a, int b) { load(a,a,b,b); }

OIIO_FORCEINLINE int4::int4 (int a, int b, int c, int d) { load(a,b,c,d); }

// OIIO_FORCEINLINE int4::int4 (int a, int b, int c, int d,
//                              int e, int f, int g, int h) {
//     load(a,b,c,d,e,f,g,h);
// }

OIIO_FORCEINLINE int4::int4 (const int *vals) { load (vals); }
OIIO_FORCEINLINE int4::int4 (const unsigned short *vals) { load(vals); }
OIIO_FORCEINLINE int4::int4 (const short *vals) { load(vals); }
OIIO_FORCEINLINE int4::int4 (const unsigned char *vals) { load(vals); }
OIIO_FORCEINLINE int4::int4 (const char *vals) { load(vals); }

OIIO_FORCEINLINE const int4 & int4::operator= (int a) { load(a); return *this; }


OIIO_FORCEINLINE void int4::store (int *values) const {
#if OIIO_SIMD_SSE
    // Use an unaligned store -- it's just as fast when the memory turns
    // out to be aligned, nearly as fast even when unaligned. Not worth
    // the headache of using stores that require alignment.
    _mm_storeu_si128 ((simd_t *)values, m_simd);
#else
    SIMD_DO (values[i] = m_val[i]);
#endif
}


OIIO_FORCEINLINE void int4::clear () {
#if OIIO_SIMD_SSE
    m_simd = _mm_setzero_si128();
#else
    *this = 0;
#endif
}



OIIO_FORCEINLINE const int4 int4::Zero () {
#if OIIO_SIMD_SSE
    return _mm_setzero_si128();
#else
    return 0;
#endif
}


OIIO_FORCEINLINE const int4 int4::One () { return int4(1); }

OIIO_FORCEINLINE const int4 int4::NegOne () {
#if OIIO_SIMD_SSE
    // Fastest way to fill an __m128 with all 1 bits is to cmpeq_epi8
    // any value to itself.
# if OIIO_SIMD_AVX && (OIIO_GNUC_VERSION > 50000)
    __m128i anyval = _mm_undefined_si128();
# else
    __m128i anyval = _mm_setzero_si128();
# endif
    return _mm_cmpeq_epi8 (anyval, anyval);
#else
    return int4(-1);
#endif
}



OIIO_FORCEINLINE const int4 int4::Iota (int start, int step) {
    return int4 (start+0*step, start+1*step, start+2*step, start+3*step);
}


OIIO_FORCEINLINE int4 operator+ (const int4& a, const int4& b) {
#if OIIO_SIMD_SSE
    return _mm_add_epi32 (a.simd(), b.simd());
#else
    SIMD_RETURN (int4, a[i] + b[i]);
#endif
}

OIIO_FORCEINLINE const int4& operator+= (int4& a, const int4& b) {
    return a = a + b;
}


OIIO_FORCEINLINE int4 operator- (const int4& a) {
#if OIIO_SIMD_SSE
    return _mm_sub_epi32 (_mm_setzero_si128(), a);
#else
    SIMD_RETURN (int4, -a[i]);
#endif
}


OIIO_FORCEINLINE int4 operator- (const int4& a, const int4& b) {
#if OIIO_SIMD_SSE
    return _mm_sub_epi32 (a.simd(), b.simd());
#else
    SIMD_RETURN (int4, a[i] - b[i]);
#endif
}


OIIO_FORCEINLINE const int4 &operator-= (int4& a, const int4& b) {
    return a = a - b;
}


#if OIIO_SIMD_SSE
// Shamelessly lifted from Syrah which lifted from Manta which lifted it
// from intel.com
OIIO_FORCEINLINE __m128i mul_epi32 (__m128i a, __m128i b) {
#if OIIO_SIMD_SSE >= 4  /* SSE >= 4.1 */
    return _mm_mullo_epi32(a, b);
#else
    // Prior to SSE 4.1, there is no _mm_mullo_epi32 instruction, so we have
    // to fake it.
    __m128i t0;
    __m128i t1;
    t0 = _mm_mul_epu32 (a, b);
    t1 = _mm_mul_epu32 (_mm_shuffle_epi32 (a, 0xB1),
                        _mm_shuffle_epi32 (b, 0xB1));
    t0 = _mm_shuffle_epi32 (t0, 0xD8);
    t1 = _mm_shuffle_epi32 (t1, 0xD8);
    return _mm_unpacklo_epi32 (t0, t1);
#endif
}
#endif


OIIO_FORCEINLINE int4 operator* (const int4& a, const int4& b) {
#if OIIO_SIMD_SSE
    return mul_epi32 (a.simd(), b.simd());
#else
    SIMD_RETURN (int4, a[i] * b[i]);
#endif
}


OIIO_FORCEINLINE const int4& operator*= (int4& a, const int4& b) { return a = a * b; }
OIIO_FORCEINLINE const int4& operator*= (int4& a, int b) { return a = a * b; }


OIIO_FORCEINLINE int4 operator/ (const int4& a, const int4& b) {
    // NO INTEGER DIVISION IN SSE!
    SIMD_RETURN (int4, a[i] / b[i]);
}


OIIO_FORCEINLINE const int4& operator/= (int4& a, const int4& b) { return a = a / b; }

OIIO_FORCEINLINE int4 operator% (const int4& a, const int4& b) {
    // NO INTEGER MODULUS IN SSE!
    SIMD_RETURN (int4, a[i] % b[i]);
}



OIIO_FORCEINLINE const int4& operator%= (int4& a, const int4& b) { return a = a % b; }


OIIO_FORCEINLINE int4 operator% (const int4& a, int w) {
    // NO INTEGER MODULUS in SSE!
    SIMD_RETURN (int4, a[i] % w);
}


OIIO_FORCEINLINE const int4& operator%= (int4& a, int b) { return a = a % b; }


OIIO_FORCEINLINE int4 operator& (const int4& a, const int4& b) {
#if OIIO_SIMD_SSE
    return _mm_and_si128 (a.simd(), b.simd());
#else
    SIMD_RETURN (int4, a[i] & b[i]);
#endif
}


OIIO_FORCEINLINE const int4& operator&= (int4& a, const int4& b) { return a = a & b; }



OIIO_FORCEINLINE int4 operator| (const int4& a, const int4& b) {
#if OIIO_SIMD_SSE
    return _mm_or_si128 (a.simd(), b.simd());
#else
    SIMD_RETURN (int4, a[i] | b[i]);
#endif
}

OIIO_FORCEINLINE const int4& operator|= (int4& a, const int4& b) { return a = a | b; }


OIIO_FORCEINLINE int4 operator^ (const int4& a, const int4& b) {
#if OIIO_SIMD_SSE
    return _mm_xor_si128 (a.simd(), b.simd());
#else
    SIMD_RETURN (int4, a[i] ^ b[i]);
#endif
}


OIIO_FORCEINLINE const int4& operator^= (int4& a, const int4& b) { return a = a ^ b; }


OIIO_FORCEINLINE int4 operator~ (const int4& a) {
#if OIIO_SIMD_SSE
    return a ^ a.NegOne();
#else
    SIMD_RETURN (int4, ~a[i]);
#endif
}

OIIO_FORCEINLINE int4 operator<< (const int4& a, unsigned int bits) {
#if OIIO_SIMD_SSE
    return _mm_slli_epi32 (a, bits);
#else
    SIMD_RETURN (int4, a[i] << bits);
#endif
}

OIIO_FORCEINLINE const int4& operator<<= (int4& a, const unsigned int bits) {
    return a = a << bits;
}


OIIO_FORCEINLINE int4 operator>> (const int4& a, const unsigned int bits) {
#if OIIO_SIMD_SSE
    return _mm_srai_epi32 (a, bits);
#else
    SIMD_RETURN (int4, a[i] >> bits);
#endif
}

OIIO_FORCEINLINE const int4& operator>>= (int4& a, const unsigned int bits) {
    return a = a >> bits;
}


OIIO_FORCEINLINE int4 srl (const int4& a, const unsigned int bits) {
#if OIIO_SIMD_SSE
    return _mm_srli_epi32 (a, bits);
#else
    SIMD_RETURN (int4, int ((unsigned int)(a[i]) >> bits));
#endif
}


OIIO_FORCEINLINE bool4 operator== (const int4& a, const int4& b) {
#if OIIO_SIMD_SSE
    return _mm_castsi128_ps(_mm_cmpeq_epi32 (a, b));
#else
    SIMD_RETURN (bool4, a[i] == b[i] ? -1 : 0);
#endif
}

OIIO_FORCEINLINE bool4 operator!= (const int4& a, const int4& b) {
    return ! (a == b);
}


OIIO_FORCEINLINE bool4 operator> (const int4& a, const int4& b) {
#if OIIO_SIMD_SSE
    return _mm_castsi128_ps(_mm_cmpgt_epi32 (a, b));
#else
    SIMD_RETURN (bool4, a[i] > b[i] ? -1 : 0);
#endif
}

OIIO_FORCEINLINE bool4 operator< (const int4& a, const int4& b) {
#if OIIO_SIMD_SSE
    return _mm_castsi128_ps(_mm_cmplt_epi32 (a, b));
#else
    SIMD_RETURN (bool4, a[i] < b[i] ? -1 : 0);
#endif
}

OIIO_FORCEINLINE bool4 operator>= (const int4& a, const int4& b) {
    return (b < a) | (a == b);
}

OIIO_FORCEINLINE bool4 operator<= (const int4& a, const int4& b) {
    return (b > a) | (a == b);
}

inline std::ostream& operator<< (std::ostream& cout, const int4& val) {
    cout << val[0];
    for (int i = 1; i < val.elements; ++i)
        cout << ' ' << val[i];
    return cout;
}


// FIXME(SSE,AVX): is there a faster way to do a partial store? 512!

OIIO_FORCEINLINE void int4::store (int *values, int n) const {
    DASSERT (n >= 0 && n <= elements);
#if defined(OIIO_SIMD)
    // For full SIMD, there is a speed advantage to storing all components.
    if (n == elements)
        store (values);
    else
#endif
        for (int i = 0; i < n; ++i)
            values[i] = m_val[i];
}

// FIXME(SSE,AVX): is there a faster way to do a partial store? 512!

OIIO_FORCEINLINE void int4::store (unsigned short *values) const {
#if OIIO_SIMD_SSE
    // Expressed as half-words and considering little endianness, we
    // currently have xAxBxCxD (the 'x' means don't care).
    int4 clamped = m_val & int4(0xffff);   // A0B0C0D0
    int4 low = _mm_shufflelo_epi16 (clamped, (0<<0) | (2<<2) | (1<<4) | (1<<6));
                    // low = AB00xxxx
    int4 high = _mm_shufflehi_epi16 (clamped, (1<<0) | (1<<2) | (0<<4) | (2<<6));
                    // high = xxxx00CD
    int4 highswapped = shuffle_sse<2,3,0,1>(high);  // 00CDxxxx
    int4 result = low | highswapped;   // ABCDxxxx
    _mm_storel_pd ((double *)values, _mm_castsi128_pd(result));
    // At this point, values[] should hold A,B,C,D
#else
    SIMD_DO (values[i] = m_val[i]);
#endif
}



OIIO_FORCEINLINE void int4::store (unsigned char *values) const {
#if OIIO_SIMD_SSE
    // Expressed as bytes and considering little endianness, we
    // currently have xAxBxCxD (the 'x' means don't care).
    int4 clamped = m_val & int4(0xff);            // A000 B000 C000 D000
    int4 swapped = shuffle_sse<1,0,3,2>(clamped); // B000 A000 D000 C000
    int4 shifted = swapped << 8;                  // 0B00 0A00 0D00 0C00
    int4 merged = clamped | shifted;              // AB00 xxxx CD00 xxxx
    int4 merged2 = shuffle_sse<2,2,2,2>(merged);  // CD00 ...
    int4 shifted2 = merged2 << 16;                // 00CD ...
    int4 result = merged | shifted2;              // ABCD ...
    *(int*)values = result[0]; //extract<0>(result);
    // At this point, values[] should hold A,B,C,D
#else
    SIMD_DO (values[i] = m_val[i]);
#endif
}




template<int i0, int i1, int i2, int i3>
OIIO_FORCEINLINE int4 shuffle (const int4& a) {
#if OIIO_SIMD_SSE
    return shuffle_sse<i0,i1,i2,i3> (__m128i(a));
#else
    return int4(a[i0], a[i1], a[i2], a[i3]);
#endif
}

template<int i> OIIO_FORCEINLINE int4 shuffle (const int4& a) { return shuffle<i,i,i,i>(a); }


template<int i>
OIIO_FORCEINLINE int extract (const int4& v) {
#if OIIO_SIMD_SSE >= 4
    return _mm_extract_epi32(v.simd(), i);  // SSE4.1 only
#else
    return v[i];
#endif
}

#if OIIO_SIMD_SSE
template<> OIIO_FORCEINLINE int extract<0> (const int4& v) {
    return _mm_cvtsi128_si32(v.simd());
}
#endif

template<int i>
OIIO_FORCEINLINE int4 insert (const int4& a, int val) {
#if OIIO_SIMD_SSE >= 4
    return _mm_insert_epi32 (a.simd(), val, i);
#else
    int4 tmp = a;
    tmp[i] = val;
    return tmp;
#endif
}



OIIO_FORCEINLINE int int4::x () const { return extract<0>(*this); }
OIIO_FORCEINLINE int int4::y () const { return extract<1>(*this); }
OIIO_FORCEINLINE int int4::z () const { return extract<2>(*this); }
OIIO_FORCEINLINE int int4::w () const { return extract<3>(*this); }
OIIO_FORCEINLINE void int4::set_x (int val) { *this = insert<0>(*this, val); }
OIIO_FORCEINLINE void int4::set_y (int val) { *this = insert<1>(*this, val); }
OIIO_FORCEINLINE void int4::set_z (int val) { *this = insert<2>(*this, val); }
OIIO_FORCEINLINE void int4::set_w (int val) { *this = insert<3>(*this, val); }


OIIO_FORCEINLINE int4 bitcast_to_int (const bool4& x)
{
#if OIIO_SIMD_SSE
    return _mm_castps_si128 (x.simd());
#else
    return *(int4 *)&x;
#endif
}

// Old names:
inline int4 bitcast_to_int4 (const bool4& x) { return bitcast_to_int(x); }


OIIO_FORCEINLINE int4 vreduce_add (const int4& v) {
#if OIIO_SIMD_SSE >= 3
    // People seem to agree that SSE3 does add reduction best with 2
    // horizontal adds.
    // suppose v = (a, b, c, d)
    simd::int4 ab_cd = _mm_hadd_epi32 (v.simd(), v.simd());
    // ab_cd = (a+b, c+d, a+b, c+d)
    simd::int4 abcd = _mm_hadd_epi32 (ab_cd.simd(), ab_cd.simd());
    // all abcd elements are a+b+c+d, return an element as fast as possible
    return abcd;
#elif OIIO_SIMD_SSE >= 2
    // I think this is the best we can do for SSE2, and I'm still not sure
    // it's faster than the default scalar operation. But anyway...
    // suppose v = (a, b, c, d)
    int4 ab_ab_cd_cd = shuffle<1,0,3,2>(v) + v;
    // ab_ab_cd_cd = (b,a,d,c) + (a,b,c,d) = (a+b,a+b,c+d,c+d)
    int4 cd_cd_ab_ab = shuffle<2,3,0,1>(ab_ab_cd_cd);
    // cd_cd_ab_ab = (c+d,c+d,a+b,a+b)
    int4 abcd = ab_ab_cd_cd + cd_cd_ab_ab;   // a+b+c+d in all components
    return abcd;
#else
    SIMD_RETURN_REDUCE (int4, 0, r += v[i]);
#endif
}


OIIO_FORCEINLINE int reduce_add (const int4& v) {
#if OIIO_SIMD_SSE
    return extract<0> (vreduce_add(v));
#else
    SIMD_RETURN_REDUCE (int, 0, r += v[i]);
#endif
}


OIIO_FORCEINLINE int reduce_and (const int4& v) {
#if OIIO_SIMD_SSE
    int4 ab = v & shuffle<1,1,3,3>(v); // ab bb cd dd
    int4 abcd = ab & shuffle<2>(ab);
    return extract<0>(abcd);
#else
    SIMD_RETURN_REDUCE (int, -1, r &= v[i]);
#endif
}


OIIO_FORCEINLINE int reduce_or (const int4& v) {
#if OIIO_SIMD_SSE
    int4 ab = v | shuffle<1,1,3,3>(v); // ab bb cd dd
    int4 abcd = ab | shuffle<2>(ab);
    return extract<0>(abcd);
#else
    SIMD_RETURN_REDUCE (int, 0, r |= v[i]);
#endif
}



OIIO_FORCEINLINE int4 blend (const int4& a, const int4& b, const bool4& mask) {
#if OIIO_SIMD_SSE >= 4 /* SSE >= 4.1 */
    return _mm_castps_si128 (_mm_blendv_ps (_mm_castsi128_ps(a.simd()),
                                            _mm_castsi128_ps(b.simd()), mask));
#elif OIIO_SIMD_SSE
    return _mm_or_si128 (_mm_and_si128(_mm_castps_si128(mask.simd()), b.simd()),
                         _mm_andnot_si128(_mm_castps_si128(mask.simd()), a.simd()));
#else
    SIMD_RETURN (int4, mask[i] ? b[i] : a[i]);
#endif
}

OIIO_FORCEINLINE int4 blend0 (const int4& a, const bool4& mask) {
#if OIIO_SIMD_SSE
    return _mm_and_si128(_mm_castps_si128(mask), a.simd());
#else
    SIMD_RETURN (int4, mask[i] ? a[i] : 0.0f);
#endif
}


OIIO_FORCEINLINE int4 blend0not (const int4& a, const bool4& mask) {
#if OIIO_SIMD_SSE
    return _mm_andnot_si128(_mm_castps_si128(mask), a.simd());
#else
    SIMD_RETURN (int4, mask[i] ? 0.0f : a[i]);
#endif
}


OIIO_FORCEINLINE int4 select (const bool4& mask, const int4& a, const int4& b) {
    return blend (b, a, mask);
}



OIIO_FORCEINLINE int4 abs (const int4& a) {
#if OIIO_SIMD_SSE >= 3
    return _mm_abs_epi32(a.simd());
#else
    SIMD_RETURN (int4, std::abs(a[i]));
#endif
}



OIIO_FORCEINLINE int4 min (const int4& a, const int4& b) {
#if OIIO_SIMD_SSE >= 4 /* SSE >= 4.1 */
    return _mm_min_epi32 (a, b);
#else
    SIMD_RETURN (int4, std::min(a[i], b[i]));
#endif
}


OIIO_FORCEINLINE int4 max (const int4& a, const int4& b) {
#if OIIO_SIMD_SSE >= 4 /* SSE >= 4.1 */
    return _mm_max_epi32 (a, b);
#else
    SIMD_RETURN (int4, std::max(a[i], b[i]));
#endif
}


OIIO_FORCEINLINE int4 rotl32 (const int4& x, const unsigned int k) {
    return (x<<k) | srl(x,32-k);
}


OIIO_FORCEINLINE int4 andnot (const int4& a, const int4& b) {
#if OIIO_SIMD_SSE
    return _mm_andnot_si128 (a.simd(), b.simd());
#else
    SIMD_RETURN (int4, ~(a[i]) & b[i]);
#endif
}


// Implementation had to be after the definition of int4::Zero.
OIIO_FORCEINLINE bool4::bool4 (const int4& ival) {
    m_simd = (ival != int4::Zero());
}




//////////////////////////////////////////////////////////////////////
// int8 implementation

OIIO_FORCEINLINE const int8 & int8::operator= (const int8& other) {
    m_simd = other.m_simd;
    return *this;
}

OIIO_FORCEINLINE int& int8::operator[] (int i) {
    DASSERT(i<elements);
    return m_val[i];
}

OIIO_FORCEINLINE int int8::operator[] (int i) const {
    DASSERT(i<elements);
    return m_val[i];
}


OIIO_FORCEINLINE void int8::load (int a) {
#if OIIO_SIMD_AVX
    m_simd = _mm256_set1_epi32 (a);
#else
    SIMD_CONSTRUCT (a);
#endif
}


OIIO_FORCEINLINE void int8::load (int a, int b, int c, int d,
                                  int e, int f, int g, int h) {
#if OIIO_SIMD_AVX
    m_simd = _mm256_set_epi32 (h, g, f, e, d, c, b, a);
#else
    m_val[0] = a;
    m_val[1] = b;
    m_val[2] = c;
    m_val[3] = d;
    m_val[4] = e;
    m_val[5] = f;
    m_val[6] = g;
    m_val[7] = h;
#endif
}


OIIO_FORCEINLINE void int8::load (const int *values) {
#if OIIO_SIMD_AVX
    m_simd = _mm256_loadu_si256 ((const simd_t *)values);
#else
    SIMD_CONSTRUCT (values[i]);
#endif
}


OIIO_FORCEINLINE void int8::load (const int *values, int n)
{
    // FIXME: is this faster with AVX masked loads?
#if OIIO_SIMD_SSE
    if (n > 0 && n <= 4) {
        int4 l; l.load (values, n);
        m_simd = int8(l, int4::Zero());
    } else if (n > 4 && n <= 8) {
        int4 h; h.load (values+4, n-4);
        m_simd = int8(int4(values), h);
    }
    else
        clear();
#else
    for (int i = 0; i < n; ++i)
        m_val[i] = values[i];
    for (int i = n; i < elements; ++i)
        m_val[i] = 0;
#endif
}

// FIXME(AVX): fast load from unsigned short, short, unsigned char, char

OIIO_FORCEINLINE void int8::load (const short *values) {
    SIMD_CONSTRUCT (values[i]);
}

OIIO_FORCEINLINE void int8::load (const unsigned short *values) {
    SIMD_CONSTRUCT (values[i]);
}


OIIO_FORCEINLINE void int8::load (const char *values) {
    SIMD_CONSTRUCT (values[i]);
}

OIIO_FORCEINLINE void int8::load (const unsigned char *values) {
    SIMD_CONSTRUCT (values[i]);
}



OIIO_FORCEINLINE int8::int8 (int a) { load(a); }

// OIIO_FORCEINLINE int8::int8 (int a, int b, int c, int d) { load(a,b,c,d); }

OIIO_FORCEINLINE int8::int8 (int a, int b, int c, int d,
                             int e, int f, int g, int h) {
    load(a,b,c,d,e,f,g,h);
}

OIIO_FORCEINLINE int8::int8 (const int *vals) { load (vals); }
OIIO_FORCEINLINE int8::int8 (const unsigned short *vals) { load(vals); }
OIIO_FORCEINLINE int8::int8 (const short *vals) { load(vals); }
OIIO_FORCEINLINE int8::int8 (const unsigned char *vals) { load(vals); }
OIIO_FORCEINLINE int8::int8 (const char *vals) { load(vals); }

OIIO_FORCEINLINE const int8 & int8::operator= (int a) { load(a); return *this; }


OIIO_FORCEINLINE void int8::store (int *values) const {
#if OIIO_SIMD_AVX
    // Use an unaligned store -- it's just as fast when the memory turns
    // out to be aligned, nearly as fast even when unaligned. Not worth
    // the headache of using stores that require alignment.
    _mm256_storeu_si256 ((simd_t *)values, m_simd);
#else
    SIMD_DO (values[i] = m_val[i]);
#endif
}


OIIO_FORCEINLINE void int8::clear () {
#if OIIO_SIMD_AVX
    m_simd = _mm256_setzero_si256();
#else
    *this = 0;
#endif
}


OIIO_FORCEINLINE const int8 int8::Zero () {
#if OIIO_SIMD_AVX
    return _mm256_setzero_si256();
#else
    return 0;
#endif
}

OIIO_FORCEINLINE const int8 int8::One () { return int8(1); }

OIIO_FORCEINLINE const int8 int8::NegOne () { return int8(-1); }


OIIO_FORCEINLINE const int8 int8::Iota (int start, int step) {
    return int8 (start+0*step, start+1*step, start+2*step, start+3*step,
                 start+4*step, start+5*step, start+6*step, start+7*step);
}


OIIO_FORCEINLINE int4 int8::lo () const {
#if OIIO_SIMD_AVX
    return _mm256_castsi256_si128 (simd());
#else
    return m_4[0];
#endif
}

OIIO_FORCEINLINE int4 int8::hi () const {
#if OIIO_SIMD_AVX
    return _mm256_extractf128_si256 (simd(), 1);
#else
    return m_4[1];
#endif
}


OIIO_FORCEINLINE int8::int8 (const int4& lo, const int4 &hi) {
#if OIIO_SIMD_AVX
    __m256i r = _mm256_castsi128_si256 (lo);
    m_simd = _mm256_insertf128_si256 (r, hi, 1);
    // N.B. equivalent, if available: m_simd = _mm256_set_m128i (hi, lo);
#else
    m_4[0] = lo;
    m_4[1] = hi;
#endif
}


OIIO_FORCEINLINE int8 operator+ (const int8& a, const int8& b) {
#if OIIO_SIMD_AVX >= 2
    return _mm256_add_epi32 (a.simd(), b.simd());
#else
    SIMD_RETURN (int8, a[i] + b[i]);
#endif
}


OIIO_FORCEINLINE const int8& operator+= (int8& a, const int8& b) {
    return a = a + b;
}


OIIO_FORCEINLINE int8 operator- (const int8& a) {
#if OIIO_SIMD_AVX >= 2
    return _mm256_sub_epi32 (_mm256_setzero_si256(), a);
#else
    SIMD_RETURN (int8, -a[i]);
#endif
}


OIIO_FORCEINLINE int8 operator- (const int8& a, const int8& b) {
#if OIIO_SIMD_AVX >= 2
    return _mm256_sub_epi32 (a.simd(), b.simd());
#else
    SIMD_RETURN (int8, a[i] - b[i]);
#endif
}


OIIO_FORCEINLINE const int8 &operator-= (int8& a, const int8& b) {
    return a = a - b;
}


OIIO_FORCEINLINE int8 operator* (const int8& a, const int8& b) {
#if OIIO_SIMD_AVX >= 2
    return _mm256_mullo_epi32 (a.simd(), b.simd());
#else
    SIMD_RETURN (int8, a[i] * b[i]);
#endif
}


OIIO_FORCEINLINE const int8& operator*= (int8& a, const int8& b) { return a = a * b; }
OIIO_FORCEINLINE const int8& operator*= (int8& a, int b) { return a = a * b; }


OIIO_FORCEINLINE int8 operator/ (const int8& a, const int8& b) {
    // NO INTEGER DIVISION IN SSE or AVX!
    SIMD_RETURN (int8, a[i] / b[i]);
}

OIIO_FORCEINLINE const int8& operator/= (int8& a, const int8& b) { return a = a / b; }


OIIO_FORCEINLINE int8 operator% (const int8& a, const int8& b) {
    // NO INTEGER MODULUS IN SSE or AVX!
    SIMD_RETURN (int8, a[i] % b[i]);
}

OIIO_FORCEINLINE const int8& operator%= (int8& a, const int8& b) { return a = a % b; }

OIIO_FORCEINLINE int8 operator% (const int8& a, int w) {
    // NO INTEGER MODULUS in SSE or AVX!
    SIMD_RETURN (int8, a[i] % w);
}

OIIO_FORCEINLINE const int8& operator%= (int8& a, int b) { return a = a % b; }


OIIO_FORCEINLINE int8 operator& (const int8& a, const int8& b) {
#if OIIO_SIMD_AVX >= 2
    return _mm256_and_si256 (a.simd(), b.simd());
#else
    SIMD_RETURN (int8, a[i] & b[i]);
#endif
}

OIIO_FORCEINLINE const int8& operator&= (int8& a, const int8& b) { return a = a & b; }

OIIO_FORCEINLINE int8 operator| (const int8& a, const int8& b) {
#if OIIO_SIMD_AVX >= 2
    return _mm256_or_si256 (a.simd(), b.simd());
#else
    SIMD_RETURN (int8, a[i] | b[i]);
#endif
}

OIIO_FORCEINLINE const int8& operator|= (int8& a, const int8& b) { return a = a | b; }

OIIO_FORCEINLINE int8 operator^ (const int8& a, const int8& b) {
#if OIIO_SIMD_AVX >= 2
    return _mm256_xor_si256 (a.simd(), b.simd());
#else
    SIMD_RETURN (int8, a[i] ^ b[i]);
#endif
}

OIIO_FORCEINLINE const int8& operator^= (int8& a, const int8& b) { return a = a ^ b; }


OIIO_FORCEINLINE int8 operator~ (const int8& a) {
#if OIIO_SIMD_AVX >= 2
    return a ^ a.NegOne();
#else
    SIMD_RETURN (int8, ~a[i]);
#endif
}


OIIO_FORCEINLINE int8 operator<< (const int8& a, unsigned int bits) {
#if OIIO_SIMD_AVX >= 2
    return _mm256_slli_epi32 (a, bits);
#elif OIIO_SIMD_SSE
    return int8 (a.lo() << bits, a.hi() << bits);
#else
    SIMD_RETURN (int8, a[i] << bits);
#endif
}


OIIO_FORCEINLINE const int8& operator<<= (int8& a, const unsigned int bits) {
    return a = a << bits;
}

OIIO_FORCEINLINE int8 operator>> (const int8& a, const unsigned int bits) {
#if OIIO_SIMD_AVX >= 2
    return _mm256_srai_epi32 (a, bits);
#elif OIIO_SIMD_SSE
    return int8 (a.lo() >> bits, a.hi() >> bits);
#else
    SIMD_RETURN (int8, a[i] >> bits);
#endif
}

OIIO_FORCEINLINE const int8& operator>>= (int8& a, const unsigned int bits) {
    return a = a >> bits;
}


OIIO_FORCEINLINE int8 srl (const int8& a, const unsigned int bits) {
#if OIIO_SIMD_AVX >= 2
    return _mm256_srli_epi32 (a, bits);
#else
    SIMD_RETURN (int8, int ((unsigned int)(a[i]) >> bits));
#endif
}


OIIO_FORCEINLINE bool8 operator== (const int8& a, const int8& b) {
#if OIIO_SIMD_AVX >= 2
    return _mm256_castsi256_ps(_mm256_cmpeq_epi32 (a.m_simd, b.m_simd));
#elif OIIO_SIMD_SSE  /* Fall back to 4-wide */
    return bool8 (a.lo() == b.lo(), a.hi() == b.hi());
#else
    SIMD_RETURN (bool8, a[i] == b[i] ? -1 : 0);
#endif
}


OIIO_FORCEINLINE bool8 operator!= (const int8& a, const int8& b) {
    return ! (a == b);
}


OIIO_FORCEINLINE bool8 operator> (const int8& a, const int8& b) {
#if OIIO_SIMD_AVX >= 2
    return _mm256_castsi256_ps(_mm256_cmpgt_epi32 (a, b));
#elif OIIO_SIMD_SSE  /* Fall back to 4-wide */
    return bool8 (a.lo() > b.lo(), a.hi() > b.hi());
#else
    SIMD_RETURN (bool8, a[i] > b[i] ? -1 : 0);
#endif
}


OIIO_FORCEINLINE bool8 operator< (const int8& a, const int8& b) {
#if OIIO_SIMD_AVX >= 2
    // No lt or lte!
    return (b > a);
#elif OIIO_SIMD_SSE  /* Fall back to 4-wide */
    return bool8 (a.lo() < b.lo(), a.hi() < b.hi());
#else
    SIMD_RETURN (bool8, a[i] < b[i] ? -1 : 0);
#endif
}


OIIO_FORCEINLINE bool8 operator>= (const int8& a, const int8& b) {
    return (a > b) | (a == b);
}


OIIO_FORCEINLINE bool8 operator<= (const int8& a, const int8& b) {
    return (b > a) | (a == b);
}


inline std::ostream& operator<< (std::ostream& cout, const int8& val) {
    cout << val[0];
    for (int i = 1; i < val.elements; ++i)
        cout << ' ' << val[i];
    return cout;
}


OIIO_FORCEINLINE void int8::store (int *values, int n) const {
    DASSERT (n >= 0 && n <= elements);
    // FIXME: is this faster with AVX masked stores?
#if OIIO_SIMD_SSE
    if (n <= 4) {
        lo().store (values, n);
    } else if (n <= 8) {
        lo().store (values);
        hi().store (values+4, n-4);
    }
#endif
        for (int i = 0; i < n; ++i)
            values[i] = m_val[i];
}


// FIXME(AVX): fast int8 store to unsigned short, unsigned char

OIIO_FORCEINLINE void int8::store (unsigned short *values) const {
#if 0 && OIIO_SIMD_AVX >= 2
    // FIXME -- try to determine if this is faster:
    lo().store (values);
    hi().store (values+4);
#else
    SIMD_DO (values[i] = m_val[i]);
#endif
}


OIIO_FORCEINLINE void int8::store (unsigned char *values) const {
#if OIIO_SIMD_SSE
    lo().store (values);
    hi().store (values+4);
#else
    SIMD_DO (values[i] = m_val[i]);
#endif
}


template<int i0, int i1, int i2, int i3, int i4, int i5, int i6, int i7>
OIIO_FORCEINLINE int8 shuffle (const int8& a) {
#if OIIO_SIMD_AVX >= 2
    int8 index (i0, i1, i2, i3, i4, i5, i6, i7);
    return _mm256_castps_si256 (_mm256_permutevar8x32_ps (_mm256_castsi256_ps(a.simd()), index.simd()));
#else
    return int8 (a[i0], a[i1], a[i2], a[i3], a[i4], a[i5], a[i6], a[i7]);
#endif
}

template<int i> OIIO_FORCEINLINE int8 shuffle (const int8& a) {
    return shuffle<i,i,i,i,i,i,i,i>(a);
}


template<int i>
OIIO_FORCEINLINE int extract (const int8& v) {
#if OIIO_SIMD_AVX && !_WIN32
    return _mm256_extract_epi32(v.simd(), i);
#else
    return v[i];
#endif
}


template<int i>
OIIO_FORCEINLINE int8 insert (const int8& a, int val) {
#if OIIO_SIMD_AVX && !_WIN32
    return _mm256_insert_epi32 (a.simd(), val, i);
#else
    int8 tmp = a;
    tmp[i] = val;
    return tmp;
#endif
}


OIIO_FORCEINLINE int int8::x () const { return extract<0>(*this); }
OIIO_FORCEINLINE int int8::y () const { return extract<1>(*this); }
OIIO_FORCEINLINE int int8::z () const { return extract<2>(*this); }
OIIO_FORCEINLINE int int8::w () const { return extract<3>(*this); }
OIIO_FORCEINLINE void int8::set_x (int val) { *this = insert<0>(*this, val); }
OIIO_FORCEINLINE void int8::set_y (int val) { *this = insert<1>(*this, val); }
OIIO_FORCEINLINE void int8::set_z (int val) { *this = insert<2>(*this, val); }
OIIO_FORCEINLINE void int8::set_w (int val) { *this = insert<3>(*this, val); }


OIIO_FORCEINLINE int8 bitcast_to_int (const bool8& x)
{
#if OIIO_SIMD_AVX
    return _mm256_castps_si256 (x.simd());
#else
    return *(int8 *)&x;
#endif
}


OIIO_FORCEINLINE int8 vreduce_add (const int8& v) {
#if OIIO_SIMD_AVX >= 2
    // From Syrah:
    int8 ab_cd_0_0_ef_gh_0_0 = _mm256_hadd_epi32(v.simd(), _mm256_setzero_si256());
    int8 abcd_0_0_0_efgh_0_0_0 = _mm256_hadd_epi32(ab_cd_0_0_ef_gh_0_0, _mm256_setzero_si256());
    // get efgh in the 0-idx slot
    int8 efgh = shuffle<4>(abcd_0_0_0_efgh_0_0_0);
    int8 final_sum = abcd_0_0_0_efgh_0_0_0 + efgh;
    return shuffle<0>(final_sum);
#elif OIIO_SIMD_SSE
    int4 hadd4 = vreduce_add(v.lo()) + vreduce_add(v.hi());
    return int8(hadd4, hadd4);
#else
    SIMD_RETURN_REDUCE (int8, 0, r += v[i]);
#endif
}


OIIO_FORCEINLINE int reduce_add (const int8& v) {
#if OIIO_SIMD_SSE
    return extract<0> (vreduce_add(v));
#else
    SIMD_RETURN_REDUCE (int, 0, r += v[i]);
#endif
}


OIIO_FORCEINLINE int reduce_and (const int8& v) {
#if OIIO_SSE_AVX >= 2
    int8 ab = v & shuffle<1,1,3,3,5,5,7,7>(v); // ab bb cd dd ef ff gh hh
    int8 abcd = ab & shuffle<2,2,2,2,6,6,6,6>(ab); // abcd x x x efgh x x x
    int8 abcdefgh = abcd & shuffle<4>(abcdefgh); // abcdefgh x x x x x x x
    return extract<0> (abcdefgh);
#else
    // AVX 1.0 or less -- use SSE
    return reduce_and(v.lo() & v.hi());
#endif
}


OIIO_FORCEINLINE int reduce_or (const int8& v) {
#if OIIO_SSE_AVX >= 2
    int8 ab = v | shuffle<1,1,3,3,5,5,7,7>(v); // ab bb cd dd ef ff gh hh
    int8 abcd = ab | shuffle<2,2,2,2,6,6,6,6>(ab); // abcd x x x efgh x x x
    int8 abcdefgh = abcd | shuffle<4>(abcdefgh); // abcdefgh x x x x x x x
    return extract<0> (abcdefgh);
#else
    // AVX 1.0 or less -- use SSE
    return reduce_or(v.lo() | v.hi());
#endif
}


OIIO_FORCEINLINE int8 blend (const int8& a, const int8& b, const bool8& mask) {
#if OIIO_SIMD_AVX
    return _mm256_castps_si256 (_mm256_blendv_ps (_mm256_castsi256_ps(a.simd()),
                                                  _mm256_castsi256_ps(b.simd()), mask));
#else
    SIMD_RETURN (int8, mask[i] ? b[i] : a[i]);
#endif
}


OIIO_FORCEINLINE int8 blend0 (const int8& a, const bool8& mask) {
#if OIIO_SIMD_AVX
    return _mm256_castps_si256(_mm256_and_ps(_mm256_castsi256_ps(a.simd()), mask));
#else
    SIMD_RETURN (int8, mask[i] ? a[i] : 0.0f);
#endif
}


OIIO_FORCEINLINE int8 blend0not (const int8& a, const bool8& mask) {
#if OIIO_SIMD_AVX
    return _mm256_castps_si256 (_mm256_andnot_ps (mask.simd(), _mm256_castsi256_ps(a.simd())));
#else
    SIMD_RETURN (int8, mask[i] ? 0.0f : a[i]);
#endif
}

OIIO_FORCEINLINE int8 select (const bool8& mask, const int8& a, const int8& b) {
    return blend (b, a, mask);
}


OIIO_FORCEINLINE int8 abs (const int8& a) {
#if OIIO_SIMD_AVX >= 2
    return _mm256_abs_epi32(a.simd());
#else
    SIMD_RETURN (int8, std::abs(a[i]));
#endif
}


OIIO_FORCEINLINE int8 min (const int8& a, const int8& b) {
#if OIIO_SIMD_AVX >= 2
    return _mm256_min_epi32 (a, b);
#else
    SIMD_RETURN (int8, std::min(a[i], b[i]));
#endif
}


OIIO_FORCEINLINE int8 max (const int8& a, const int8& b) {
#if OIIO_SIMD_AVX >= 2
    return _mm256_max_epi32 (a, b);
#else
    SIMD_RETURN (int8, std::max(a[i], b[i]));
#endif
}


OIIO_FORCEINLINE int8 rotl32 (const int8& x, const unsigned int k) {
    return (x<<k) | srl(x,32-k);
}


OIIO_FORCEINLINE int8 andnot (const int8& a, const int8& b) {
#if OIIO_SIMD_AVX >= 2
    return _mm256_andnot_si256 (a.simd(), b.simd());
#else
    SIMD_RETURN (int8, ~(a[i]) & b[i]);
#endif
}


// Implementation had to be after the definition of int8::Zero.
OIIO_FORCEINLINE bool8::bool8 (const int8& ival) {
    m_simd = (ival != int8::Zero());
}




//////////////////////////////////////////////////////////////////////
// float4 implementation


OIIO_FORCEINLINE float4::float4 (const int4& ival) {
#if OIIO_SIMD_SSE
    m_simd = _mm_cvtepi32_ps (ival.simd());
#else
    SIMD_CONSTRUCT (float(ival[i]));
#endif
}


OIIO_FORCEINLINE const float4 float4::Zero () {
#if OIIO_SIMD_SSE
    return _mm_setzero_ps();
#else
    return float4(0.0f);
#endif
}

OIIO_FORCEINLINE const float4 float4::One () {
    return float4(1.0f);
}

OIIO_FORCEINLINE const float4 float4::Iota (float start, float step) {
    return float4 (start+0.0f*step, start+1.0f*step, start+2.0f*step, start+3.0f*step);
}

/// Set all components to 0.0
OIIO_FORCEINLINE void float4::clear () {
#if OIIO_SIMD_SSE
    m_simd = _mm_setzero_ps();
#else
    load (0.0f);
#endif
}

#if defined(ILMBASE_VERSION_MAJOR) && ILMBASE_VERSION_MAJOR >= 2
OIIO_FORCEINLINE const float4 & float4::operator= (const Imath::V4f &v) {
    load ((const float *)&v);
    return *this;
}
#endif

OIIO_FORCEINLINE const float4 & float4::operator= (const Imath::V3f &v) {
    load (v[0], v[1], v[2], 0.0f);
    return *this;
}

OIIO_FORCEINLINE float& float4::operator[] (int i) {
    DASSERT(i<elements);
    return m_val[i];
}

OIIO_FORCEINLINE float float4::operator[] (int i) const {
    DASSERT(i<elements);
    return m_val[i];
}


OIIO_FORCEINLINE void float4::load (float val) {
#if OIIO_SIMD_SSE
    m_simd = _mm_set1_ps (val);
#elif defined(OIIO_SIMD_NEON)
    m_simd = vdupq_n_f32 (val);
#else
    SIMD_CONSTRUCT (val);
#endif
}

OIIO_FORCEINLINE void float4::load (float a, float b, float c, float d) {
#if OIIO_SIMD_SSE
    m_simd = _mm_set_ps (d, c, b, a);
#elif defined(OIIO_SIMD_NEON)
    float values[4] = { a, b, c, d };
    m_simd = vld1q_f32 (values);
#else
    m_val[0] = a;
    m_val[1] = b;
    m_val[2] = c;
    m_val[3] = d;
#endif
}

    /// Load from an array of 4 values
OIIO_FORCEINLINE void float4::load (const float *values) {
#if OIIO_SIMD_SSE
    m_simd = _mm_loadu_ps (values);
#elif defined(OIIO_SIMD_NEON)
    m_simd = vld1q_f32 (values);
#else
    SIMD_CONSTRUCT (values[i]);
#endif
}


OIIO_FORCEINLINE void float4::load (const float *values, int n) {
#if OIIO_SIMD_SSE
    switch (n) {
    case 1:
        m_simd = _mm_load_ss (values);
        break;
    case 2:
        // Trickery: load one double worth of bits!
        m_simd = _mm_castpd_ps (_mm_load_sd ((const double*)values));
        break;
    case 3:
        m_simd = _mm_setr_ps (values[0], values[1], values[2], 0.0f);
        // This looks wasteful, but benchmarks show that it's the
        // fastest way to set 3 values with the 4th getting zero.
        // Actually, gcc and clang both turn it into something more
        // efficient than _mm_setr_ps. The version below looks smart,
        // but was much more expensive as the _mm_setr_ps!
        //   __m128 xy = _mm_castsi128_ps(_mm_loadl_epi64((const __m128i*)values));
        //   m_simd = _mm_movelh_ps(xy, _mm_load_ss (values + 2));
        break;
    case 4:
        m_simd = _mm_loadu_ps (values);
        break;
    default:
        break;
    }
#elif defined(OIIO_SIMD_NEON)
    switch (n) {
    case 1: m_simd = vdupq_n_f32 (val);                    break;
    case 2: load (values[0], values[1], 0.0f, 0.0f);      break;
    case 3: load (values[0], values[1], values[2], 0.0f); break;
    case 4: m_simd = vld1q_f32 (values);                   break;
    default: break;
    }
#else
    for (int i = 0; i < n; ++i)
        m_val[i] = values[i];
    for (int i = n; i < paddedelements; ++i)
        m_val[i] = 0;
#endif
}


OIIO_FORCEINLINE void float4::load (const unsigned short *values) {
#if defined(OIIO_SIMD_SSE) && OIIO_SIMD_SSE >= 2
    m_simd = _mm_cvtepi32_ps (int4(values).simd());
    // You might guess that the following is faster, but it's NOT:
    //   NO!  m_simd = _mm_cvtpu16_ps (*(__m64*)values);
#else
    SIMD_CONSTRUCT (values[i]);
#endif
}


OIIO_FORCEINLINE void float4::load (const short *values) {
#if defined(OIIO_SIMD_SSE) && OIIO_SIMD_SSE >= 2
    m_simd = _mm_cvtepi32_ps (int4(values).simd());
#else
    SIMD_CONSTRUCT (values[i]);
#endif
}


OIIO_FORCEINLINE void float4::load (const unsigned char *values) {
#if defined(OIIO_SIMD_SSE) && OIIO_SIMD_SSE >= 2
    m_simd = _mm_cvtepi32_ps (int4(values).simd());
#else
    SIMD_CONSTRUCT (values[i]);
#endif
}

    /// Load from an array of 4 char values, convert to float
OIIO_FORCEINLINE void float4::load (const char *values) {
#if defined(OIIO_SIMD_SSE) && OIIO_SIMD_SSE >= 2
    m_simd = _mm_cvtepi32_ps (int4(values).simd());
#else
    SIMD_CONSTRUCT (values[i]);
#endif
}

#ifdef _HALF_H_
OIIO_FORCEINLINE void float4::load (const half *values) {
#if defined(__F16C__) && defined(OIIO_SIMD_SSE)
    /* Enabled 16 bit float instructions! */
    __m128i a = _mm_castpd_si128 (_mm_load_sd ((const double *)values));
    m_simd = _mm_cvtph_ps (a);
#elif defined(OIIO_SIMD_SSE) && OIIO_SIMD_SSE >= 2
    // SSE half-to-float by Fabian "ryg" Giesen. Public domain.
    // https://gist.github.com/rygorous/2144712
    int4 h ((const unsigned short *)values);
# define CONSTI(name) *(const __m128i *)&name
# define CONSTF(name) *(const __m128 *)&name
    OIIO_SIMD_UINT4_CONST(mask_nosign, 0x7fff);
    OIIO_SIMD_UINT4_CONST(magic,       (254 - 15) << 23);
    OIIO_SIMD_UINT4_CONST(was_infnan,  0x7bff);
    OIIO_SIMD_UINT4_CONST(exp_infnan,  255 << 23);
    __m128i mnosign     = CONSTI(mask_nosign);
    __m128i expmant     = _mm_and_si128(mnosign, h);
    __m128i justsign    = _mm_xor_si128(h, expmant);
    __m128i expmant2    = expmant; // copy (just here for counting purposes)
    __m128i shifted     = _mm_slli_epi32(expmant, 13);
    __m128  scaled      = _mm_mul_ps(_mm_castsi128_ps(shifted), *(const __m128 *)&magic);
    __m128i b_wasinfnan = _mm_cmpgt_epi32(expmant2, CONSTI(was_infnan));
    __m128i sign        = _mm_slli_epi32(justsign, 16);
    __m128  infnanexp   = _mm_and_ps(_mm_castsi128_ps(b_wasinfnan), CONSTF(exp_infnan));
    __m128  sign_inf    = _mm_or_ps(_mm_castsi128_ps(sign), infnanexp);
    __m128  final       = _mm_or_ps(scaled, sign_inf);
    // ~11 SSE2 ops.
    m_simd = final;
# undef CONSTI
# undef CONSTF
#else /* No SIMD defined: */
    SIMD_CONSTRUCT (values[i]);
#endif
}
#endif /* _HALF_H_ */

OIIO_FORCEINLINE void float4::store (float *values) const {
#if OIIO_SIMD_SSE
    // Use an unaligned store -- it's just as fast when the memory turns
    // out to be aligned, nearly as fast even when unaligned. Not worth
    // the headache of using stores that require alignment.
    _mm_storeu_ps (values, m_simd);
#elif defined(OIIO_SIMD_NEON)
    vst1q_f32 (values, m_simd);
#else
    SIMD_DO (values[i] = m_val[i]);
#endif
}

OIIO_FORCEINLINE void float4::store (float *values, int n) const {
    DASSERT (n >= 0 && n <= 4);
#if OIIO_SIMD_SSE
    switch (n) {
        case 1:
        _mm_store_ss (values, m_simd);
        break;
    case 2:
        // Trickery: store two floats as a double worth of bits
        _mm_store_sd ((double*)values, _mm_castps_pd(m_simd));
        break;
    case 3:
        values[0] = m_val[0];
        values[1] = m_val[1];
        values[2] = m_val[2];
        // This looks wasteful, but benchmarks show that it's the
        // fastest way to store 3 values, in benchmarks was faster than
        // this, below:
        //   _mm_store_sd ((double*)values, _mm_castps_pd(m_simd));
        //   _mm_store_ss (values + 2, _mm_movehl_ps(m_simd,m_simd));
        break;
    case 4:
        store (values);
        break;
    default:
        break;
    }
#elif defined(OIIO_SIMD_NEON)
    switch (n) {
    case 1:
        vst1q_lane_f32 (values, m_simd, 0);
        break;
    case 2:
        vst1q_lane_f32 (values++, m_simd, 0);
        vst1q_lane_f32 (values, m_simd, 1);
        break;
    case 3:
        vst1q_lane_f32 (values++, m_simd, 0);
        vst1q_lane_f32 (values++, m_simd, 1);
        vst1q_lane_f32 (values, m_simd, 2);
        break;
    case 4:
        vst1q_f32 (values, m_simd); break;
    default:
        break;
    }
#else
    for (int i = 0; i < n; ++i)
        values[i] = m_val[i];
#endif
}

#ifdef _HALF_H_
OIIO_FORCEINLINE void float4::store (half *values) const {
#if defined(__F16C__) && defined(OIIO_SIMD_SSE)
    __m128i h = _mm_cvtps_ph (m_simd, (_MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC));
    _mm_store_sd ((double *)values, _mm_castsi128_pd(h));
#else
    SIMD_DO (values[i] = m_val[i]);
#endif
}
#endif

OIIO_FORCEINLINE float4 operator+ (const float4& a, const float4& b) {
#if OIIO_SIMD_SSE
    return _mm_add_ps (a.m_simd, b.m_simd);
#elif defined(OIIO_SIMD_NEON)
    return vaddq_f32 (a.m_simd, b.m_simd);
#else
    SIMD_RETURN (float4, a[i] + b[i]);
#endif
}

OIIO_FORCEINLINE const float4 & float4::operator+= (const float4& a) {
#if OIIO_SIMD_SSE
    m_simd = _mm_add_ps (m_simd, a.m_simd);
#elif defined(OIIO_SIMD_NEON)
    m_simd = vaddq_f32 (m_simd, a.m_simd);
#else
    SIMD_DO (m_val[i] += a[i]);
#endif
    return *this;
    }

OIIO_FORCEINLINE float4 float4::operator- () const {
#if OIIO_SIMD_SSE
    return _mm_sub_ps (_mm_setzero_ps(), m_simd);
#elif defined(OIIO_SIMD_NEON)
    return vsubq_f32 (Zero(), m_simd);
#else
    SIMD_RETURN (float4, -m_val[i]);
#endif
}

OIIO_FORCEINLINE float4 operator- (const float4& a, const float4& b) {
#if OIIO_SIMD_SSE
    return _mm_sub_ps (a.m_simd, b.m_simd);
#elif defined(OIIO_SIMD_NEON)
    return vsubq_f32 (a.m_simd, b.m_simd);
#else
    SIMD_RETURN (float4, a[i] - b[i]);
#endif
}

OIIO_FORCEINLINE const float4 & float4::operator-= (const float4& a) {
#if OIIO_SIMD_SSE
    m_simd = _mm_sub_ps (m_simd, a.m_simd);
#elif defined(OIIO_SIMD_NEON)
    m_simd = vsubq_f32 (m_simd, a.m_simd);
#else
    SIMD_DO (m_val[i] -= a[i]);
#endif
    return *this;
}

OIIO_FORCEINLINE float4 operator* (const float4& a, const float4& b) {
#if OIIO_SIMD_SSE
    return _mm_mul_ps (a.m_simd, b.m_simd);
#elif defined(OIIO_SIMD_NEON)
    return vmulq_f32 (a.m_simd, b.m_simd);
#else
    SIMD_RETURN (float4, a[i] * b[i]);
#endif
}

OIIO_FORCEINLINE const float4 & float4::operator*= (const float4& a) {
#if OIIO_SIMD_SSE
    m_simd = _mm_mul_ps (m_simd, a.m_simd);
#elif defined(OIIO_SIMD_NEON)
    m_simd = vmulq_f32 (m_simd, a.m_simd);
#else
    SIMD_DO (m_val[i] *= a[i]);
#endif
    return *this;
}

OIIO_FORCEINLINE const float4 & float4::operator*= (float val) {
#if OIIO_SIMD_SSE
    m_simd = _mm_mul_ps (m_simd, _mm_set1_ps(val));
#elif defined(OIIO_SIMD_NEON)
    m_simd = vmulq_f32 (m_simd, vdupq_n_f32(val));
#else
    SIMD_DO (m_val[i] *= val);
#endif
    return *this;
}

OIIO_FORCEINLINE float4 operator/ (const float4& a, const float4& b) {
#if OIIO_SIMD_SSE
    return _mm_div_ps (a.m_simd, b.m_simd);
#else
    SIMD_RETURN (float4, a[i] / b[i]);
#endif
}

OIIO_FORCEINLINE const float4 & float4::operator/= (const float4& a) {
#if OIIO_SIMD_SSE
    m_simd = _mm_div_ps (m_simd, a.m_simd);
#else
    SIMD_DO (m_val[i] /= a[i]);
#endif
    return *this;
}

OIIO_FORCEINLINE const float4 & float4::operator/= (float val) {
#if OIIO_SIMD_SSE
    m_simd = _mm_div_ps (m_simd, _mm_set1_ps(val));
#else
    SIMD_DO (m_val[i] /= val);
#endif
    return *this;
}

OIIO_FORCEINLINE bool4 operator== (const float4& a, const float4& b) {
#if OIIO_SIMD_SSE
    return _mm_cmpeq_ps (a.m_simd, b.m_simd);
#else
    SIMD_RETURN (bool4, a[i] == b[i] ? -1 : 0);
#endif
}

OIIO_FORCEINLINE bool4 operator!= (const float4& a, const float4& b) {
#if OIIO_SIMD_SSE
    return _mm_cmpneq_ps (a.m_simd, b.m_simd);
#else
    SIMD_RETURN (bool4, a[i] != b[i] ? -1 : 0);
#endif
}

OIIO_FORCEINLINE bool4 operator< (const float4& a, const float4& b) {
#if OIIO_SIMD_SSE
    return _mm_cmplt_ps (a.m_simd, b.m_simd);
#else
    SIMD_RETURN (bool4, a[i] < b[i] ? -1 : 0);
#endif
}

OIIO_FORCEINLINE bool4 operator>  (const float4& a, const float4& b) {
#if OIIO_SIMD_SSE
    return _mm_cmpgt_ps (a.m_simd, b.m_simd);
#else
    SIMD_RETURN (bool4, a[i] > b[i] ? -1 : 0);
#endif
}

OIIO_FORCEINLINE bool4 operator>= (const float4& a, const float4& b) {
#if OIIO_SIMD_SSE
    return _mm_cmpge_ps (a.m_simd, b.m_simd);
#else
    SIMD_RETURN (bool4, a[i] >= b[i] ? -1 : 0);
#endif
}

OIIO_FORCEINLINE bool4 operator<= (const float4& a, const float4& b) {
#if OIIO_SIMD_SSE
    return _mm_cmple_ps (a.m_simd, b.m_simd);
#else
    SIMD_RETURN (bool4, a[i] <= b[i] ? -1 : 0);
#endif
}

OIIO_FORCEINLINE float4 AxyBxy (const float4& a, const float4& b) {
#if OIIO_SIMD_SSE
    return _mm_movelh_ps (a.m_simd, b.m_simd);
#else
    return float4 (a[0], a[1], b[0], b[1]);
#endif
}

OIIO_FORCEINLINE float4 AxBxAyBy (const float4& a, const float4& b) {
#if OIIO_SIMD_SSE
    return _mm_unpacklo_ps (a.m_simd, b.m_simd);
#else
    return float4 (a[0], b[0], a[1], b[1]);
#endif
}

OIIO_FORCEINLINE float4 float4::xyz0 () const {
    return insert<3>(*this, 0.0f);
}

OIIO_FORCEINLINE float4 float4::xyz1 () const {
    return insert<3>(*this, 1.0f);
}

inline std::ostream& operator<< (std::ostream& cout, const float4& val) {
    cout << val[0];
    for (int i = 1; i < val.elements; ++i)
        cout << ' ' << val[i];
    return cout;
}


// Implementation had to be after the definition of float4.
OIIO_FORCEINLINE int4::int4 (const float4& f)
{
#if OIIO_SIMD_SSE
    m_simd = _mm_cvttps_epi32(f.simd());
#else
    SIMD_CONSTRUCT ((int) f[i]);
#endif
}


template<int i0, int i1, int i2, int i3>
OIIO_FORCEINLINE float4 shuffle (const float4& a) {
#if OIIO_SIMD_SSE
    return shuffle_sse<i0,i1,i2,i3> (__m128(a));
#else
    return float4(a[i0], a[i1], a[i2], a[i3]);
#endif
}

template<int i> OIIO_FORCEINLINE float4 shuffle (const float4& a) { return shuffle<i,i,i,i>(a); }

#if defined(OIIO_SIMD_NEON)
template<> OIIO_FORCEINLINE float4 shuffle<0> (const float4& a) {
    float32x2_t t = vget_low_f32(a.m_simd); return vdupq_lane_f32(t,0);
}
template<> OIIO_FORCEINLINE float4 shuffle<1> (const float4& a) {
    float32x2_t t = vget_low_f32(a.m_simd); return vdupq_lane_f32(t,1);
}
template<> OIIO_FORCEINLINE float4 shuffle<2> (const float4& a) {
    float32x2_t t = vget_high_f32(a.m_simd); return vdupq_lane_f32(t,0);
}
template<> OIIO_FORCEINLINE float4 shuffle<3> (const float4& a) {
    float32x2_t t = vget_high_f32(a.m_simd); return vdupq_lane_f32(t,1);
}
#endif



/// Helper: as rapid as possible extraction of one component, when the
/// index is fixed.
template<int i>
OIIO_FORCEINLINE float extract (const float4& a) {
#if OIIO_SIMD_SSE
    return _mm_cvtss_f32(shuffle_sse<i,i,i,i>(a.simd()));
#else
    return a[i];
#endif
}

#if OIIO_SIMD_SSE
template<> OIIO_FORCEINLINE float extract<0> (const float4& a) {
    return _mm_cvtss_f32(a.simd());
}
#endif


/// Helper: substitute val for a[i]
template<int i>
OIIO_FORCEINLINE float4 insert (const float4& a, float val) {
#if OIIO_SIMD_SSE >= 4
    return _mm_insert_ps (a, _mm_set_ss(val), i<<4);
#else
    float4 tmp = a;
    tmp[i] = val;
    return tmp;
#endif
}

#if OIIO_SIMD_SSE
// Slightly faster special cases for SSE
template<> OIIO_FORCEINLINE float4 insert<0> (const float4& a, float val) {
    return _mm_move_ss (a.simd(), _mm_set_ss(val));
}
#endif


OIIO_FORCEINLINE float float4::x () const { return extract<0>(*this); }
OIIO_FORCEINLINE float float4::y () const { return extract<1>(*this); }
OIIO_FORCEINLINE float float4::z () const { return extract<2>(*this); }
OIIO_FORCEINLINE float float4::w () const { return extract<3>(*this); }
OIIO_FORCEINLINE void float4::set_x (float val) { *this = insert<0>(*this, val); }
OIIO_FORCEINLINE void float4::set_y (float val) { *this = insert<1>(*this, val); }
OIIO_FORCEINLINE void float4::set_z (float val) { *this = insert<2>(*this, val); }
OIIO_FORCEINLINE void float4::set_w (float val) { *this = insert<3>(*this, val); }


OIIO_FORCEINLINE int4 bitcast_to_int (const float4& x)
{
#if OIIO_SIMD_SSE
    return _mm_castps_si128 (x.simd());
#else
    return *(int4 *)&x;
#endif
}

OIIO_FORCEINLINE float4 bitcast_to_float (const int4& x)
{
#if OIIO_SIMD_SSE
    return _mm_castsi128_ps (x.simd());
#else
    return *(float4 *)&x;
#endif
}


// Old names:
inline int4 bitcast_to_int4 (const float4& x) { return bitcast_to_int(x); }
inline float4 bitcast_to_float4 (const int4& x) { return bitcast_to_float(x); }



OIIO_FORCEINLINE float4 vreduce_add (const float4& v) {
#if OIIO_SIMD_SSE >= 3
    // People seem to agree that SSE3 does add reduction best with 2
    // horizontal adds.
    // suppose v = (a, b, c, d)
    simd::float4 ab_cd = _mm_hadd_ps (v.simd(), v.simd());
    // ab_cd = (a+b, c+d, a+b, c+d)
    simd::float4 abcd = _mm_hadd_ps (ab_cd.simd(), ab_cd.simd());
    // all abcd elements are a+b+c+d
    return abcd;
#elif defined(OIIO_SIMD_SSE)
    // I think this is the best we can do for SSE2, and I'm still not sure
    // it's faster than the default scalar operation. But anyway...
    // suppose v = (a, b, c, d)
    float4 ab_ab_cd_cd = shuffle<1,0,3,2>(v) + v;
    // now x = (b,a,d,c) + (a,b,c,d) = (a+b,a+b,c+d,c+d)
    float4 cd_cd_ab_ab = shuffle<2,3,0,1>(ab_ab_cd_cd);
    // now y = (c+d,c+d,a+b,a+b)
    float4 abcd = ab_ab_cd_cd + cd_cd_ab_ab;   // a+b+c+d in all components
    return abcd;
#else
    return float4 (v[0] + v[1] + v[2] + v[3]);
#endif
}


OIIO_FORCEINLINE float reduce_add (const float4& v) {
#if OIIO_SIMD_SSE
    return _mm_cvtss_f32(vreduce_add (v));
#else
    return v[0] + v[1] + v[2] + v[3];
#endif
}

OIIO_FORCEINLINE float4 vdot (const float4 &a, const float4 &b) {
#if OIIO_SIMD_SSE >= 4
    return _mm_dp_ps (a.simd(), b.simd(), 0xff);
#elif OIIO_SIMD_NEON
    float32x4_t ab = vmulq_f32(a, b);
    float32x4_t sum1 = vaddq_f32(ab, vrev64q_f32(ab));
    return vaddq_f32(sum1, vcombine_f32(vget_high_f32(sum1), vget_low_f32(sum1)));
#else
    return vreduce_add (a*b);
#endif
}

OIIO_FORCEINLINE float dot (const float4 &a, const float4 &b) {
#if OIIO_SIMD_SSE >= 4
    return _mm_cvtss_f32 (_mm_dp_ps (a.simd(), b.simd(), 0xff));
#else
    return reduce_add (a*b);
#endif
}

OIIO_FORCEINLINE float4 vdot3 (const float4 &a, const float4 &b) {
#if OIIO_SIMD_SSE >= 4
    return _mm_dp_ps (a.simd(), b.simd(), 0x7f);
#else
    return vreduce_add((a*b).xyz0());
#endif
}

OIIO_FORCEINLINE float dot3 (const float4 &a, const float4 &b) {
#if OIIO_SIMD_SSE >= 4
    return _mm_cvtss_f32 (_mm_dp_ps (a.simd(), b.simd(), 0x77));
#else
    return reduce_add ((a*b).xyz0());
#endif
}


OIIO_FORCEINLINE float4 blend (const float4& a, const float4& b, const bool4& mask)
{
#if OIIO_SIMD_SSE >= 4
    // SSE >= 4.1 only
    return _mm_blendv_ps (a.simd(), b.simd(), mask.simd());
#elif defined(OIIO_SIMD_SSE)
    // Trick for SSE < 4.1
    return _mm_or_ps (_mm_and_ps(mask.simd(), b.simd()),
                      _mm_andnot_ps(mask.simd(), a.simd()));
#else
    return float4 (mask[0] ? b[0] : a[0],
                   mask[1] ? b[1] : a[1],
                   mask[2] ? b[2] : a[2],
                   mask[3] ? b[3] : a[3]);
#endif
}


OIIO_FORCEINLINE float4 blend0 (const float4& a, const bool4& mask)
{
#if OIIO_SIMD_SSE
    return _mm_and_ps(mask.simd(), a.simd());
#else
    return float4 (mask[0] ? a[0] : 0.0f,
                   mask[1] ? a[1] : 0.0f,
                   mask[2] ? a[2] : 0.0f,
                   mask[3] ? a[3] : 0.0f);
#endif
}


OIIO_FORCEINLINE float4 blend0not (const float4& a, const bool4& mask)
{
#if OIIO_SIMD_SSE
    return _mm_andnot_ps(mask.simd(), a.simd());
#else
    return float4 (mask[0] ? 0.0f : a[0],
                   mask[1] ? 0.0f : a[1],
                   mask[2] ? 0.0f : a[2],
                   mask[3] ? 0.0f : a[3]);
#endif
}


OIIO_FORCEINLINE float4 safe_div (const float4 &a, const float4 &b) {
#if OIIO_SIMD_SSE
    return blend0not (a/b, b == float4::Zero());
#else
    return float4 (b[0] == 0.0f ? 0.0f : a[0] / b[0],
                   b[1] == 0.0f ? 0.0f : a[1] / b[1],
                   b[2] == 0.0f ? 0.0f : a[2] / b[2],
                   b[3] == 0.0f ? 0.0f : a[3] / b[3]);
#endif
}


OIIO_FORCEINLINE float3 hdiv (const float4 &a)
{
#if OIIO_SIMD_SSE
    return float3(safe_div(a, shuffle<3>(a)).xyz0());
#else
    float d = a[3];
    return d == 0.0f ? float3 (0.0f) : float3 (a[0]/d, a[1]/d, a[2]/d);
#endif
}



OIIO_FORCEINLINE float4 select (const bool4& mask, const float4& a, const float4& b)
{
    return blend (b, a, mask);
}


OIIO_FORCEINLINE float4 abs (const float4& a)
{
#if OIIO_SIMD_SSE
    // Just clear the sign bit for cheap fabsf
    return _mm_and_ps (a.simd(), _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff)));
#else
    SIMD_RETURN (float4, fabsf(a[i]));
#endif
}


OIIO_FORCEINLINE float4 sign (const float4& a)
{
    float4 one(1.0f);
    return blend (one, -one, a < float4::Zero());
}


OIIO_FORCEINLINE float4 ceil (const float4& a)
{
#if defined(OIIO_SIMD_SSE) && OIIO_SIMD_SSE >= 4  /* SSE >= 4.1 */
    return _mm_ceil_ps (a);
#else
    SIMD_RETURN (float4, ceilf(a[i]));
#endif
}

OIIO_FORCEINLINE float4 floor (const float4& a)
{
#if defined(OIIO_SIMD_SSE) && OIIO_SIMD_SSE >= 4  /* SSE >= 4.1 */
    return _mm_floor_ps (a);
#else
    SIMD_RETURN (float4, floorf(a[i]));
#endif
}

OIIO_FORCEINLINE float4 round (const float4& a)
{
#if defined(OIIO_SIMD_SSE) && OIIO_SIMD_SSE >= 4  /* SSE >= 4.1 */
    return _mm_round_ps (a, (_MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC));
#else
    SIMD_RETURN (float4, roundf(a[i]));
#endif
}

OIIO_FORCEINLINE int4 floori (const float4& a)
{
#if defined(OIIO_SIMD_SSE) && OIIO_SIMD_SSE >= 4  /* SSE >= 4.1 */
    return int4(floor(a));
#elif defined(OIIO_SIMD_SSE)   /* SSE2/3 */
    int4 i (a);  // truncates
    int4 isneg = bitcast_to_int (a < float4::Zero());
    return i + isneg;
    // The trick here (thanks, Cycles, for letting me spy on your code) is
    // that the comparison will return (int)-1 for components that are less
    // than zero, and adding that is the same as subtracting one!
#else
    SIMD_RETURN (int4, (int)floorf(a[i]));
#endif
}


OIIO_FORCEINLINE int4 rint (const float4& a)
{
    return int4 (round(a));
}


OIIO_FORCEINLINE float4 sqrt (const float4 &a)
{
#if OIIO_SIMD_SSE
    return _mm_sqrt_ps (a.simd());
#else
    SIMD_RETURN (float4, sqrtf(a[i]));
#endif
}


OIIO_FORCEINLINE float4 rsqrt (const float4 &a)
{
#if OIIO_SIMD_SSE
    return _mm_div_ps (_mm_set1_ps(1.0f), _mm_sqrt_ps (a.simd()));
#else
    SIMD_RETURN (float4, 1.0f/sqrtf(a[i]));
#endif
}


OIIO_FORCEINLINE float4 rsqrt_fast (const float4 &a)
{
#if OIIO_SIMD_SSE
    return _mm_rsqrt_ps (a.simd());
#else
    SIMD_RETURN (float4, 1.0f/sqrtf(a[i]));
#endif
}


OIIO_FORCEINLINE float4 min (const float4& a, const float4& b)
{
#if OIIO_SIMD_SSE
    return _mm_min_ps (a, b);
#else
    SIMD_RETURN (float4, std::min (a[i], b[i]));
#endif
}

OIIO_FORCEINLINE float4 max (const float4& a, const float4& b)
{
#if OIIO_SIMD_SSE
    return _mm_max_ps (a, b);
#else
    SIMD_RETURN (float4, std::max (a[i], b[i]));
#endif
}


OIIO_FORCEINLINE float4 andnot (const float4& a, const float4& b) {
#if OIIO_SIMD_SSE
    return _mm_andnot_ps (a.simd(), b.simd());
#else
    const int *ai = (const int *)&a;
    const int *bi = (const int *)&b;
    return bitcast_to_float (int4(~(ai[0]) & bi[0],
                                  ~(ai[1]) & bi[1],
                                  ~(ai[2]) & bi[2],
                                  ~(ai[3]) & bi[3]));
#endif
}


OIIO_FORCEINLINE float4 madd (const simd::float4& a, const simd::float4& b,
                              const simd::float4& c)
{
#if OIIO_SIMD_SSE && OIIO_FMA_ENABLED
    // If we are sure _mm_fmadd_ps intrinsic is available, use it.
    return _mm_fmadd_ps (a, b, c);
#elif OIIO_SIMD_SSE && !defined(_MSC_VER)
    // If we directly access the underlying __m128, on some platforms and
    // compiler flags, it will turn into fma anyway, even if we don't use
    // the intrinsic.
    return a.simd() * b.simd() + c.simd();
#else
    // Fallback: just use regular math and hope for the best.
    return a * b + c;
#endif
}


OIIO_FORCEINLINE float4 msub (const simd::float4& a, const simd::float4& b,
                              const simd::float4& c)
{
#if OIIO_SIMD_SSE && OIIO_FMA_ENABLED
    // If we are sure _mm_fnmsub_ps intrinsic is available, use it.
    return _mm_fmsub_ps (a, b, c);
#elif OIIO_SIMD_SSE && !defined(_MSC_VER)
    // If we directly access the underlying __m128, on some platforms and
    // compiler flags, it will turn into fma anyway, even if we don't use
    // the intrinsic.
    return a.simd() * b.simd() - c.simd();
#else
    // Fallback: just use regular math and hope for the best.
    return a * b - c;
#endif
}



OIIO_FORCEINLINE float4 nmadd (const simd::float4& a, const simd::float4& b,
                               const simd::float4& c)
{
#if OIIO_SIMD_SSE && OIIO_FMA_ENABLED
    // If we are sure _mm_fnmadd_ps intrinsic is available, use it.
    return _mm_fnmadd_ps (a, b, c);
#elif OIIO_SIMD_SSE && !defined(_MSC_VER)
    // If we directly access the underlying __m128, on some platforms and
    // compiler flags, it will turn into fma anyway, even if we don't use
    // the intrinsic.
    return c.simd() - a.simd() * b.simd();
#else
    // Fallback: just use regular math and hope for the best.
    return c - a * b;
#endif
}



OIIO_FORCEINLINE float4 nmsub (const simd::float4& a, const simd::float4& b,
                               const simd::float4& c)
{
#if OIIO_SIMD_SSE && OIIO_FMA_ENABLED
    // If we are sure _mm_fnmsub_ps intrinsic is available, use it.
    return _mm_fnmsub_ps (a, b, c);
#elif OIIO_SIMD_SSE && !defined(_MSC_VER)
    // If we directly access the underlying __m128, on some platforms and
    // compiler flags, it will turn into fma anyway, even if we don't use
    // the intrinsic.
    return -(a.simd() * b.simd()) - c.simd();
#else
    // Fallback: just use regular math and hope for the best.
    return -(a * b) - c;
#endif
}



// Full precision exp() of all components of a SIMD vector.
template<typename T>
OIIO_FORCEINLINE T exp (const T& v)
{
#if OIIO_SIMD_SSE
    // Implementation inspired by:
    // https://github.com/embree/embree/blob/master/common/simd/sse_special.h
    // Which is listed as Copyright (C) 2007  Julien Pommier and distributed
    // under the zlib license.
    typedef typename T::int_t int_t;
    T x = v;
    const float exp_hi (88.3762626647949f);
    const float exp_lo (-88.3762626647949f);
    const float cephes_LOG2EF (1.44269504088896341f);
    const float cephes_exp_C1 (0.693359375f);
    const float cephes_exp_C2 (-2.12194440e-4f);
    const float cephes_exp_p0 (1.9875691500E-4f);
    const float cephes_exp_p1 (1.3981999507E-3f);
    const float cephes_exp_p2 (8.3334519073E-3f);
    const float cephes_exp_p3 (4.1665795894E-2f);
    const float cephes_exp_p4 (1.6666665459E-1f);
    const float cephes_exp_p5 (5.0000001201E-1f);
    T tmp (0.0f);
    T one (1.0f);
    x = min (x, T(exp_hi));
    x = max (x, T(exp_lo));
    T fx = madd (x, T(cephes_LOG2EF), T(0.5f));
    int_t emm0 = int_t(fx);
    tmp = T(emm0);
    T mask = bitcast_to_float (bitcast_to_int(tmp > fx) & bitcast_to_int(one));
    fx = tmp - mask;
    tmp = fx * cephes_exp_C1;
    T z = fx * cephes_exp_C2;
    x = x - tmp;
    x = x - z;
    z = x * x;
    T y = cephes_exp_p0;
    y = madd (y, x, cephes_exp_p1);
    y = madd (y, x, cephes_exp_p2);
    y = madd (y, x, cephes_exp_p3);
    y = madd (y, x, cephes_exp_p4);
    y = madd (y, x, cephes_exp_p5);
    y = madd (y, z, x);
    y = y + one;
    emm0 = (int_t(fx) + int_t(0x7f)) << 23;
    T pow2n = bitcast_to_float(emm0);
    y = y * pow2n;
    return y;
#else
    SIMD_RETURN (T, expf(v[i]));
#endif
}



// Full precision log() of all components of a SIMD vector.
template<typename T>
OIIO_FORCEINLINE T log (const T& v)
{
#if OIIO_SIMD_SSE
    // Implementation inspired by:
    // https://github.com/embree/embree/blob/master/common/simd/sse_special.h
    // Which is listed as Copyright (C) 2007  Julien Pommier and distributed
    // under the zlib license.
    typedef typename T::int_t int_t;
    typedef typename T::bool_t bool_t;
    T x = v;
    int_t emm0;
    T zero (T::Zero());
    T one (1.0f);
    bool_t invalid_mask = (x <= zero);
    const int min_norm_pos ((int)0x00800000);
    const int inv_mant_mask ((int)~0x7f800000);
    x = max(x, bitcast_to_float(int_t(min_norm_pos)));  /* cut off denormalized stuff */
    emm0 = srl (bitcast_to_int(x), 23);
    /* keep only the fractional part */
    x = bitcast_to_float (bitcast_to_int(x) & int_t(inv_mant_mask));
    x = bitcast_to_float (bitcast_to_int(x) | bitcast_to_int(T(0.5f)));
    emm0 = emm0 - int_t(0x7f);
    T e (emm0);
    e = e + one;
    // OIIO_SIMD_FLOAT4_CONST (cephes_SQRTHF, 0.707106781186547524f);
    const float cephes_SQRTHF (0.707106781186547524f);
    bool_t mask = (x < T(cephes_SQRTHF));
    T tmp = bitcast_to_float (bitcast_to_int(x) & bitcast_to_int(mask));
    x = x - one;
    e = e - bitcast_to_float (bitcast_to_int(one) & bitcast_to_int(mask));
    x = x + tmp;
    T z = x * x;
    const float cephes_log_p0 (7.0376836292E-2f);
    const float cephes_log_p1 (- 1.1514610310E-1f);
    const float cephes_log_p2 (1.1676998740E-1f);
    const float cephes_log_p3 (- 1.2420140846E-1f);
    const float cephes_log_p4 (+ 1.4249322787E-1f);
    const float cephes_log_p5 (- 1.6668057665E-1f);
    const float cephes_log_p6 (+ 2.0000714765E-1f);
    const float cephes_log_p7 (- 2.4999993993E-1f);
    const float cephes_log_p8 (+ 3.3333331174E-1f);
    const float cephes_log_q1 (-2.12194440e-4f);
    const float cephes_log_q2 (0.693359375f);
    T y = cephes_log_p0;
    y = madd (y, x, T(cephes_log_p1));
    y = madd (y, x, T(cephes_log_p2));
    y = madd (y, x, T(cephes_log_p3));
    y = madd (y, x, T(cephes_log_p4));
    y = madd (y, x, T(cephes_log_p5));
    y = madd (y, x, T(cephes_log_p6));
    y = madd (y, x, T(cephes_log_p7));
    y = madd (y, x, T(cephes_log_p8));
    y = y * x;
    y = y * z;
    y = madd(e, T(cephes_log_q1), y);
    y = nmadd (z, 0.5f, y);
    x = x + y;
    x = madd (e, T(cephes_log_q2), x);
    x = bitcast_to_float (bitcast_to_int(x) | bitcast_to_int(invalid_mask)); // negative arg will be NAN
    return x;
#else
    SIMD_RETURN (T, logf(v[i]));
#endif
}



OIIO_FORCEINLINE void transpose (float4 &a, float4 &b, float4 &c, float4 &d)
{
#if OIIO_SIMD_SSE
    _MM_TRANSPOSE4_PS (a, b, c, d);
#else
    float4 A (a[0], b[0], c[0], d[0]);
    float4 B (a[1], b[1], c[1], d[1]);
    float4 C (a[2], b[2], c[2], d[2]);
    float4 D (a[3], b[3], c[3], d[3]);
    a = A;  b = B;  c = C;  d = D;
#endif
}


OIIO_FORCEINLINE void transpose (const float4& a, const float4& b, const float4& c, const float4& d,
                                 float4 &r0, float4 &r1, float4 &r2, float4 &r3)
{
#if OIIO_SIMD_SSE
    //_MM_TRANSPOSE4_PS (a, b, c, d);
    float4 l02 = _mm_unpacklo_ps (a, c);
    float4 h02 = _mm_unpackhi_ps (a, c);
    float4 l13 = _mm_unpacklo_ps (b, d);
    float4 h13 = _mm_unpackhi_ps (b, d);
    r0 = _mm_unpacklo_ps (l02, l13);
    r1 = _mm_unpackhi_ps (l02, l13);
    r2 = _mm_unpacklo_ps (h02, h13);
    r3 = _mm_unpackhi_ps (h02, h13);
#else
    r0.load (a[0], b[0], c[0], d[0]);
    r1.load (a[1], b[1], c[1], d[1]);
    r2.load (a[2], b[2], c[2], d[2]);
    r3.load (a[3], b[3], c[3], d[3]);
#endif
}


OIIO_FORCEINLINE void transpose (int4 &a, int4 &b, int4 &c, int4 &d)
{
#if OIIO_SIMD_SSE
    __m128 A = _mm_castsi128_ps (a);
    __m128 B = _mm_castsi128_ps (b);
    __m128 C = _mm_castsi128_ps (c);
    __m128 D = _mm_castsi128_ps (d);
    _MM_TRANSPOSE4_PS (A, B, C, D);
    a = _mm_castps_si128 (A);
    b = _mm_castps_si128 (B);
    c = _mm_castps_si128 (C);
    d = _mm_castps_si128 (D);
#else
    int4 A (a[0], b[0], c[0], d[0]);
    int4 B (a[1], b[1], c[1], d[1]);
    int4 C (a[2], b[2], c[2], d[2]);
    int4 D (a[3], b[3], c[3], d[3]);
    a = A;  b = B;  c = C;  d = D;
#endif
}

OIIO_FORCEINLINE void transpose (const int4& a, const int4& b, const int4& c, const int4& d,
                                 int4 &r0, int4 &r1, int4 &r2, int4 &r3)
{
#if OIIO_SIMD_SSE
    //_MM_TRANSPOSE4_PS (a, b, c, d);
    __m128 A = _mm_castsi128_ps (a);
    __m128 B = _mm_castsi128_ps (b);
    __m128 C = _mm_castsi128_ps (c);
    __m128 D = _mm_castsi128_ps (d);
    _MM_TRANSPOSE4_PS (A, B, C, D);
    r0 = _mm_castps_si128 (A);
    r1 = _mm_castps_si128 (B);
    r2 = _mm_castps_si128 (C);
    r3 = _mm_castps_si128 (D);
#else
    r0.load (a[0], b[0], c[0], d[0]);
    r1.load (a[1], b[1], c[1], d[1]);
    r2.load (a[2], b[2], c[2], d[2]);
    r3.load (a[3], b[3], c[3], d[3]);
#endif
}


OIIO_FORCEINLINE float4 AxBxCxDx (const float4& a, const float4& b,
                                  const float4& c, const float4& d)
{
#if OIIO_SIMD_SSE
    float4 l02 = _mm_unpacklo_ps (a, c);
    float4 l13 = _mm_unpacklo_ps (b, d);
    return _mm_unpacklo_ps (l02, l13);
#else
    return float4 (a[0], b[0], c[0], d[0]);
#endif
}


OIIO_FORCEINLINE int4 AxBxCxDx (const int4& a, const int4& b,
                                const int4& c, const int4& d)
{
#if OIIO_SIMD_SSE
    int4 l02 = _mm_unpacklo_epi32 (a, c);
    int4 l13 = _mm_unpacklo_epi32 (b, d);
    return _mm_unpacklo_epi32 (l02, l13);
#else
    return int4 (a[0], b[0], c[0], d[0]);
#endif
}



//////////////////////////////////////////////////////////////////////
// float3 implementation

OIIO_FORCEINLINE float3::float3 (const float3 &other) {
#if defined(OIIO_SIMD_SSE) || defined(OIIO_SIMD_NEON)
    m_simd = other.m_simd;
#else
    SIMD_CONSTRUCT_PAD (other[i]);
#endif
}

OIIO_FORCEINLINE float3::float3 (const float4 &other) {
#if defined(OIIO_SIMD_SSE) || defined(OIIO_SIMD_NEON)
    m_simd = other.simd();
#else
    SIMD_CONSTRUCT_PAD (other[i]);
    m_val[3] = 0.0f;
#endif
}

OIIO_FORCEINLINE const float3 float3::Zero () { return float3(float4::Zero()); }

OIIO_FORCEINLINE const float3 float3::One () { return float3(1.0f); }

OIIO_FORCEINLINE const float3 float3::Iota (float start, float step) {
    return float3 (start+0.0f*step, start+1.0f*step, start+2.0f*step);
}


OIIO_FORCEINLINE void float3::load (float val) { float4::load (val, val, val, 0.0f); }

OIIO_FORCEINLINE void float3::load (const float *values) { float4::load (values, 3); }

OIIO_FORCEINLINE void float3::load (const float *values, int n) {
    float4::load (values, n);
}

OIIO_FORCEINLINE void float3::load (const unsigned short *values) {
    float4::load (float(values[0]), float(values[1]), float(values[2]));
}

OIIO_FORCEINLINE void float3::load (const short *values) {
    float4::load (float(values[0]), float(values[1]), float(values[2]));
}

OIIO_FORCEINLINE void float3::load (const unsigned char *values) {
    float4::load (float(values[0]), float(values[1]), float(values[2]));
}

OIIO_FORCEINLINE void float3::load (const char *values) {
    float4::load (float(values[0]), float(values[1]), float(values[2]));
}

#ifdef _HALF_H_
OIIO_FORCEINLINE void float3::load (const half *values) {
    float4::load (float(values[0]), float(values[1]), float(values[2]));
}
#endif /* _HALF_H_ */

OIIO_FORCEINLINE void float3::store (float *values) const {
    float4::store (values, 3);
}

OIIO_FORCEINLINE void float3::store (float *values, int n) const {
    float4::store (values, n);
}

#ifdef _HALF_H_
OIIO_FORCEINLINE void float3::store (half *values) const {
    SIMD_DO (values[i] = m_val[i]);
}
#endif

OIIO_FORCEINLINE void float3::store (Imath::V3f &vec) const {
    store ((float *)&vec);
}

OIIO_FORCEINLINE float3 operator+ (const float3& a, const float3& b) {
    return float3 (float4(a) + float4(b));
}

OIIO_FORCEINLINE const float3 & float3::operator+= (const float3& a) {
    *this = *this + a; return *this;
}

OIIO_FORCEINLINE float3 float3::operator- () const {
    return float3 (-float4(*this));
}

OIIO_FORCEINLINE float3 operator- (const float3& a, const float3& b) {
    return float3 (float4(a) - float4(b));
}

OIIO_FORCEINLINE const float3 & float3::operator-= (const float3& a) {
    *this = *this - a; return *this;
}

OIIO_FORCEINLINE float3 operator* (const float3& a, const float3& b) {
    return float3 (float4(a) * float4(b));
}

OIIO_FORCEINLINE const float3 & float3::operator*= (const float3& a) {
    *this = *this * a; return *this;
}

OIIO_FORCEINLINE const float3 & float3::operator*= (float a) {
    *this = *this * a; return *this;
}

OIIO_FORCEINLINE float3 operator/ (const float3& a, const float3& b) {
    return float3 (float4(a) / b.xyz1()); // Avoid divide by zero!
}

OIIO_FORCEINLINE const float3 & float3::operator/= (const float3& a) {
    *this = *this / a; return *this;
}

OIIO_FORCEINLINE const float3 & float3::operator/= (float a) {
    *this = *this / a; return *this;
}


inline std::ostream& operator<< (std::ostream& cout, const float3& val) {
    cout << val[0];
    for (int i = 1; i < val.elements; ++i)
        cout << ' ' << val[i];
    return cout;
}


OIIO_FORCEINLINE float3 vreduce_add (const float3& v) {
#if OIIO_SIMD_SSE
    return float3 ((vreduce_add(float4(v))).xyz0());
#else
    return float3 (v[0] + v[1] + v[2]);
#endif
}


OIIO_FORCEINLINE float3 vdot (const float3 &a, const float3 &b) {
#if OIIO_SIMD_SSE >= 4
    return float3(_mm_dp_ps (a.simd(), b.simd(), 0x77));
#else
    return vreduce_add (a*b);
#endif
}


OIIO_FORCEINLINE float dot (const float3 &a, const float3 &b) {
#if OIIO_SIMD_SSE >= 4
    return _mm_cvtss_f32 (_mm_dp_ps (a.simd(), b.simd(), 0x77));
#else
    return reduce_add (a*b);
#endif
}


OIIO_FORCEINLINE float3 vdot3 (const float3 &a, const float3 &b) {
#if OIIO_SIMD_SSE >= 4
    return float3(_mm_dp_ps (a.simd(), b.simd(), 0x77));
#else
    return float3 (vreduce_add((a*b).xyz0()).xyz0());
#endif
}

OIIO_FORCEINLINE float3 float3::normalized () const
{
#if OIIO_SIMD
    float3 len2 = vdot3 (*this, *this);
    return float3 (safe_div (*this, sqrt(len2)));
#else
    float len2 = dot (*this, *this);
    return len2 > 0.0f ? (*this) / sqrtf(len2) : float3::Zero();
#endif
}


OIIO_FORCEINLINE float3 float3::normalized_fast () const
{
#if OIIO_SIMD
    float3 len2 = vdot3 (*this, *this);
    float4 invlen = blend0not (rsqrt_fast (len2), len2 == float4::Zero());
    return float3 ((*this) * invlen);
#else
    float len2 = dot (*this, *this);
    return len2 > 0.0f ? (*this) / sqrtf(len2) : float3::Zero();
#endif
}



//////////////////////////////////////////////////////////////////////
// matrix44 implementation


OIIO_FORCEINLINE const Imath::M44f& matrix44::M44f() const {
    return *(Imath::M44f*)this;
}


OIIO_FORCEINLINE float4 matrix44::operator[] (int i) const {
#if OIIO_SIMD_SSE
    return m_row[i];
#else
    return float4 (m_mat[i]);
#endif
}


OIIO_FORCEINLINE matrix44 matrix44::transposed () const {
    matrix44 T;
#if OIIO_SIMD_SSE
    simd::transpose (m_row[0], m_row[1], m_row[2], m_row[3],
                     T.m_row[0], T.m_row[1], T.m_row[2], T.m_row[3]);
#else
    T = m_mat.transposed();
#endif
    return T;
}

OIIO_FORCEINLINE float3 matrix44::transformp (const float3 &V) const {
#if OIIO_SIMD_SSE
    float4 R = shuffle<0>(V) * m_row[0] + shuffle<1>(V) * m_row[1] +
               shuffle<2>(V) * m_row[2] + m_row[3];
    R = R / shuffle<3>(R);
    return float3 (R.xyz0());
#else
    Imath::V3f R;
    m_mat.multVecMatrix (*(Imath::V3f *)&V, R);
    return float3(R);
#endif
}

OIIO_FORCEINLINE float3 matrix44::transformv (const float3 &V) const {
#if OIIO_SIMD_SSE
    float4 R = shuffle<0>(V) * m_row[0] + shuffle<1>(V) * m_row[1] +
               shuffle<2>(V) * m_row[2];
    return float3 (R.xyz0());
#else
    Imath::V3f R;
    m_mat.multDirMatrix (*(Imath::V3f *)&V, R);
    return float3(R);
#endif
}

OIIO_FORCEINLINE float3 matrix44::transformvT (const float3 &V) const {
#if OIIO_SIMD_SSE
    matrix44 T = transposed();
    float4 R = shuffle<0>(V) * T[0] + shuffle<1>(V) * T[1] +
               shuffle<2>(V) * T[2];
    return float3 (R.xyz0());
#else
    Imath::V3f R;
    m_mat.transposed().multDirMatrix (*(Imath::V3f *)&V, R);
    return float3(R);
#endif
}

OIIO_FORCEINLINE bool matrix44::operator== (const matrix44& m) const {
#if OIIO_SIMD_SSE
    bool4 b0 = (m_row[0] == m[0]);
    bool4 b1 = (m_row[1] == m[1]);
    bool4 b2 = (m_row[2] == m[2]);
    bool4 b3 = (m_row[3] == m[3]);
    return simd::all (b0 & b1 & b2 & b3);
#else
    return memcmp(this, &m, 16*sizeof(float)) == 0;
#endif
}

OIIO_FORCEINLINE bool matrix44::operator== (const Imath::M44f& m) const {
    return memcmp(this, &m, 16*sizeof(float)) == 0;
}

OIIO_FORCEINLINE bool operator== (const Imath::M44f& a, const matrix44 &b) {
    return (b == a);
}

OIIO_FORCEINLINE bool matrix44::operator!= (const matrix44& m) const {
#if OIIO_SIMD_SSE
    bool4 b0 = (m_row[0] != m[0]);
    bool4 b1 = (m_row[1] != m[1]);
    bool4 b2 = (m_row[2] != m[2]);
    bool4 b3 = (m_row[3] != m[3]);
    return simd::any (b0 | b1 | b2 | b3);
#else
    return memcmp(this, &m, 16*sizeof(float)) != 0;
#endif
}

OIIO_FORCEINLINE bool matrix44::operator!= (const Imath::M44f& m) const {
    return memcmp(this, &m, 16*sizeof(float)) != 0;
}

OIIO_FORCEINLINE bool operator!= (const Imath::M44f& a, const matrix44 &b) {
    return (b != a);
}

OIIO_FORCEINLINE matrix44 matrix44::inverse() const {
#if OIIO_SIMD_SSE
    // Adapted from this code from Intel:
    // ftp://download.intel.com/design/pentiumiii/sml/24504301.pdf
    float4 minor0, minor1, minor2, minor3;
    float4 row0, row1, row2, row3;
    float4 det, tmp1;
    const float *src = (const float *)this;
    float4 zero = float4::Zero();
    tmp1 = _mm_loadh_pi(_mm_loadl_pi(zero, (__m64*)(src)), (__m64*)(src+ 4));
    row1 = _mm_loadh_pi(_mm_loadl_pi(zero, (__m64*)(src+8)), (__m64*)(src+12));
    row0 = _mm_shuffle_ps(tmp1, row1, 0x88);
    row1 = _mm_shuffle_ps(row1, tmp1, 0xDD);
    tmp1 = _mm_loadh_pi(_mm_loadl_pi(tmp1, (__m64*)(src+ 2)), (__m64*)(src+ 6));
    row3 = _mm_loadh_pi(_mm_loadl_pi(zero, (__m64*)(src+10)), (__m64*)(src+14));
    row2 = _mm_shuffle_ps(tmp1, row3, 0x88);
    row3 = _mm_shuffle_ps(row3, tmp1, 0xDD);
    // -----------------------------------------------
    tmp1 = row2 * row3;
    tmp1 = shuffle<1,0,3,2>(tmp1);
    minor0 = row1 * tmp1;
    minor1 = row0 * tmp1;
    tmp1 = shuffle<2,3,0,1>(tmp1);
    minor0 = (row1 * tmp1) - minor0;
    minor1 = (row0 * tmp1) - minor1;
    minor1 = shuffle<2,3,0,1>(minor1);
    // -----------------------------------------------
    tmp1 = row1 * row2;
    tmp1 = shuffle<1,0,3,2>(tmp1);
    minor0 = (row3 * tmp1) + minor0;
    minor3 = row0 * tmp1;
    tmp1 = shuffle<2,3,0,1>(tmp1);
    minor0 = minor0 - (row3 * tmp1);
    minor3 = (row0 * tmp1) - minor3;
    minor3 = shuffle<2,3,0,1>(minor3);
    // -----------------------------------------------
    tmp1 = shuffle<2,3,0,1>(row1) * row3;
    tmp1 = shuffle<1,0,3,2>(tmp1);
    row2 = shuffle<2,3,0,1>(row2);
    minor0 = (row2 * tmp1) + minor0;
    minor2 = row0 * tmp1;
    tmp1 = shuffle<2,3,0,1>(tmp1);
    minor0 = minor0 - (row2 * tmp1);
    minor2 = (row0 * tmp1) - minor2;
    minor2 = shuffle<2,3,0,1>(minor2);
    // -----------------------------------------------
    tmp1 = row0 * row1;
    tmp1 = shuffle<1,0,3,2>(tmp1);
    minor2 = (row3 * tmp1) + minor2;
    minor3 = (row2 * tmp1) - minor3;
    tmp1 = shuffle<2,3,0,1>(tmp1);
    minor2 = (row3 * tmp1) - minor2;
    minor3 = minor3 - (row2 * tmp1);
    // -----------------------------------------------
    tmp1 = row0 * row3;
    tmp1 = shuffle<1,0,3,2>(tmp1);
    minor1 = minor1 - (row2 * tmp1);
    minor2 = (row1 * tmp1) + minor2;
    tmp1 = shuffle<2,3,0,1>(tmp1);
    minor1 = (row2 * tmp1) + minor1;
    minor2 = minor2 - (row1 * tmp1);
    // -----------------------------------------------
    tmp1 = row0 * row2;
    tmp1 = shuffle<1,0,3,2>(tmp1);
    minor1 = (row3 * tmp1) + minor1;
    minor3 = minor3 - (row1 * tmp1);
    tmp1 = shuffle<2,3,0,1>(tmp1);
    minor1 = minor1 - (row3 * tmp1);
    minor3 = (row1 * tmp1) + minor3;
    // -----------------------------------------------
    det = row0 * minor0;
    det = shuffle<2,3,0,1>(det) + det;
    det = _mm_add_ss(shuffle<1,0,3,2>(det), det);
    tmp1 = _mm_rcp_ss(det);
    det = _mm_sub_ss(_mm_add_ss(tmp1, tmp1), _mm_mul_ss(det, _mm_mul_ss(tmp1, tmp1)));
    det = shuffle<0>(det);
    return matrix44 (det*minor0, det*minor1, det*minor2, det*minor3);
#else
    return m_mat.inverse();
#endif
}


inline std::ostream& operator<< (std::ostream& cout, const matrix44 &M) {
    const float *m = (const float *)&M;
    cout << m[0];
    for (int i = 1; i < 16; ++i)
        cout << ' ' << m[i];
    return cout;
}



OIIO_FORCEINLINE float3 transformp (const matrix44 &M, const float3 &V) {
    return M.transformp (V);
}

OIIO_FORCEINLINE float3 transformp (const Imath::M44f &M, const float3 &V)
{
#if OIIO_SIMD
    return matrix44(M).transformp (V);
#else
    Imath::V3f R;
    M.multVecMatrix (*(Imath::V3f *)&V, R);
    return float3(R);
#endif
}


OIIO_FORCEINLINE float3 transformv (const matrix44 &M, const float3 &V) {
    return M.transformv (V);
}

OIIO_FORCEINLINE float3 transformv (const Imath::M44f &M, const float3 &V)
{
#if OIIO_SIMD
    return matrix44(M).transformv (V);
#else
    Imath::V3f R;
    M.multDirMatrix (*(Imath::V3f *)&V, R);
    return float3(R);
#endif
}

OIIO_FORCEINLINE float3 transformvT (const matrix44 &M, const float3 &V)
{
    return M.transformvT (V);
}

OIIO_FORCEINLINE float3 transformvT (const Imath::M44f &M, const float3 &V)
{
#if OIIO_SIMD
    return matrix44(M).transformvT(V);
#else
    return transformv (M.transposed(), V);
#endif
}



//////////////////////////////////////////////////////////////////////
// float8 implementation

OIIO_FORCEINLINE float& float8::operator[] (int i) {
    DASSERT(i<elements);
    return m_val[i];
}

OIIO_FORCEINLINE float float8::operator[] (int i) const {
    DASSERT(i<elements);
    return m_val[i];
}


inline std::ostream& operator<< (std::ostream& cout, const float8& val) {
    cout << val[0];
    for (int i = 1; i < val.elements; ++i)
        cout << ' ' << val[i];
    return cout;
}


OIIO_FORCEINLINE float4 float8::lo () const {
#if OIIO_SIMD_AVX
    return _mm256_castps256_ps128 (simd());
#else
    return m_4[0];
#endif
}

OIIO_FORCEINLINE float4 float8::hi () const {
#if OIIO_SIMD_AVX
    return _mm256_extractf128_ps (simd(), 1);
#else
    return m_4[1];
#endif
}


OIIO_FORCEINLINE float8::float8 (const float4& lo, const float4 &hi) {
#if OIIO_SIMD_AVX
    __m256 r = _mm256_castps128_ps256 (lo);
    m_simd = _mm256_insertf128_ps (r, hi, 1);
    // N.B. equivalent, if available: m_simd = _mm256_set_m128 (hi, lo);
#else
    m_4[0] = lo;
    m_4[1] = hi;
#endif
}


OIIO_FORCEINLINE float8::float8 (const int8& ival) {
#if OIIO_SIMD_AVX
    m_simd = _mm256_cvtepi32_ps (ival);
#else
    SIMD_CONSTRUCT (float(ival[i]));
#endif
}


OIIO_FORCEINLINE const float8 float8::Zero () {
#if OIIO_SIMD_AVX
    return _mm256_setzero_ps();
#else
    return float8(0.0f);
#endif
}

OIIO_FORCEINLINE const float8 float8::One () {
    return float8(1.0f);
}

OIIO_FORCEINLINE const float8 float8::Iota (float start, float step) {
    return float8 (start+0.0f*step, start+1.0f*step, start+2.0f*step, start+3.0f*step,
                   start+4.0f*step, start+5.0f*step, start+6.0f*step, start+7.0f*step);
}

/// Set all components to 0.0
OIIO_FORCEINLINE void float8::clear () {
#if OIIO_SIMD_AVX
    m_simd = _mm256_setzero_ps();
#else
    load (0.0f);
#endif
}



OIIO_FORCEINLINE void float8::load (float val) {
#if OIIO_SIMD_AVX
    m_simd = _mm256_set1_ps (val);
#else
    SIMD_CONSTRUCT (val);
#endif
}

OIIO_FORCEINLINE void float8::load (float a, float b, float c, float d,
                                    float e, float f, float g, float h) {
#if OIIO_SIMD_AVX
    m_simd = _mm256_set_ps (h, g, f, e, d, c, b, a);
#else
    m_val[0] = a;
    m_val[1] = b;
    m_val[2] = c;
    m_val[3] = d;
    m_val[4] = e;
    m_val[5] = f;
    m_val[6] = g;
    m_val[7] = h;
#endif
}


OIIO_FORCEINLINE void float8::load (const float *values) {
#if OIIO_SIMD_AVX
    m_simd = _mm256_loadu_ps (values);
#elif OIIO_SIMD_SSE
    m_4[0] = float4(values);
    m_4[1] = float4(values+4);
#else
    SIMD_CONSTRUCT (values[i]);
#endif
}


OIIO_FORCEINLINE void float8::load (const float *values, int n) {
    // FIXME: is this faster with AVX masked loads?
#if OIIO_SIMD_SSE
    if (n > 0 && n <= 4) {
        float4 l; l.load (values, n);
        m_simd = float8(l, float4::Zero());
    } else if (n > 4 && n <= 8) {
        float4 h; h.load (values+4, n-4);
        m_simd = float8(float4(values), h);
    }
    else
        clear();
#else
    for (int i = 0; i < n; ++i)
        m_val[i] = values[i];
    for (int i = n; i < paddedelements; ++i)
        m_val[i] = 0;
#endif
}


OIIO_FORCEINLINE void float8::load (const unsigned short *values) {
#if OIIO_SIMD_AVX
    // Rely on the uint16->int conversion, then convert to float
    m_simd = _mm256_cvtepi32_ps (int8(values).simd());
#else
    SIMD_CONSTRUCT (values[i]);
#endif
}


OIIO_FORCEINLINE void float8::load (const short *values) {
#if OIIO_SIMD_AVX
    // Rely on the int16->int conversion, then convert to float
    m_simd = _mm256_cvtepi32_ps (int8(values).simd());
#else
    SIMD_CONSTRUCT (values[i]);
#endif
}


OIIO_FORCEINLINE void float8::load (const unsigned char *values) {
#if OIIO_SIMD_AVX
    m_simd = _mm256_cvtepi32_ps (int8(values).simd());
#else
    SIMD_CONSTRUCT (values[i]);
#endif
}


OIIO_FORCEINLINE void float8::load (const char *values) {
#if OIIO_SIMD_AVX
    m_simd = _mm256_cvtepi32_ps (int8(values).simd());
#else
    SIMD_CONSTRUCT (values[i]);
#endif
}

#ifdef _HALF_H_
OIIO_FORCEINLINE void float8::load (const half *values) {
#if OIIO_SIMD_AVX && defined(__F16C__)
    /* Enabled 16 bit float instructions! */
    int4 a ((const int *)values);
    m_simd = _mm256_cvtph_ps (a);
#elif OIIO_SIMD_SSE >= 2
    m_4[0] = float4(values);
    m_4[1] = float4(values+4);
#else /* No SIMD defined: */
    SIMD_CONSTRUCT (values[i]);
#endif
}
#endif /* _HALF_H_ */


OIIO_FORCEINLINE void float8::store (float *values) const {
#if OIIO_SIMD_AVX
    // Use an unaligned store -- it's just as fast when the memory turns
    // out to be aligned, nearly as fast even when unaligned. Not worth
    // the headache of using stores that require alignment.
    _mm256_storeu_ps (values, m_simd);
#else
    SIMD_DO (values[i] = m_val[i]);
#endif
}


OIIO_FORCEINLINE void float8::store (float *values, int n) const {
    DASSERT (n >= 0 && n <= elements);
    // FIXME: is this faster with AVX masked stores?
#if OIIO_SIMD_SSE
    if (n <= 4) {
        lo().store (values, n);
    } else if (n <= 8) {
        lo().store (values);
        hi().store (values+4, n-4);
    }
#else
    for (int i = 0; i < n; ++i)
        values[i] = m_val[i];
#endif
}

#ifdef _HALF_H_
OIIO_FORCEINLINE void float8::store (half *values) const {
#if OIIO_SIMD_AVX && defined(__F16C__)
    __m128i h = _mm256_cvtps_ph (m_simd, (_MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC));
    _mm_storeu_si128 ((__m128i *)values, h);
#else
    SIMD_DO (values[i] = m_val[i]);
#endif
}
#endif


OIIO_FORCEINLINE float8 operator+ (const float8& a, const float8& b) {
#if OIIO_SIMD_AVX
    return _mm256_add_ps (a.m_simd, b.m_simd);
#else
    SIMD_RETURN (float8, a[i] + b[i]);
#endif
}

OIIO_FORCEINLINE const float8 & float8::operator+= (const float8& a) {
#if OIIO_SIMD_AVX
    m_simd = _mm256_add_ps (m_simd, a.m_simd);
#else
    SIMD_DO (m_val[i] += a[i]);
#endif
    return *this;
}

OIIO_FORCEINLINE float8 float8::operator- () const {
#if OIIO_SIMD_AVX
    return _mm256_sub_ps (_mm256_setzero_ps(), m_simd);
#else
    SIMD_RETURN (float8, -m_val[i]);
#endif
}

OIIO_FORCEINLINE float8 operator- (const float8& a, const float8& b) {
#if OIIO_SIMD_AVX
    return _mm256_sub_ps (a.m_simd, b.m_simd);
#else
    SIMD_RETURN (float8, a[i] - b[i]);
#endif
}

OIIO_FORCEINLINE const float8 & float8::operator-= (const float8& a) {
#if OIIO_SIMD_AVX
    m_simd = _mm256_sub_ps (m_simd, a.m_simd);
#else
    SIMD_DO (m_val[i] -= a[i]);
#endif
    return *this;
}

OIIO_FORCEINLINE float8 operator* (const float8& a, const float8& b) {
#if OIIO_SIMD_AVX
    return _mm256_mul_ps (a.m_simd, b.m_simd);
#else
    SIMD_RETURN (float8, a[i] * b[i]);
#endif
}

OIIO_FORCEINLINE const float8 & float8::operator*= (const float8& a) {
#if OIIO_SIMD_AVX
    m_simd = _mm256_mul_ps (m_simd, a.m_simd);
#else
    SIMD_DO (m_val[i] *= a[i]);
#endif
    return *this;
}

OIIO_FORCEINLINE const float8 & float8::operator*= (float val) {
#if OIIO_SIMD_AVX
    m_simd = _mm256_mul_ps (m_simd, _mm256_set1_ps(val));
#else
    SIMD_DO (m_val[i] *= val);
#endif
    return *this;
}

OIIO_FORCEINLINE float8 operator/ (const float8& a, const float8& b) {
#if OIIO_SIMD_AVX
    return _mm256_div_ps (a.m_simd, b.m_simd);
#else
    SIMD_RETURN (float8, a[i] / b[i]);
#endif
}

OIIO_FORCEINLINE const float8 & float8::operator/= (const float8& a) {
#if OIIO_SIMD_AVX
    m_simd = _mm256_div_ps (m_simd, a.m_simd);
#else
    SIMD_DO (m_val[i] /= a[i]);
#endif
    return *this;
}

OIIO_FORCEINLINE const float8 & float8::operator/= (float val) {
#if OIIO_SIMD_AVX
    m_simd = _mm256_div_ps (m_simd, _mm256_set1_ps(val));
#else
    SIMD_DO (m_val[i] /= val);
#endif
    return *this;
}

OIIO_FORCEINLINE bool8 operator== (const float8& a, const float8& b) {
#if OIIO_SIMD_AVX
    return _mm256_cmp_ps (a.m_simd, b.m_simd, _CMP_EQ_OQ);
#else
    SIMD_RETURN (bool8, a[i] == b[i] ? -1 : 0);
#endif
}

OIIO_FORCEINLINE bool8 operator!= (const float8& a, const float8& b) {
#if OIIO_SIMD_AVX
    return _mm256_cmp_ps (a.m_simd, b.m_simd, _CMP_NEQ_OQ);
#else
    SIMD_RETURN (bool8, a[i] != b[i] ? -1 : 0);
#endif
}

OIIO_FORCEINLINE bool8 operator< (const float8& a, const float8& b) {
#if OIIO_SIMD_AVX
    return _mm256_cmp_ps (a.m_simd, b.m_simd, _CMP_LT_OQ);
#else
    SIMD_RETURN (bool8, a[i] < b[i] ? -1 : 0);
#endif
}

OIIO_FORCEINLINE bool8 operator>  (const float8& a, const float8& b) {
#if OIIO_SIMD_AVX
    return _mm256_cmp_ps (a.m_simd, b.m_simd, _CMP_GT_OQ);
#else
    SIMD_RETURN (bool8, a[i] > b[i] ? -1 : 0);
#endif
}

OIIO_FORCEINLINE bool8 operator>= (const float8& a, const float8& b) {
#if OIIO_SIMD_AVX
    return _mm256_cmp_ps (a.m_simd, b.m_simd, _CMP_GE_OQ);
#else
    SIMD_RETURN (bool8, a[i] >= b[i] ? -1 : 0);
#endif
}

OIIO_FORCEINLINE bool8 operator<= (const float8& a, const float8& b) {
#if OIIO_SIMD_AVX
    return _mm256_cmp_ps (a.m_simd, b.m_simd, _CMP_LE_OQ);
#else
    SIMD_RETURN (bool8, a[i] <= b[i] ? -1 : 0);
#endif
}


// Implementation had to be after the definition of float8.
OIIO_FORCEINLINE int8::int8 (const float8& f)
{
#if OIIO_SIMD_AVX
    m_simd = _mm256_cvttps_epi32(f);
#else
    SIMD_CONSTRUCT ((int) f[i]);
#endif
}


template<int i0, int i1, int i2, int i3, int i4, int i5, int i6, int i7>
OIIO_FORCEINLINE float8 shuffle (const float8& a) {
#if OIIO_SIMD_AVX >= 2
    int8 index (i0, i1, i2, i3, i4, i5, i6, i7);
    return _mm256_permutevar8x32_ps (a, index);
#else
    return float8 (a[i0], a[i1], a[i2], a[i3], a[i4], a[i5], a[i6], a[i7]);
#endif
}

template<int i> OIIO_FORCEINLINE float8 shuffle (const float8& a) {
#if OIIO_SIMD_AVX >= 2
    return _mm256_permutevar8x32_ps (a, int8(i));
#else
    return shuffle<i,i,i,i,i,i,i,i>(a);
#endif
}


template<int i>
OIIO_FORCEINLINE float extract (const float8& v) {
#if OIIO_SIMD_AVX_NO_FIXME
    // Looks like the fastest we can do it is to extract a float4,
    // shuffle its one element everywhere, then extract element 0.
    _m128 f4 = _mm256_extractf128_ps (i >> 2);
    int j = i & 3;
    return _mm_cvtss_f32(shuffle_sse<j,j,j,j>(a.simd()));
#else
    return v[i];
#endif
}


template<int i>
OIIO_FORCEINLINE float8 insert (const float8& a, float val) {
#if OIIO_SIMD_AVX_NO_FIXME
    return _mm256_insert_epi32 (a, val, i);
#else
    float8 tmp = a;
    tmp[i] = val;
    return tmp;
#endif
}


OIIO_FORCEINLINE float float8::x () const { return extract<0>(*this); }
OIIO_FORCEINLINE float float8::y () const { return extract<1>(*this); }
OIIO_FORCEINLINE float float8::z () const { return extract<2>(*this); }
OIIO_FORCEINLINE float float8::w () const { return extract<3>(*this); }
OIIO_FORCEINLINE void float8::set_x (float val) { *this = insert<0>(*this, val); }
OIIO_FORCEINLINE void float8::set_y (float val) { *this = insert<1>(*this, val); }
OIIO_FORCEINLINE void float8::set_z (float val) { *this = insert<2>(*this, val); }
OIIO_FORCEINLINE void float8::set_w (float val) { *this = insert<3>(*this, val); }


OIIO_FORCEINLINE int8 bitcast_to_int (const float8& x)
{
#if OIIO_SIMD_AVX
    return _mm256_castps_si256 (x.simd());
#else
    return *(int8 *)&x;
#endif
}

OIIO_FORCEINLINE float8 bitcast_to_float (const int8& x)
{
#if OIIO_SIMD_AVX
    return _mm256_castsi256_ps (x.simd());
#else
    return *(float8 *)&x;
#endif
}


OIIO_FORCEINLINE float8 vreduce_add (const float8& v) {
#if OIIO_SIMD_AVX
    // From Syrah:
    float8 ab_cd_0_0_ef_gh_0_0 = _mm256_hadd_ps(v.simd(), _mm256_setzero_ps());
    float8 abcd_0_0_0_efgh_0_0_0 = _mm256_hadd_ps(ab_cd_0_0_ef_gh_0_0, _mm256_setzero_ps());
    // get efgh in the 0-idx slot
    float8 efgh = shuffle<4>(abcd_0_0_0_efgh_0_0_0);
    float8 final_sum = abcd_0_0_0_efgh_0_0_0 + efgh;
    return shuffle<0>(final_sum);
#else
    float4 hadd4 = vreduce_add(v.lo()) + vreduce_add(v.hi());
    return float8(hadd4, hadd4);
#endif
}


OIIO_FORCEINLINE float reduce_add (const float8& v) {
#if OIIO_SIMD_AVX >= 2
    return extract<0>(vreduce_add(v));
#else
    return reduce_add(v.lo()) + reduce_add(v.hi());
#endif
}


OIIO_FORCEINLINE float8 blend (const float8& a, const float8& b, const bool8& mask)
{
#if OIIO_SIMD_AVX
    return _mm256_blendv_ps (a, b, mask);
#elif defined(OIIO_SIMD_SSE)
    return float8 (blend (a.lo(), b.lo(), mask.lo()),
                   blend (a.hi(), b.hi(), mask.hi()));
#else
    SIMD_RETURN (float8, mask[i] ? b[i] : a[i]);
#endif
}


OIIO_FORCEINLINE float8 blend0 (const float8& a, const bool8& mask)
{
#if OIIO_SIMD_AVX
    return _mm256_and_ps(mask, a);
#elif defined(OIIO_SIMD_SSE)
    return float8 (blend0 (a.lo(), mask.lo()),
                   blend0 (a.hi(), mask.hi()));
#else
    SIMD_RETURN (float8, mask[i] ? a[i] : 0.0f);
#endif
}


OIIO_FORCEINLINE float8 blend0not (const float8& a, const bool8& mask)
{
#if OIIO_SIMD_AVX
    return _mm256_andnot_ps(mask, a);
#elif defined(OIIO_SIMD_SSE)
    return float8 (blend0not (a.lo(), mask.lo()),
                   blend0not (a.hi(), mask.hi()));
#else
    SIMD_RETURN (float8, mask[i] ? 0.0f : a[i]);
#endif
}


OIIO_FORCEINLINE float8 safe_div (const float8 &a, const float8 &b) {
#if OIIO_SIMD_SSE
    return blend0not (a/b, b == float8::Zero());
#else
    SIMD_RETURN (float8, b[i] == 0.0f ? 0.0f : a[i] / b[i]);
#endif
}


OIIO_FORCEINLINE float8 select (const bool8& mask, const float8& a, const float8& b)
{
    return blend (b, a, mask);
}


OIIO_FORCEINLINE float8 abs (const float8& a)
{
#if OIIO_SIMD_AVX
    // Just clear the sign bit for cheap fabsf
    return _mm256_and_ps (a.simd(), _mm256_castsi256_ps(_mm256_set1_epi32(0x7fffffff)));
#else
    SIMD_RETURN (float8, fabsf(a[i]));
#endif
}


OIIO_FORCEINLINE float8 sign (const float8& a)
{
    float8 one(1.0f);
    return blend (one, -one, a < float8::Zero());
}


OIIO_FORCEINLINE float8 ceil (const float8& a)
{
#if OIIO_SIMD_AVX
    return _mm256_ceil_ps (a);
#else
    SIMD_RETURN (float8, ceilf(a[i]));
#endif
}

OIIO_FORCEINLINE float8 floor (const float8& a)
{
#if OIIO_SIMD_AVX
    return _mm256_floor_ps (a);
#else
    SIMD_RETURN (float8, floorf(a[i]));
#endif
}

OIIO_FORCEINLINE float8 round (const float8& a)
{
#if OIIO_SIMD_AVX
    return _mm256_round_ps (a, (_MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC));
#else
    SIMD_RETURN (float8, roundf(a[i]));
#endif
}

OIIO_FORCEINLINE int8 floori (const float8& a)
{
#if OIIO_SIMD_AVX
    return int8(floor(a));
#elif defined(OIIO_SIMD_SSE)   /* SSE2/3 */
    int8 i (a);  // truncates
    int8 isneg = bitcast_to_int (a < float8::Zero());
    return i + isneg;
    // The trick here (thanks, Cycles, for letting me spy on your code) is
    // that the comparison will return (int)-1 for components that are less
    // than zero, and adding that is the same as subtracting one!
#else
    SIMD_RETURN (int8, (int)floorf(a[i]));
#endif
}


OIIO_FORCEINLINE int8 rint (const float8& a)
{
    return int8 (round(a));
}



OIIO_FORCEINLINE float8 sqrt (const float8 &a)
{
#if OIIO_SIMD_AVX
    return _mm256_sqrt_ps (a.simd());
#else
    SIMD_RETURN (float8, sqrtf(a[i]));
#endif
}



OIIO_FORCEINLINE float8 rsqrt (const float8 &a)
{
#if OIIO_SIMD_AVX
    return _mm256_div_ps (_mm256_set1_ps(1.0f), _mm256_sqrt_ps (a.simd()));
#else
    SIMD_RETURN (float8, 1.0f/sqrtf(a[i]));
#endif
}



OIIO_FORCEINLINE float8 rsqrt_fast (const float8 &a)
{
#if OIIO_SIMD_AVX
    return _mm256_rsqrt_ps (a.simd());
#else
    SIMD_RETURN (float8, 1.0f/sqrtf(a[i]));
#endif
}



OIIO_FORCEINLINE float8 min (const float8& a, const float8& b)
{
#if OIIO_SIMD_AVX
    return _mm256_min_ps (a, b);
#else
    SIMD_RETURN (float8, std::min (a[i], b[i]));
#endif
}

OIIO_FORCEINLINE float8 max (const float8& a, const float8& b)
{
#if OIIO_SIMD_AVX
    return _mm256_max_ps (a, b);
#else
    SIMD_RETURN (float8, std::max (a[i], b[i]));
#endif
}


OIIO_FORCEINLINE float8 andnot (const float8& a, const float8& b) {
#if OIIO_SIMD_AVX
    return _mm256_andnot_ps (a.simd(), b.simd());
#else
    const int *ai = (const int *)&a;
    const int *bi = (const int *)&b;
    return bitcast_to_float (int8(~(ai[0]) & bi[0],
                                  ~(ai[1]) & bi[1],
                                  ~(ai[2]) & bi[2],
                                  ~(ai[3]) & bi[3],
                                  ~(ai[4]) & bi[4],
                                  ~(ai[5]) & bi[5],
                                  ~(ai[6]) & bi[6],
                                  ~(ai[7]) & bi[7]));
#endif
}


OIIO_FORCEINLINE float8 madd (const simd::float8& a, const simd::float8& b,
                              const simd::float8& c)
{
#if OIIO_SIMD_AVX && OIIO_FMA_ENABLED
    // If we are sure _mm256_fmadd_ps intrinsic is available, use it.
    return _mm256_fmadd_ps (a, b, c);
#else
    // Fallback: just use regular math and hope for the best.
    return a * b + c;
#endif
}


OIIO_FORCEINLINE float8 msub (const simd::float8& a, const simd::float8& b,
                              const simd::float8& c)
{
#if OIIO_SIMD_AVX && OIIO_FMA_ENABLED
    // If we are sure _mm256_fnmsub_ps intrinsic is available, use it.
    return _mm256_fmsub_ps (a, b, c);
#else
    // Fallback: just use regular math and hope for the best.
    return a * b - c;
#endif
}



OIIO_FORCEINLINE float8 nmadd (const simd::float8& a, const simd::float8& b,
                               const simd::float8& c)
{
#if OIIO_SIMD_AVX && OIIO_FMA_ENABLED
    // If we are sure _mm256_fnmadd_ps intrinsic is available, use it.
    return _mm256_fnmadd_ps (a, b, c);
#else
    // Fallback: just use regular math and hope for the best.
    return c - a * b;
#endif
}



OIIO_FORCEINLINE float8 nmsub (const simd::float8& a, const simd::float8& b,
                               const simd::float8& c)
{
#if OIIO_SIMD_AVX && OIIO_FMA_ENABLED
    // If we are sure _mm256_fnmsub_ps intrinsic is available, use it.
    return _mm256_fnmsub_ps (a, b, c);
#else
    // Fallback: just use regular math and hope for the best.
    return -(a * b) - c;
#endif
}





} // end namespace simd

OIIO_NAMESPACE_END


#undef SIMD_DO
#undef SIMD_CONSTRUCT
#undef SIMD_CONSTRUCT_PAD
#undef SIMD_RETURN
#undef SIMD_RETURN_REDUCE

#endif /* OIIO_SIMD_H */
