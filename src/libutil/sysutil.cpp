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
#include <boost/regex.hpp>

#ifdef __linux__
# include <sys/sysinfo.h>
# include <unistd.h>
#endif

#ifdef __APPLE__
# include <mach/task.h>
# include <mach/mach_init.h>
# include <mach-o/dyld.h>
# include <unistd.h>
#endif

#ifdef _WIN32
# include "osdep.h"
# include <Psapi.h>
#else
# include <sys/resource.h>
#endif

#include "dassert.h"

#include "sysutil.h"

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
        fscanf (file, "%lu", &vm);  // Just need the first num: vm size
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



bool
Sysutil::date_is_valid(const std::string &date,
        const std::string &date_separator, const std::string &time_separator)
{
    if (!boost::regex_search (date_separator.c_str(), boost::regex ("^[-\\s:/]$|^$")) ||
            !boost::regex_search (time_separator.c_str(), boost::regex ("^[\\s:]$|^$"))) {
        return false;
    }

    std::string pattern;
    pattern = "^[1-9]\\d{3}(" + date_separator + "(0[1-9]|1[0-2])(" +
            date_separator + "([0-2][1-9]|[1-3][01]))?(Z|\\s::)?"
            "([T\\s]?([01]\\d|2[0-3])" + time_separator + "[0-5]\\d" +
            time_separator + "[0-5]\\dZ?(\\.\\d+)?Z?([-+]([01]\\d|2[0-3])" +
            time_separator + "([0-5]\\d))?)?|(::)?)?$";
    boost::regex date_pattern (pattern.c_str(), boost::regex::perl);
    return boost::regex_search (date.c_str(), date_pattern);
}



bool
Sysutil::to_time_t(const std::string &date, time_t &tt)
{
    // Erase separators and fractional seconds.
    boost::regex separator_pattern ("([-:TZ\\s])|(\\.\\d+)", boost::regex::perl);
    std::string tmp_date = boost::regex_replace (date, separator_pattern, "");

    // Insert removed minus sign, if there was one.
    if (tmp_date.length() > 14 && tmp_date[14] != '+') {
        tmp_date = tmp_date.substr(0, 14) + "-" + tmp_date.substr (14);
    }

    if (!Sysutil::date_is_valid (tmp_date, "", ""))
        return false;

    // Insert default values into date, if the date is too short.
    std::string default_date = "19700101000000+0000";
    tmp_date += default_date.substr (tmp_date.length());

    int h_to_utc, m_to_utc;
    struct tm tm_date;
    char sign;
    sscanf (tmp_date.c_str(), "%4d%2d%2d%2d%2d%2d%c%2d%2d", &tm_date.tm_year,
            &tm_date.tm_mon, &tm_date.tm_mday, &tm_date.tm_hour,
            &tm_date.tm_min, &tm_date.tm_sec, &sign, &h_to_utc, &m_to_utc);

    /* Adjust year and month */
    tm_date.tm_year -= 1900;
    tm_date.tm_mon -= 1;
    tm_date.tm_isdst = 0;

    tt = mktime (&tm_date);
    tt += (sign == '-' ? -1 : 1) * (h_to_utc * 60 * 60 + m_to_utc * 60);

    return true;
}
