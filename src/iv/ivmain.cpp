// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#if defined(_MSC_VER)
// Ignore warnings about conditional expressions that always evaluate true
// on a given platform but may evaluate differently on another. There's
// nothing wrong with such conditionals.
// Also ignore warnings about not being able to generate default assignment
// operators for some Qt classes included in headers below.
#    pragma warning(disable : 4127 4512)
#endif

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <iterator>

#include <QApplication>

#include "imageviewer.h"
#include <OpenImageIO/argparse.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/timer.h>

using namespace OIIO;


static bool verbose         = false;
static bool foreground_mode = false;
static bool autopremult     = true;
static bool rawcolor        = false;
static std::vector<std::string> filenames;



static int
parse_files(int argc, const char* argv[])
{
    for (int i = 0; i < argc; i++)
        filenames.emplace_back(argv[i]);
    return 0;
}



static void
getargs(int argc, char* argv[])
{
    bool help = false;
    ArgParse ap;
    // clang-format off
    ap.options ("iv -- image viewer\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  iv [options] [filename...]",
                "%*", parse_files, "",
                "--help", &help, "Print help message",
                "-v", &verbose, "Verbose status messages",
                "-F", &foreground_mode, "Foreground mode",
                "--no-autopremult %!", &autopremult,
                    "Turn off automatic premultiplication of images with unassociated alpha",
                "--rawcolor", &rawcolor,
                    "Do not automatically transform to RGB",
                nullptr);
    // clang-format on
    if (ap.parse(argc, (const char**)argv) < 0) {
        std::cerr << ap.geterror() << std::endl;
        ap.usage();
        exit(EXIT_FAILURE);
    }
    if (help) {
        ap.usage();
        exit(EXIT_SUCCESS);
    }
}

#ifdef _MSC_VER
// if we are not in DEBUG mode this code switch the app to
// full windowed mode (no console and no need to define WinMain)
// FIXME: this should be done in CMakeLists.txt but first we have to
// fix Windows Debug build
#    ifdef NDEBUG
#        pragma comment(linker, "/subsystem:windows /entry:mainCRTStartup")
#    endif
#endif


int
main(int argc, char* argv[])
{
    // Helpful for debugging to make sure that any crashes dump a stack
    // trace.
    Sysutil::setup_crash_stacktrace("stdout");

    Filesystem::convert_native_arguments(argc, (const char**)argv);
    getargs(argc, argv);

    if (!foreground_mode)
        Sysutil::put_in_background(argc, argv);

    // LG
    //    Q_INIT_RESOURCE(iv);
    QApplication app(argc, argv);
    ImageViewer* mainWin = new ImageViewer;
    mainWin->show();

    // Set up the imagecache with parameters that make sense for iv
    ImageCache* imagecache = ImageCache::create(true);
    imagecache->attribute("autotile", 256);
    imagecache->attribute("deduplicate", (int)0);
    if (!autopremult)
        imagecache->attribute("unassociatedalpha", 1);
    if (rawcolor)
        mainWin->rawcolor(true);

    // Make sure we are the top window with the focus.
    mainWin->raise();
    mainWin->activateWindow();

    // Add the images
    for (const auto& s : filenames) {
        mainWin->add_image(s);
    }

    mainWin->current_image(0);

    int r = app.exec();
    // OK to clean up here

#ifdef NDEBUG
    if (verbose)
#endif
    {
        size_t mem = Sysutil::memory_used(true);
        std::cout << "iv total memory used: " << Strutil::memformat(mem)
                  << "\n";
        std::cout << "\n";
        std::cout << imagecache->getstats(1 + verbose) << "\n";
    }

    return r;
}
