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
#include <cstring>
#include <cstdio>
#include <string>
#include <iostream>
#include <ctime>

#ifdef __linux__
# include <sys/sysinfo.h>
# include <unistd.h>
# include <sys/ioctl.h>
#endif

#if defined (__FreeBSD__) || defined (__FreeBSD_kernel__)
# include <sys/types.h>
# include <sys/resource.h>
# include <sys/sysctl.h>
# include <sys/wait.h>
# include <sys/ioctl.h>
# include <unistd.h>
#endif

#ifdef __APPLE__
# include <mach/task.h>
# include <mach/mach_init.h>
# include <mach-o/dyld.h>
# include <unistd.h>
# include <sys/ioctl.h>
# include <sys/sysctl.h>
#endif

#include <OpenImageIO/platform.h>

#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# define DEFINE_CONSOLEV2_PROPERTIES
# include <windows.h>
# include <Psapi.h>
# include <cstdio>
# include <io.h>
# include <malloc.h>
#else
# include <sys/resource.h>
#endif

#ifdef __GNU__
# include <unistd.h>
# include <sys/ioctl.h>
#endif

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/strutil.h>

#include <boost/version.hpp>
#include <boost/thread.hpp>


OIIO_NAMESPACE_BEGIN

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
        unsigned long vm = 0, rss = 0;
        int n = fscanf (file, "%lu %lu", &vm, &rss);
        if (n == 2)
            size = size_t(resident ? rss : vm);
        size *= getpagesize();
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

#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    // FIXME -- does somebody know a good method for figuring this out for
    // FreeBSD?
    return 0;   // Punt
#else
    // No idea what platform this is
    ASSERT (0 && "Need to implement Sysutil::memory_used on this platform");
    return 0;   // Punt
#endif
}



size_t
Sysutil::physical_memory ()
{
#if defined(__linux__)
    size_t size = 0;
    FILE *file = fopen ("/proc/meminfo", "r");
    if (file) {
        char buf[1024];
        while (fgets (buf, sizeof(buf), file)) {
            if (! strncmp (buf, "MemTotal:", 9)) {
                size = 1024 * (size_t) strtol (buf+9, NULL, 10);
                break;
            }
        }
        fclose (file);
    }
    return size;

#elif defined(__APPLE__)
    // man 3 sysctl   ...or...
    // https://developer.apple.com/library/mac/#documentation/Darwin/Reference/ManPages/man3/sysctl.3.html
    // http://stackoverflow.com/questions/583736/determine-physical-mem-size-programmatically-on-osx
    int mib[2] = { CTL_HW, HW_MEMSIZE };
    int64_t physical_memory;
    size_t length = sizeof(physical_memory);
    sysctl (mib, 2, &physical_memory, &length, NULL, 0);
    return size_t(physical_memory);

#elif defined(_WIN32)
    // According to MSDN
    // (http://msdn.microsoft.com/en-us/library/windows/desktop/aa366589(v=vs.85).aspx
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof (statex);
    GlobalMemoryStatusEx (&statex);
    return size_t (statex.ullTotalPhys);  // Total physical memory
    // N.B. Other fields nice to know (in bytes, except for dwMemoryLoad):
    //        statex.dwMemoryLoad      Percent of memory in use
    //        statex.ullAvailPhys      Free physical memory
    //        statex.ullTotalPageFile  Total size of paging file
    //        statex.ullAvailPageFile  Free mem in paging file
    //        statex.ullTotalVirtual   Total virtual memory
    //        statex.ullAvailVirtual   Free virtual memory

#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    // man 3 sysctl   ...or...
    // http://www.freebsd.org/cgi/man.cgi?query=sysctl&sektion=3
    // FIXME -- Does this accept a size_t?  Or only an int?  I can't
    // seem to find an online resource that indicates that FreeBSD has a
    // HW_MEMESIZE like Linux and OSX have, or a HW_PHYSMEM64 like
    // OpenBSD has.
    int mib[2] = { CTL_HW, HW_PHYSMEM };
    size_t physical_memory;
    size_t length = sizeof(physical_memory);
    sysctl (mib, 2, &physical_memory, &length, NULL, 0);
    return physical_memory;

#else
    // No idea what platform this is
    ASSERT (0 && "Need to implement Sysutil::physical_memory on this platform");
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

#if defined(__linux__)
    unsigned int size = sizeof(filename);
    int r = readlink ("/proc/self/exe", filename, size);
    ASSERT(r < int(size)); // user won't get the right answer if the filename is too long to store
    if (r > 0) filename[r] = 0; // readlink does not fill in the 0 byte
#elif defined(__APPLE__)
    // For info:  'man 3 dyld'
    unsigned int size = sizeof(filename);
    int r = _NSGetExecutablePath (filename, &size);
    if (r == 0)
        r = size;
#elif defined(_WIN32)
    // According to MSDN...
    unsigned int size = sizeof(filename);
    int r = GetModuleFileName (NULL, filename, size);
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    int mib[4];
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PATHNAME;
    mib[3] = -1;
    size_t cb = sizeof(filename);
    int r=1;
    sysctl(mib, 4, filename, &cb, NULL, 0);
#elif defined(__GNU__) || defined(__OpenBSD__)
    int r = 0;
#else
    // No idea what platform this is
    OIIO_STATIC_ASSERT_MSG (0, "this_program_path() unimplemented on this platform");
#endif

    if (r > 0)
        return std::string (filename);
    return std::string();   // Couldn't figure it out
}



string_view
Sysutil::getenv (string_view name)
{
    return string_view (::getenv (name.c_str()));
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

#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__GNU__)
    struct winsize w;
    ioctl (0, TIOCGWINSZ, &w);
    columns = w.ws_col;
#elif defined(_WIN32)
    HANDLE h = GetStdHandle (STD_OUTPUT_HANDLE);
    if (h != INVALID_HANDLE_VALUE) {
        CONSOLE_SCREEN_BUFFER_INFO csbi = { { 0 } };
        GetConsoleScreenBufferInfo (h, &csbi);
        columns = csbi.dwSize.X;
    }
#endif

    return columns;
}



int
Sysutil::terminal_rows ()
{
    int rows = 24;   // a decent guess, if we have nothing more to go on

#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__GNU__)
    struct winsize w;
    ioctl (0, TIOCGWINSZ, &w);
    rows = w.ws_row;
#elif defined(_WIN32)
    HANDLE h = GetStdHandle (STD_OUTPUT_HANDLE);
    if (h != INVALID_HANDLE_VALUE) {
        CONSOLE_SCREEN_BUFFER_INFO csbi = { { 0 } };
        GetConsoleScreenBufferInfo (h, &csbi);
        rows = csbi.dwSize.Y;
    }
#endif

    return rows;
}



#ifdef _WIN32
int isatty(int fd) { return _isatty(fd); }
#endif


Term::Term (FILE *file)
{
    m_is_console = isatty (fileno((file)));
}



#ifdef _WIN32
// from https://msdn.microsoft.com/fr-fr/library/windows/desktop/mt638032%28v=vs.85%29.aspx

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

bool enableVTMode()
{
    // Set output mode to handle virtual terminal sequences
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode))
    {
        return false;
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode))
    {
        return false;
    }
    return true;
}
#endif



Term::Term (const std::ostream &stream)
{
    m_is_console = (&stream == &std::cout && isatty(fileno(stdout)))
                || (&stream == &std::cerr && isatty(fileno(stderr)))
                || (&stream == &std::clog && isatty(fileno(stderr)));

#ifdef _WIN32
    if (m_is_console)
        enableVTMode();
#else
    // Non-windows: also check the TERM env variable for a terminal known to
    // be capable of the color codes. List copied from the Apache-licensed
    // Google Benchmark: https://github.com/google/benchmark/blob/master/src/colorprint.cc
    const char* const supported_terminals[] = {
        "cygwin", "linux", "rxvt-unicode", "rxvt-unicode-256color",
        "screen", "screen-256color", "tmux", "tmux-256color",
        "xterm", "xterm-256color", "xterm-color"
    };
    string_view TERM = Sysutil::getenv ("TERM");
    bool term_supports_color = false;
    for (auto t : supported_terminals)
        term_supports_color |= (TERM == t);
    m_is_console &= term_supports_color;
    // FIXME/NOTE: It's possible that this will fail to print color for some
    // terminal emulator omitted from the list. Will Rosecrans suggests
    // using the shell command 'tput colors' for a more authoritative
    // answer. LG isn't sure under what conditions that might break and
    // doesn't have time to look into it further at this time.
    // https://github.com/OpenImageIO/oiio/pull/1752
    // Some day we should return to this if we come to rely more heavily on
    // this console coloring as a core feature.
#endif
}



std::string
Term::ansi (string_view command) const
{
    static const char *codes[] = {
        "default",    "0",
        "normal",     "0",
        "reset",      "0",
        "bold",       "1",
        "italic",     "3",       // Not widely supported, sometimes inverse
        "underscore", "4",
        "underline",  "4",
        "blink",      "5",
        "reverse",    "7",
        "concealed",  "8",
        "strike",     "9",       // Not widely supported
        "black",      "30",
        "red",        "31",
        "green",      "32",
        "yellow",     "33",
        "blue",       "34",
        "magenta",    "35",
        "cyan",       "36",
        "white",      "37",
        "black_bg",   "40",
        "red_bg",     "41",
        "green_bg",   "42",
        "yellow_bg",  "43",
        "blue_bg",    "44",
        "magenta_bg", "45",
        "cyan_bg",    "46",
        "white_bg",   "47",
        NULL
    };
    std::string ret;
    if (is_console()) {
        std::vector<string_view> cmds;
        Strutil::split (command, cmds, ",");
        for (size_t c = 0; c < cmds.size(); ++c) {
            for (size_t i = 0; codes[i]; i += 2)
                if (codes[i] == cmds[c]) {
                    ret += c ? ";" : "\033[";
                    ret += codes[i+1];
                }
        }
        ret += "m";
    }
    return ret;
}



std::string
Term::ansi_fgcolor (int r, int g, int b)
{
    std::string ret;
    if (is_console()) {
        r = std::max (0, std::min (255, r));
        g = std::max (0, std::min (255, g));
        b = std::max (0, std::min (255, b));
        ret = Strutil::format ("\033[38;2;%d;%d;%dm", r, g, b);
    }
    return ret;
}



std::string
Term::ansi_bgcolor (int r, int g, int b)
{
    std::string ret;
    if (is_console()) {
        r = std::max (0, std::min (255, r));
        g = std::max (0, std::min (255, g));
        b = std::max (0, std::min (255, b));
        ret = Strutil::format ("\033[48;2;%d;%d;%dm", r, g, b);
    }
    return ret;
}



bool
#if !defined(_MSC_VER)
Sysutil::put_in_background (int argc, char* argv[])
#else
Sysutil::put_in_background (int, char* [])
#endif
{
    // You would think that this would be sufficient:
    //   pid_t pid = fork ();
    //   if (pid < 0)       // Some kind of error, we were unable to background
    //      return false;
    //   if (pid == 0)
    //       return true;   // This is the child process, so continue with life
    //   // Otherwise, this is the parent process, so terminate
    //   exit (0); 
    // But it's not.  On OS X, it's not safe to fork() if your app is linked
    // against certain libraries or frameworks.  So the only thing that I
    // think is safe is to exec a new process.
    // Another solution is this:
    //    daemon (1, 1);
    // But it suffers from the same problem on OS X, and seems to just be
    // a wrapper for fork.

#if defined(__linux__) || defined(__GLIBC__)
    // Simplest case:
    // daemon returns 0 if successful, thus return true if successful
    return daemon (1, 1) == 0;
#endif

#ifdef __APPLE__
    std::string newcmd = std::string(argv[0]) + " -F";
    for (int i = 1;  i < argc;  ++i) {
        newcmd += " \"";
        newcmd += argv[i];
        newcmd += "\"";
    }
    newcmd += " &";
    if (system (newcmd.c_str()) != -1)
        exit (0);
    return true;
#endif

#ifdef WIN32
    return true;
#endif

    // Otherwise, we don't know what to do
    return false;
}



unsigned int
Sysutil::hardware_concurrency ()
{
    return boost::thread::hardware_concurrency();
}



unsigned int
Sysutil::physical_concurrency ()
{
#if BOOST_VERSION >= 105600
    return boost::thread::physical_concurrency();
#else
    return boost::thread::hardware_concurrency();
#endif
}



size_t
Sysutil::max_open_files ()
{
#if defined(_WIN32)
    return size_t(_getmaxstdio());
#else
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0)
        return rl.rlim_cur;
#endif
    return size_t(-1); // Couldn't figure out, so return effectively infinity
}

void* aligned_malloc(std::size_t size, std::size_t align) {
#if defined(_WIN32)
    return _aligned_malloc(size, align);
#else
    void* ptr;
    return posix_memalign(&ptr, align, size) == 0 ? ptr : nullptr;
#endif
}

void aligned_free(void* ptr) {
#if defined(_WIN32)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}


OIIO_NAMESPACE_END
