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


/////////////////////////////////////////////////////////////////////////
/// @file   atomic.h
///
/// @brief  Wrappers and utilities for atomics.
/////////////////////////////////////////////////////////////////////////


#ifndef OPENIMAGEIO_ATOMIC_H
#define OPENIMAGEIO_ATOMIC_H

#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/platform.h>

#include <atomic>
#define OIIO_USE_STDATOMIC 1



OIIO_NAMESPACE_BEGIN

using std::atomic;
typedef atomic<int> atomic_int;
typedef atomic<long long> atomic_ll;



/// Atomically set avar to the minimum of its current value and bval.
template<typename T>
OIIO_FORCEINLINE void
atomic_min (atomic<T> &avar, const T &bval)
{
    do {
        T a = avar.load();
        if (a <= bval || avar.compare_exchange_weak (a,bval))
            break;
    } while (true);
}


/// Atomically set avar to the maximum of its current value and bval.
template<typename T>
OIIO_FORCEINLINE void
atomic_max (atomic<T> &avar, const T &bval)
{
    do {
        T a = avar.load();
        if (a >= bval || avar.compare_exchange_weak (a,bval))
            break;
    } while (true);
}



// Add atomically to a float and return the original value.
OIIO_FORCEINLINE float
atomic_fetch_add (atomic<float> &a, float f)
{
    do {
        float oldval = a.load();
        float newval = oldval + f;
        if (a.compare_exchange_weak (oldval, newval))
            return oldval;
    } while (true);
}


// Add atomically to a double and return the original value.
OIIO_FORCEINLINE double
atomic_fetch_add (atomic<double> &a, double f)
{
    do {
        double oldval = a.load();
        double newval = oldval + f;
        if (a.compare_exchange_weak (oldval, newval))
            return oldval;
    } while (true);
}


OIIO_NAMESPACE_END


#endif // OPENIMAGEIO_ATOMIC_H
