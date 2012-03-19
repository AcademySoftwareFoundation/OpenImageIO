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

#if defined(_MSC_VER)
// Ignore warnings about conditional expressions that always evaluate true
// on a given platform but may evaluate differently on another. There's
// nothing wrong with such conditionals.
// Also ignore warnings about not being able to generate default assignment
// operators for some Qt classes included in headers below.
#  pragma warning (disable : 4127 4512)
#endif

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <iostream>
#include <iterator>

#include <QtGui/QApplication>

#include <boost/foreach.hpp>

#include "imageio.h"
#include "imageviewer.h"
#include "timer.h"
#include "argparse.h"
#include "sysutil.h"
#include "strutil.h"
#include "imagecache.h"

OIIO_NAMESPACE_USING;


static bool verbose = false;
static bool foreground_mode = false;
static std::vector<std::string> filenames;



static int
parse_files (int argc, const char *argv[])
{
    for (int i = 0;  i < argc;  i++)
        filenames.push_back (argv[i]);
    return 0;
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
    ap.options ("iv -- image viewer\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  iv [options] [filename...]",
                  "%*", parse_files, "",
                  "--help", &help, "Print help message",
                  "-v", &verbose, "Verbose status messages",
                  "-F", &foreground_mode, "Foreground mode",
                  NULL);
    if (ap.parse (argc, (const char**)argv) < 0) {
        std::cerr << ap.geterror() << std::endl;
        ap.usage ();
        exit (EXIT_FAILURE);
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }
}



/// Try to put the process into the background so it doesn't continue to
/// tie up any shell that it was launched from.  Return true if successful,
/// false if it was unable to do so.
static bool
#if !defined(_MSC_VER)
put_in_background (int argc, char* argv[])
#else
put_in_background (int, char* [])
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

#ifdef __linux
    // Simplest case:
    daemon (1, 1);
    return true;
#endif


#ifdef __APPLE__
#if 0
    // Another solution -- But I can't seem to make this work!
    std::vector<char *> newargs;
    newargs.push_back ("-F");
    for (int i = 0;  i < argc;  ++i)
        newargs.push_back (argv[i]);
    newargs.push_back (NULL);
    if (fork())
        exit (0);
    execv ("/Users/lg/lg/proj/oiio/dist/macosx/bin/iv" /*argv[0]*/, &newargs[0]);
    exit (0);
#endif

#if 1
    // This one works -- just call system(), but we have to properly
    // quote all the arguments in case filenames have spaces in them.
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


#endif

#ifdef WIN32
    // if we are not in DEBUG mode this code switch the IV to
    // full windowed mode (no console and no need to define WinMain)
    // FIXME: this should be done in CMakeLists.txt but first we have to
    // fix Windows Debug build
# ifndef DEBUG
#  pragma comment(linker, "/subsystem:windows /entry:mainCRTStartup")
# endif
    return true;
#endif
}



int
main (int argc, char *argv[])
{
    getargs (argc, argv);

    if (! foreground_mode)
        put_in_background (argc, argv);

    // LG
//    Q_INIT_RESOURCE(iv);
    QApplication app(argc, argv);
    ImageViewer *mainWin = new ImageViewer;
    mainWin->show();

    // Set up the imagecache with parameters that make sense for iv
    ImageCache *imagecache = ImageCache::create (true);
    imagecache->attribute ("autotile", 256);

    // Make sure we are the top window with the focus.
    mainWin->raise ();
    mainWin->activateWindow ();

    // Add the images
    BOOST_FOREACH (const std::string &s, filenames) {
        mainWin->add_image (s);
    }

    mainWin->current_image (0);

    int r = app.exec();
    // OK to clean up here

#ifndef DEBUG
    if (verbose)
#endif
    {
        size_t mem = Sysutil::memory_used (true);
        std::cout << "iv total memory used: " << Strutil::memformat (mem) << "\n";
        std::cout << "\n";
        std::cout << imagecache->getstats (1+verbose) << "\n";
    }

    return r;
}
