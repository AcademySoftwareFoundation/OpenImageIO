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

#include <cstdlib>
#include <string>
#include <iostream>

#ifdef __linux__
# include <sys/sysinfo.h>
#endif

#ifdef __APPLE__
# include <mach/task.h>
# include <mach/mach_init.h>
#endif

#ifdef _WIN32
# include <windows.h>
# include <Psapi.h>
#else
# include <sys/resource.h>
#endif

#include "dassert.h"

#define DLL_EXPORT_PUBLIC /* Because we are implementing Plugin */
#include "sysutil.h"
#undef DLL_EXPORT_PUBLIC

using namespace Sysutil;


size_t
Sysutil::memory_used (bool resident)
{
#if defined(__linux__)
#if 0
    // doesn't seem to work?
    struct rusage ru;
    int ret = getrusage (RUSAGE_SELF, &ru);
    return (size_t)ru.ru_maxrss * (size_t)1024;
#else
    // Ugh, getrusage doesn't work well on Linux.  Try grabbing info
    // directly from the /proc pseudo-filesystem.  Reading from
    // /proc/self/statm gives info on your own process, as one line of
    // numbers that are: virtual mem program size, resident set size,
    // shared pages, text/code, data/stack, library, dirty pages.  The
    // mem sizes should all be multiplied by the page size.
    size_t size = 0;
    FILE *file = fopen("/proc/self/statm", "r");
    if (file) {
        unsigned long vm = 0;
        fscanf (file, "%ul", &vm);  // Just need the first num: vm size
        fclose (file);
        size = (size_t)vm * getpagesize();
    }
    return size;
#endif

#elif defined(__APPLE__)
    // Inspired by:
    // http://miknight.blogspot.com/2005/11/resident-set-size-in-mac-os-x.html
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;
    task_info(current_task(), TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count);
    size_t size = (resident ? t_info.resident_size : t_info.virtual_size);
    return size;

#elif defined(_WIN32)
    // According to MSDN...
    PROCESS_MEMORY_COUNTERS counters;
    if (GetProcessMemoryInfo (GetCurrentProcess(), &counters, sizeof (counters)))
        return counters.PagefileUsage;
    else return 0;

#else
    // No idea what platform this is
    ASSERT (0);
    return 0;   // Punt
#endif
}

