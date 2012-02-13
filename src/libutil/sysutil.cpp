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
#include <ctime>

#ifdef __linux__
# include <sys/sysinfo.h>
# include <unistd.h>
# include <sys/ioctl.h>
#endif

#ifdef __FreeBSD__
# include <sys/types.h>
# include <sys/resource.h>
# include <sys/sysctl.h>
# include <sys/wait.h>
# include <sys/ioctl.h>
#endif

#ifdef __APPLE__
# include <mach/task.h>
# include <mach/mach_init.h>
# include <mach-o/dyld.h>
# include <unistd.h>
# include <sys/ioctl.h>
#endif

#ifdef _WIN32
# include "osdep.h"
# include <Psapi.h>
#else
# include <sys/resource.h>
#endif

#include "dassert.h"

#include "sysutil.h"

OIIO_NAMESPACE_ENTER
{

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
        int n = fscanf (file, "%lu", &vm);  // Just need the first num: vm size
        if (n == 1)
            size = (size_t)vm * getpagesize();
        fclose (file);
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



void
Sysutil::get_local_time (const time_t *time, struct tm *converted_time)
{
#ifdef _MSC_VER
    localtime_s (converted_time, time);
#else
    localtime_r (time, converted_time);
#endif
}



std::string
Sysutil::this_program_path ()
{
    char filename[10240];
    filename[0] = 0;
    unsigned int size = sizeof(filename);

#if defined(__linux__)
    int r = readlink ("/proc/self/exe", filename, size);
#elif defined(__APPLE__)
    // For info:  'man 3 dyld'
    int r = _NSGetExecutablePath (filename, &size);
    if (r == 0)
        r = size;
#elif defined(_WIN32)
    // According to MSDN...
    int r = GetModuleFileName (NULL, filename, size);
#elif defined(__FreeBSD__)
    int mib[4];
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PATHNAME;
    mib[3] = -1;
//  char filename[1024];
    size_t cb = sizeof(filename);
    int r=1;
    sysctl(mib, 4, filename, &cb, NULL, 0);
#else
    // No idea what platform this is
    ASSERT (0);
#endif

    if (r > 0)
        return std::string (filename);
    return std::string();   // Couldn't figure it out
}



void
Sysutil::usleep (unsigned long useconds)
{
#ifdef _WIN32
    Sleep (useconds/1000);   // Win32 Sleep() is milliseconds, not micro
#else
    ::usleep (useconds);     // *nix usleep() is in microseconds
#endif
}



int
Sysutil::terminal_columns ()
{
    int columns = 80;   // a decent guess, if we have nothing more to go on

#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
    struct winsize w;
    ioctl (0, TIOCGWINSZ, &w);
    columns = w.ws_col;
#elif defined(_WIN32)
    // FIXME: is there a Windows equivalent?
#endif

    return columns;
}


}
OIIO_NAMESPACE_EXIT
