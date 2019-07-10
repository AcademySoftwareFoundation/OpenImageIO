// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#include <cstdio>
#include <cstdlib>

#include <OpenImageIO/strutil.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/timer.h>


OIIO_NAMESPACE_BEGIN

double Timer::seconds_per_tick = 1.0e-6;


class TimerSetupOnce {
public:
    TimerSetupOnce()
    {
#ifdef _WIN32
        // From MSDN web site
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
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
#endif
    }
};

static TimerSetupOnce once;



OIIO_NAMESPACE_END
