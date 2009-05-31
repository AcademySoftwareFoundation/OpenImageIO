/*
    Copyright 2005-2008 Intel Corporation.  All Rights Reserved.

    This file is part of Threading Building Blocks.

    Threading Building Blocks is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    Threading Building Blocks is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Threading Building Blocks; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/

#ifndef __TBB_machine_H
#define __TBB_machine_H

#include "tbb/tbb_stddef.h"

#if _WIN32||_WIN64

#ifdef _MANAGED
#pragma managed(push, off)
#endif

#if defined(_M_IX86)
#include "tbb/machine/windows_ia32.h"
#elif defined(_M_AMD64) 
#include "tbb/machine/windows_em64t.h"
#else
#error Unsupported platform
#endif

#ifdef _MANAGED
#pragma managed(pop)
#endif

#elif __linux__ || __FreeBSD__

#if __i386__
#include "tbb/machine/linux_ia32.h"
#elif __x86_64__
#include "tbb/machine/linux_em64t.h"
#elif __ia64__
#include "tbb/machine/linux_itanium.h"
#endif

#elif __APPLE__

#if __i386__
#include "tbb/machine/linux_ia32.h"
#elif __x86_64__
#include "tbb/machine/linux_em64t.h"
#elif __POWERPC__
#include "tbb/machine/mac_ppc.h"
#endif

#elif _AIX

#include "tbb/machine/ibm_aix51.h"

#elif __sun || __SUNPRO_CC

#define __asm__ asm 
#define __volatile__ volatile
#if __i386  || __i386__
#include "tbb/machine/linux_ia32.h"
#elif __x86_64__
#include "tbb/machine/linux_em64t.h"
#endif

#endif

#if !defined(__TBB_CompareAndSwap4) || !defined(__TBB_CompareAndSwap8) || !defined(__TBB_Yield)
#error Minimal requirements for tbb_machine.h not satisfied 
#endif

#ifndef __TBB_load_with_acquire
    //! Load with acquire semantics; i.e., no following memory operation can move above the load.
    template<typename T>
    inline T __TBB_load_with_acquire(const volatile T& location) {
        T temp = location;
#ifdef __TBB_fence_for_acquire 
        __TBB_fence_for_acquire();
#endif /* __TBB_fence_for_acquire */
        return temp;
    }
#endif

#ifndef __TBB_store_with_release
    //! Store with release semantics; i.e., no prior memory operation can move below the store.
    template<typename T, typename V>
    inline void __TBB_store_with_release(volatile T& location, V value) {
#ifdef __TBB_fence_for_release
        __TBB_fence_for_release();
#endif /* __TBB_fence_for_release */
        location = value; 
    }
#endif

#ifndef __TBB_Pause
    inline void __TBB_Pause(int32_t) {
        __TBB_Yield();
    }
#endif

namespace tbb {
namespace internal {

//! Class that implements exponential backoff.
/** See implementation of SpinwaitWhileEq for an example. */
class AtomicBackoff {
    //! Time delay, in units of "pause" instructions. 
    /** Should be equal to approximately the number of "pause" instructions
        that take the same time as an context switch. */
    static const int32_t LOOPS_BEFORE_YIELD = 16;
    int32_t count;
public:
    AtomicBackoff() : count(1) {}

    //! Pause for a while.
    void pause() {
        if( count<=LOOPS_BEFORE_YIELD ) {
            __TBB_Pause(count);
            // Pause twice as long the next time.
            count*=2;
        } else {
            // Pause is so long that we might as well yield CPU to scheduler.
            __TBB_Yield();
        }
    }

    // pause for a few times and then return false immediately.
    bool bounded_pause() {
        if( count<=LOOPS_BEFORE_YIELD ) {
            __TBB_Pause(count);
            // Pause twice as long the next time.
            count*=2;
            return true;
        } else {
            return false;
        }
    }

    void reset() {
        count = 1;
    }
};

template<size_t S, typename T>
inline intptr_t __TBB_MaskedCompareAndSwap (volatile int32_t *ptr, T value, T comparand ) {
    T *base = (T *)( (uintptr_t)(ptr) & ~(uintptr_t)(0x3) );
#if __TBB_BIG_ENDIAN
    const uint8_t bitoffset = ( (4-S) - ( (uint8_t *)ptr - (uint8_t *)base) ) * 8;
#else
    const uint8_t bitoffset = ( (uint8_t *)ptr - (uint8_t *)base ) * 8;
#endif
    const uint32_t mask = ( (1<<(S*8) ) - 1)<<bitoffset;
    AtomicBackoff b;
    uint32_t result;
    for(;;) {
        result = *(volatile uint32_t *)base;
        uint32_t old_value = ( result & ~mask ) | ( comparand << bitoffset );
        uint32_t new_value = ( result & ~mask ) | ( value << bitoffset );
        // __TBB_CompareAndSwap4 presumed to have full fence. 
        uint32_t tmp = __TBB_CompareAndSwap4( base, new_value, old_value );
        if( tmp==old_value || ((tmp^old_value)&mask)!=0 ) 
            break;
        b.pause();
    }
    return (T)((result & mask) >> bitoffset);
}

template<size_t S, typename T>
inline T __TBB_CompareAndSwapGeneric (volatile void *ptr, T value, T comparand ) { 
    return __TBB_CompareAndSwapW((T *)ptr,value,comparand);
}

template<>
inline uint8_t __TBB_CompareAndSwapGeneric <1,uint8_t> (volatile void *ptr, uint8_t value, uint8_t comparand ) {
#ifdef __TBB_CompareAndSwap1
    return __TBB_CompareAndSwap1(ptr,value,comparand);
#else
    return __TBB_MaskedCompareAndSwap<1,uint8_t>((volatile int32_t *)ptr,value,comparand);
#endif
}

template<>
inline uint16_t __TBB_CompareAndSwapGeneric <2,uint16_t> (volatile void *ptr, uint16_t value, uint16_t comparand ) {
#ifdef __TBB_CompareAndSwap2
    return __TBB_CompareAndSwap2(ptr,value,comparand);
#else
    return __TBB_MaskedCompareAndSwap<2,uint16_t>((volatile int32_t *)ptr,value,comparand);
#endif
}

template<>
inline uint32_t __TBB_CompareAndSwapGeneric <4,uint32_t> (volatile void *ptr, uint32_t value, uint32_t comparand ) { 
    return __TBB_CompareAndSwap4(ptr,value,comparand);
}

template<>
inline uint64_t __TBB_CompareAndSwapGeneric <8,uint64_t> (volatile void *ptr, uint64_t value, uint64_t comparand ) { 
    return __TBB_CompareAndSwap8(ptr,value,comparand);
}

template<size_t S, typename T>
inline T __TBB_FetchAndAddGeneric (volatile void *ptr, T addend) {
    AtomicBackoff b;
    T result;
    for(;;) {
        result = *reinterpret_cast<volatile T *>(ptr);
        // __TBB_CompareAndSwapGeneric presumed to have full fence. 
        if( __TBB_CompareAndSwapGeneric<S,T> ( ptr, result+addend, result )==result ) 
            break;
        b.pause();
    }
    return result;
}

template<size_t S, typename T>
inline T __TBB_FetchAndStoreGeneric (volatile void *ptr, T value) {
    AtomicBackoff b;
    T result;
    for(;;) {
        result = *reinterpret_cast<volatile T *>(ptr);
        // __TBB_CompareAndSwapGeneric presumed to have full fence.
        if( __TBB_CompareAndSwapGeneric<S,T> ( ptr, value, result )==result ) 
            break;
        b.pause();
    }
    return result;
}

// Macro __TBB_TypeWithAlignmentAtLeastAsStrict(T) should be a type with alignment at least as 
// strict as type T.  Type type should have a trivial default constructor and destructor, so that
// arrays of that type can be declared without initializers.  
// It is correct (but perhaps a waste of space) if __TBB_TypeWithAlignmentAtLeastAsStrict(T) expands
// to a type bigger than T.
// The default definition here works on machines where integers are naturally aligned and the
// strictest alignment is 16.
#ifndef __TBB_TypeWithAlignmentAtLeastAsStrict

#if __GNUC__ || __SUNPRO_CC
struct __TBB_machine_type_with_strictest_alignment {
    int member[4];
} __attribute__((aligned(16)));
#elif _MSC_VER
__declspec(align(16)) struct __TBB_machine_type_with_strictest_alignment {
    int member[4];
};
#else
#error Must define __TBB_TypeWithAlignmentAtLeastAsStrict(T) or __TBB_machine_type_with_strictest_alignment
#endif

template<size_t N> struct type_with_alignment {__TBB_machine_type_with_strictest_alignment member;};
template<> struct type_with_alignment<1> { char member; };
template<> struct type_with_alignment<2> { uint16_t member; };
template<> struct type_with_alignment<4> { uint32_t member; };
template<> struct type_with_alignment<8> { uint64_t member; };

#if _MSC_VER||defined(__GNUC__)&&__GNUC__==3 && __GNUC_MINOR__<=2  
//! Work around for bug in GNU 3.2 and MSVC compilers.
/** Bug is that compiler sometimes returns 0 for __alignof(T) when T has not yet been instantiated.
    The work-around forces instantiation by forcing computation of sizeof(T) before __alignof(T). */
template<size_t Size, typename T> 
struct work_around_alignment_bug {
#if _MSC_VER
    static const size_t alignment = __alignof(T);
#else
    static const size_t alignment = __alignof__(T);
#endif
};
#define __TBB_TypeWithAlignmentAtLeastAsStrict(T) tbb::internal::type_with_alignment<tbb::internal::work_around_alignment_bug<sizeof(T),T>::alignment>
#elif __GNUC__ || __SUNPRO_CC
#define __TBB_TypeWithAlignmentAtLeastAsStrict(T) tbb::internal::type_with_alignment<__alignof__(T)>
#else
#define __TBB_TypeWithAlignmentAtLeastAsStrict(T) __TBB_machine_type_with_strictest_alignment
#endif
#endif  /* ____TBB_TypeWithAlignmentAtLeastAsStrict */

} // namespace internal
} // namespace tbb

#ifndef __TBB_CompareAndSwap1
#define __TBB_CompareAndSwap1 tbb::internal::__TBB_CompareAndSwapGeneric<1,uint8_t>
#endif

#ifndef __TBB_CompareAndSwap2 
#define __TBB_CompareAndSwap2 tbb::internal::__TBB_CompareAndSwapGeneric<2,uint16_t>
#endif

#ifndef __TBB_CompareAndSwapW
#define __TBB_CompareAndSwapW tbb::internal::__TBB_CompareAndSwapGeneric<sizeof(ptrdiff_t),ptrdiff_t>
#endif

#ifndef __TBB_FetchAndAdd1
#define __TBB_FetchAndAdd1 tbb::internal::__TBB_FetchAndAddGeneric<1,uint8_t>
#endif

#ifndef __TBB_FetchAndAdd2
#define __TBB_FetchAndAdd2 tbb::internal::__TBB_FetchAndAddGeneric<2,uint16_t>
#endif

#ifndef __TBB_FetchAndAdd4
#define __TBB_FetchAndAdd4 tbb::internal::__TBB_FetchAndAddGeneric<4,uint32_t>
#endif

#ifndef __TBB_FetchAndAdd8
#define __TBB_FetchAndAdd8 tbb::internal::__TBB_FetchAndAddGeneric<8,uint64_t>
#endif

#ifndef __TBB_FetchAndAddW
#define __TBB_FetchAndAddW tbb::internal::__TBB_FetchAndAddGeneric<sizeof(ptrdiff_t),ptrdiff_t>
#endif

#ifndef __TBB_FetchAndStore1
#define __TBB_FetchAndStore1 tbb::internal::__TBB_FetchAndStoreGeneric<1,uint8_t>
#endif

#ifndef __TBB_FetchAndStore2
#define __TBB_FetchAndStore2 tbb::internal::__TBB_FetchAndStoreGeneric<2,uint16_t>
#endif

#ifndef __TBB_FetchAndStore4
#define __TBB_FetchAndStore4 tbb::internal::__TBB_FetchAndStoreGeneric<4,uint32_t>
#endif

#ifndef __TBB_FetchAndStore8
#define __TBB_FetchAndStore8 tbb::internal::__TBB_FetchAndStoreGeneric<8,uint64_t>
#endif

#ifndef __TBB_FetchAndStoreW
#define __TBB_FetchAndStoreW tbb::internal::__TBB_FetchAndStoreGeneric<sizeof(ptrdiff_t),ptrdiff_t>
#endif

#if __TBB_DECL_FENCED_ATOMICS

#ifndef __TBB_CompareAndSwap1__TBB_full_fence
#define __TBB_CompareAndSwap1__TBB_full_fence __TBB_CompareAndSwap1
#endif 
#ifndef __TBB_CompareAndSwap1acquire
#define __TBB_CompareAndSwap1acquire __TBB_CompareAndSwap1__TBB_full_fence
#endif 
#ifndef __TBB_CompareAndSwap1release
#define __TBB_CompareAndSwap1release __TBB_CompareAndSwap1__TBB_full_fence
#endif 

#ifndef __TBB_CompareAndSwap2__TBB_full_fence
#define __TBB_CompareAndSwap2__TBB_full_fence __TBB_CompareAndSwap2
#endif
#ifndef __TBB_CompareAndSwap2acquire
#define __TBB_CompareAndSwap2acquire __TBB_CompareAndSwap2__TBB_full_fence
#endif
#ifndef __TBB_CompareAndSwap2release
#define __TBB_CompareAndSwap2release __TBB_CompareAndSwap2__TBB_full_fence
#endif

#ifndef __TBB_CompareAndSwap4__TBB_full_fence
#define __TBB_CompareAndSwap4__TBB_full_fence __TBB_CompareAndSwap4
#endif 
#ifndef __TBB_CompareAndSwap4acquire
#define __TBB_CompareAndSwap4acquire __TBB_CompareAndSwap4__TBB_full_fence
#endif 
#ifndef __TBB_CompareAndSwap4release
#define __TBB_CompareAndSwap4release __TBB_CompareAndSwap4__TBB_full_fence
#endif 

#ifndef __TBB_CompareAndSwap8__TBB_full_fence
#define __TBB_CompareAndSwap8__TBB_full_fence __TBB_CompareAndSwap8
#endif
#ifndef __TBB_CompareAndSwap8acquire
#define __TBB_CompareAndSwap8acquire __TBB_CompareAndSwap8__TBB_full_fence
#endif
#ifndef __TBB_CompareAndSwap8release
#define __TBB_CompareAndSwap8release __TBB_CompareAndSwap8__TBB_full_fence
#endif

#ifndef __TBB_FetchAndAdd1__TBB_full_fence
#define __TBB_FetchAndAdd1__TBB_full_fence __TBB_FetchAndAdd1
#endif
#ifndef __TBB_FetchAndAdd1acquire
#define __TBB_FetchAndAdd1acquire __TBB_FetchAndAdd1__TBB_full_fence
#endif
#ifndef __TBB_FetchAndAdd1release
#define __TBB_FetchAndAdd1release __TBB_FetchAndAdd1__TBB_full_fence
#endif

#ifndef __TBB_FetchAndAdd2__TBB_full_fence
#define __TBB_FetchAndAdd2__TBB_full_fence __TBB_FetchAndAdd2
#endif
#ifndef __TBB_FetchAndAdd2acquire
#define __TBB_FetchAndAdd2acquire __TBB_FetchAndAdd2__TBB_full_fence
#endif
#ifndef __TBB_FetchAndAdd2release
#define __TBB_FetchAndAdd2release __TBB_FetchAndAdd2__TBB_full_fence
#endif

#ifndef __TBB_FetchAndAdd4__TBB_full_fence
#define __TBB_FetchAndAdd4__TBB_full_fence __TBB_FetchAndAdd4
#endif
#ifndef __TBB_FetchAndAdd4acquire
#define __TBB_FetchAndAdd4acquire __TBB_FetchAndAdd4__TBB_full_fence
#endif
#ifndef __TBB_FetchAndAdd4release
#define __TBB_FetchAndAdd4release __TBB_FetchAndAdd4__TBB_full_fence
#endif

#ifndef __TBB_FetchAndAdd8__TBB_full_fence
#define __TBB_FetchAndAdd8__TBB_full_fence __TBB_FetchAndAdd8
#endif
#ifndef __TBB_FetchAndAdd8acquire
#define __TBB_FetchAndAdd8acquire __TBB_FetchAndAdd8__TBB_full_fence
#endif
#ifndef __TBB_FetchAndAdd8release
#define __TBB_FetchAndAdd8release __TBB_FetchAndAdd8__TBB_full_fence
#endif

#ifndef __TBB_FetchAndStore1__TBB_full_fence
#define __TBB_FetchAndStore1__TBB_full_fence __TBB_FetchAndStore1
#endif
#ifndef __TBB_FetchAndStore1acquire
#define __TBB_FetchAndStore1acquire __TBB_FetchAndStore1__TBB_full_fence
#endif
#ifndef __TBB_FetchAndStore1release
#define __TBB_FetchAndStore1release __TBB_FetchAndStore1__TBB_full_fence
#endif

#ifndef __TBB_FetchAndStore2__TBB_full_fence
#define __TBB_FetchAndStore2__TBB_full_fence __TBB_FetchAndStore2
#endif
#ifndef __TBB_FetchAndStore2acquire
#define __TBB_FetchAndStore2acquire __TBB_FetchAndStore2__TBB_full_fence
#endif
#ifndef __TBB_FetchAndStore2release
#define __TBB_FetchAndStore2release __TBB_FetchAndStore2__TBB_full_fence
#endif

#ifndef __TBB_FetchAndStore4__TBB_full_fence
#define __TBB_FetchAndStore4__TBB_full_fence __TBB_FetchAndStore4
#endif
#ifndef __TBB_FetchAndStore4acquire
#define __TBB_FetchAndStore4acquire __TBB_FetchAndStore4__TBB_full_fence
#endif
#ifndef __TBB_FetchAndStore4release
#define __TBB_FetchAndStore4release __TBB_FetchAndStore4__TBB_full_fence
#endif

#ifndef __TBB_FetchAndStore8__TBB_full_fence
#define __TBB_FetchAndStore8__TBB_full_fence __TBB_FetchAndStore8
#endif
#ifndef __TBB_FetchAndStore8acquire
#define __TBB_FetchAndStore8acquire __TBB_FetchAndStore8__TBB_full_fence
#endif
#ifndef __TBB_FetchAndStore8release
#define __TBB_FetchAndStore8release __TBB_FetchAndStore8__TBB_full_fence
#endif

#endif // __TBB_DECL_FENCED_ATOMICS

// Special atomic functions
#ifndef __TBB_FetchAndAddWrelease
#define __TBB_FetchAndAddWrelease __TBB_FetchAndAddW
#endif

#ifndef __TBB_FetchAndIncrementWacquire
#define __TBB_FetchAndIncrementWacquire(P) __TBB_FetchAndAddW(P,1)
#endif

#ifndef __TBB_FetchAndDecrementWrelease
#define __TBB_FetchAndDecrementWrelease(P) __TBB_FetchAndAddW(P,(-1))
#endif

#if __TBB_WORDSIZE==4
// On 32-bit platforms, "atomic.h" requires definition of __TBB_Store8 and __TBB_Load8
#ifndef __TBB_Store8
inline void __TBB_Store8 (volatile void *ptr, int64_t value) {
    tbb::internal::AtomicBackoff b;
    for(;;) {
        int64_t result = *(int64_t *)ptr;
        if( __TBB_CompareAndSwap8(ptr,value,result)==result ) break;
        b.pause();
    }
}
#endif

#ifndef __TBB_Load8
inline int64_t __TBB_Load8 (const volatile void *ptr) {
    int64_t result = *(int64_t *)ptr;
    result = __TBB_CompareAndSwap8((volatile void *)ptr,result,result);
    return result;
}
#endif
#endif /* __TBB_WORDSIZE==4 */

#ifndef __TBB_Log2
inline intptr_t __TBB_Log2( uintptr_t x ) {
    long result = -1;
    for(; x; x>>=1 ) ++result;
    return result;
}
#endif

#ifndef __TBB_AtomicOR
inline void __TBB_AtomicOR( volatile void *operand, uintptr_t addend ) {
    tbb::internal::AtomicBackoff b;
    for(;;) {
        uintptr_t tmp = *(volatile uintptr_t *)operand;
        uintptr_t result = __TBB_CompareAndSwapW(operand, tmp|addend, tmp);
        if( result==tmp ) break;
        b.pause();
    }
}
#endif

#ifndef __TBB_AtomicAND
inline void __TBB_AtomicAND( volatile void *operand, uintptr_t addend ) {
    tbb::internal::AtomicBackoff b;
    for(;;) {
        uintptr_t tmp = *(volatile uintptr_t *)operand;
        uintptr_t result = __TBB_CompareAndSwapW(operand, tmp&addend, tmp);
        if( result==tmp ) break;
        b.pause();
    }
}
#endif

#ifndef __TBB_TryLockByte
inline bool __TBB_TryLockByte( unsigned char &flag ) {
    return __TBB_CompareAndSwap1(&flag,1,0)==0;
}
#endif

#ifndef __TBB_LockByte
inline uintptr_t __TBB_LockByte( unsigned char& flag ) {
    if ( !__TBB_TryLockByte(flag) ) {
        tbb::internal::AtomicBackoff b;
        do {
            b.pause();
        } while ( !__TBB_TryLockByte(flag) );
    }
    return 0;
}
#endif

#endif /* __TBB_machine_H */
