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
#error Do not include this file directly; include tbb_machine.h instead
#endif

#include <windows.h>

#if defined(__INTEL_COMPILER)
#define __TBB_fence_for_acquire() __asm { __asm nop }
#define __TBB_fence_for_release() __asm { __asm nop }
#elif _MSC_VER >= 1300
extern "C" void _ReadWriteBarrier();
#pragma intrinsic(_ReadWriteBarrier)
#define __TBB_fence_for_acquire() _ReadWriteBarrier()
#define __TBB_fence_for_release() _ReadWriteBarrier()
#endif

#define __TBB_WORDSIZE 8
#define __TBB_BIG_ENDIAN 0

// ATTENTION: if you ever change argument types in machine-specific primitives,
// please take care of atomic_word<> specializations in tbb/atomic.h
extern "C" {
    __int8 __TBB_machine_cmpswp1 (volatile void *ptr, __int8 value, __int8 comparand );
    __int8 __TBB_machine_fetchadd1 (volatile void *ptr, __int8 addend );
    __int8 __TBB_machine_fetchstore1 (volatile void *ptr, __int8 value );
    __int16 __TBB_machine_cmpswp2 (volatile void *ptr, __int16 value, __int16 comparand );
    __int16 __TBB_machine_fetchadd2 (volatile void *ptr, __int16 addend );
    __int16 __TBB_machine_fetchstore2 (volatile void *ptr, __int16 value );
    void __TBB_machine_pause (__int32 delay );
}


#if !__INTEL_COMPILER
extern "C" unsigned char _BitScanReverse64( unsigned long* i, unsigned __int64 w );
#pragma intrinsic(_BitScanReverse64)
#endif

inline __int64 __TBB_machine_lg( unsigned __int64 i ) {
#if __INTEL_COMPILER
    unsigned __int64 j;
    __asm
    {
        bsr rax, i
        mov j, rax
    }
#else
    unsigned long j;
    _BitScanReverse64( &j, i );
#endif
    return j;
}

inline void __TBB_machine_OR( volatile void *operand, uintptr_t addend ) {
    InterlockedOr64((LONGLONG *)operand, addend); 
}

inline void __TBB_machine_AND( volatile void *operand, uintptr_t addend ) {
    InterlockedAnd64((LONGLONG *)operand, addend); 
}

#define __TBB_CompareAndSwap1(P,V,C) __TBB_machine_cmpswp1(P,V,C)
#define __TBB_CompareAndSwap2(P,V,C) __TBB_machine_cmpswp2(P,V,C)
#define __TBB_CompareAndSwap4(P,V,C) InterlockedCompareExchange( (LONG *) P , V , C ) 
#define __TBB_CompareAndSwap8(P,V,C) InterlockedCompareExchange64( (LONGLONG *) P , V , C )
#define __TBB_CompareAndSwapW(P,V,C) InterlockedCompareExchange64( (LONGLONG *) P , V , C )

#define __TBB_FetchAndAdd1(P,V) __TBB_machine_fetchadd1(P,V)
#define __TBB_FetchAndAdd2(P,V) __TBB_machine_fetchadd2(P,V)
#define __TBB_FetchAndAdd4(P,V) ( InterlockedAdd((LONG *) P , V ) - V ) 
#define __TBB_FetchAndAdd8(P,V) ( InterlockedAdd64((LONGLONG *) P , V ) - V ) 
#define __TBB_FetchAndAddW(P,V)  ( InterlockedAdd64((LONGLONG *) P , V ) - V ) 

#define __TBB_FetchAndStore1(P,V) __TBB_machine_fetchstore1(P,V)
#define __TBB_FetchAndStore2(P,V) __TBB_machine_fetchstore2(P,V)
#define __TBB_FetchAndStore4(P,V) InterlockedExchange((LONG *) P , V )
#define __TBB_FetchAndStore8(P,V) InterlockedExchange64((LONGLONG *) P , V )
#define __TBB_FetchAndStoreW(P,V)  InterlockedExchange64((LONGLONG *) P , V ) 

// Not used if wordsize == 8
#undef __TBB_Store8
#undef __TBB_Load8

#define __TBB_AtomicOR(P,V) __TBB_machine_OR(P,V)
#define __TBB_AtomicAND(P,V) __TBB_machine_AND(P,V)

// Definition of other functions
#if !defined(_WIN32_WINNT)
extern "C" BOOL WINAPI SwitchToThread(void);
#endif
#define __TBB_Yield()  SwitchToThread()
#define __TBB_Pause(V) __TBB_machine_pause(V)
#define __TBB_Log2(V)    __TBB_machine_lg(V)

#if !__INTEL_COMPILER
extern "C" void __cpuid( int cpuinfo[4], int mode );
#pragma intrinsic(__cpuid)
#endif

#define __TBB_cpuid
inline void __TBB_x86_cpuid( __int32 buffer[4], __int32 mode ) {
#if __INTEL_COMPILER
    __asm
    {
        mov eax,mode
        cpuid
        mov rdi,buffer
        mov [rdi+0],eax
        mov [rdi+4],ebx
        mov [rdi+8],ecx
        mov [rdi+12],edx
    }
#else
    __cpuid(buffer, mode);
#endif
}

