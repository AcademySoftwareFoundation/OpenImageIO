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

#include <QApplication>

#include "imageviewer.h"
#include <OpenImageIO/timer.h>
#include <OpenImageIO/argparse.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/filesystem.h>

using namespace OIIO;


static bool verbose = false;
static bool foreground_mode = false;
static std::vector<std::string> filenames;



static int
parse_files (int argc, const char *argv[])
{
    for (int i = 0;  i < argc;  i++)
        filenames.emplace_back(argv[i]);
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

#ifdef WIN32
    // if we are not in DEBUG mode this code switch the app to
    // full windowed mode (no console and no need to define WinMain)
    // FIXME: this should be done in CMakeLists.txt but first we have to
    // fix Windows Debug build
# ifdef NDEBUG
#  pragma comment(linker, "/subsystem:windows /entry:mainCRTStartup")
# endif
#endif


int
main (int argc, char *argv[])
{
    Filesystem::convert_native_arguments (argc, (const char **)argv);
    getargs (argc, argv);

    if (! foreground_mode)
        Sysutil::put_in_background (argc, argv);

    // LG
//    Q_INIT_RESOURCE(iv);
    QApplication app(argc, argv);
    ImageViewer *mainWin = new ImageViewer;
    mainWin->show();

    // Set up the imagecache with parameters that make sense for iv
    ImageCache *imagecache = ImageCache::create (true);
    imagecache->attribute ("autotile", 256);
    imagecache->attribute ("deduplicate", (int)0);

    // Make sure we are the top window with the focus.
    mainWin->raise ();
    mainWin->activateWindow ();

    // Add the images
    for (const auto &s : filenames) {
        mainWin->add_image (s);
    }

    mainWin->current_image (0);

    int r = app.exec();
    // OK to clean up here

#ifdef NDEBUG
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
