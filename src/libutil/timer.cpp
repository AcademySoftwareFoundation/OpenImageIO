// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cstdio>
#include <cstdlib>

#include <OpenImageIO/timer.h>

#ifdef _WIN32
#    include <windows.h>
#endif


OIIO_NAMESPACE_BEGIN

double Timer::seconds_per_tick;
Timer::ticks_t Timer::ticks_per_second;

class TimerSetupOnce {
public:
    TimerSetupOnce()
    {
#ifdef _WIN32
        // From MSDN web site
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        Timer::ticks_per_second = Timer::ticks_t(freq.QuadPart);
        Timer::seconds_per_tick = 1.0 / (double)freq.QuadPart;
#elif defined(__APPLE__)
        // NOTE(boulos): Both this value and that of the windows
        // counterpart above only need to be calculated once. In
        // Manta, we stored this on the side as a scaling factor but
        // that would require a .cpp file (meaning timer.h can't just
        // be used as a header). It is also claimed that since
        // Leopard, Apple returns 1 for both numer and denom.
        mach_timebase_info_data_t time_info;
        mach_timebase_info(&time_info);
        Timer::seconds_per_tick = (1e-9 * static_cast<double>(time_info.numer))
                                  / static_cast<double>(time_info.denom);
        Timer::ticks_per_second = Timer::ticks_t(1.0f
                                                 / Timer::seconds_per_tick);
#elif OIIO_TIMER_LINUX_USE_clock_gettime
        // Defaults based on a nanosecond resolution timer: clock_gettime()
        Timer::seconds_per_tick = 1.0e-9;
        Timer::ticks_per_second = 1000000000;
#else
        // Defaults based on a microsecond resolution timer: gettimeofday()
        Timer::seconds_per_tick = 1.0e-6;
        Timer::ticks_per_second = 1000000;
#endif
        // Note: For anything but Windows and Mac, we rely on gettimeofday,
        // which is microsecond timing, so there's nothing to set up.
    }
};

static TimerSetupOnce once;

#ifdef _WIN32
// a non-inline function on Windows, to avoid
// including windows headers from OIIO public header.
Timer::ticks_t
Timer::now(void) const
{
    LARGE_INTEGER n;
    QueryPerformanceCounter(&n);
    return n.QuadPart;
}
#endif

OIIO_NAMESPACE_END
