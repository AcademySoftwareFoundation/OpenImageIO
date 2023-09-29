// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

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



static ArgParse
getargs(int argc, char* argv[])
{
    ArgParse ap;
    // clang-format off
    ap.intro("iv -- image viewer\n"
             OIIO_INTRO_STRING)
      .usage("iv [options] [filename...]")
      .add_version(OIIO_VERSION_STRING);

    ap.arg("filename")
      .action(ArgParse::append())
      .hidden();
    ap.arg("-v")
      .help("Verbose status messages")
      .dest("verbose");
    ap.arg("-F")
      .help("Foreground mode")
      .dest("foreground_mode")
      .store_true();
    ap.arg("--no-autopremult")
      .help("Turn off automatic premultiplication of images with unassociated alpha")
      .store_true();
    ap.arg("--rawcolor")
      .help("Do not automatically transform to RGB");

    ap.parse(argc, (const char**)argv);
    return ap;
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
    ArgParse ap = getargs(argc, argv);

    if (!ap["foreground_mode"].get<int>())
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
    if (ap["no-autopremult"].get<int>())
        imagecache->attribute("unassociatedalpha", 1);
    if (ap["rawcolor"].get<int>())
        mainWin->rawcolor(true);

    // Make sure we are the top window with the focus.
    mainWin->raise();
    mainWin->activateWindow();

    // Add the images
    for (auto& f : ap["filename"].as_vec<std::string>())
        mainWin->add_image(f);

    mainWin->current_image(0);

    int r = app.exec();
    // OK to clean up here

    int verbose = ap["verbose"].get<int>();
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
    shutdown();
    return r;
}
