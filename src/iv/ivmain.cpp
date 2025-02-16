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
      .usage("iv [options] [filename... | dirname...]")
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

    ap.arg("--display")
      .help("OCIO display")
      .metavar("STRING")
      .defaultval("")
      .action(ArgParse::store());
    ap.arg("--image-color-space")
      .help("OCIO image color space")
      .metavar("STRING")
      .defaultval("")
      .action(ArgParse::store());
    ap.arg("--view")
      .help("OCIO view")
      .metavar("STRING")
      .defaultval("")
      .action(ArgParse::store());
    
    ap.parse(argc, (const char**)argv);
    return ap;
    // clang-format on
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

    std::string color_space = ap["image-color-space"].as_string("");
    std::string display     = ap["display"].as_string("");
    std::string view        = ap["view"].as_string("");
    //    std::string look = ap["look"].as_string("");

    bool use_ocio       = color_space != "" && display != "" && view != "";
    std::string ocioenv = Sysutil::getenv("OCIO");
    if (ocioenv.empty() || !Filesystem::exists(ocioenv)) {
#ifdef _MSC_VER
        _putenv_s("OCIO", "ocio://default");
#else
        setenv("OCIO", "ocio://default", 1);
#endif
    }

    ImageViewer* mainWin = new ImageViewer(use_ocio, color_space, display,
                                           view);

    mainWin->show();

    // Set up the imagecache with parameters that make sense for iv
    auto imagecache = ImageCache::create(true);
    imagecache->attribute("autotile", 256);
    imagecache->attribute("deduplicate", (int)0);
    if (ap["no-autopremult"].get<int>())
        imagecache->attribute("unassociatedalpha", 1);
    if (ap["rawcolor"].get<int>())
        mainWin->rawcolor(true);

    // Make sure we are the top window with the focus.
    mainWin->raise();
    mainWin->activateWindow();

    ustring uexists("exists");
    std::vector<std::string> extensionsVector;  // Vector to hold all extensions
    auto all_extensions = OIIO::get_string_attribute("extension_list");
    for (auto oneformat : OIIO::Strutil::splitsv(all_extensions, ";")) {
        // Split the extensions by semicolon
        auto format_exts = OIIO::Strutil::splitsv(oneformat, ":", 2);
        for (auto ext : OIIO::Strutil::splitsv(format_exts[1], ","))
            extensionsVector.emplace_back(ext);
    }

    // Add the images
    for (auto& f : ap["filename"].as_vec<std::string>()) {
        // Check if the file exists
        if (!Filesystem::exists(f)) {
            print(stderr, "Error: File or directory does not exist: {}\n", f);
            continue;
        }

        if (Filesystem::is_directory(f)) {
            // If f is a directory, iterate through its files
            std::vector<std::string> files;
            Filesystem::get_directory_entries(f, files);

            std::vector<std::string> validImages;  // Vector to hold valid images
            for (auto& file : files) {
                std::string extension = Filesystem::extension(file).substr(
                    1);  // Remove the leading dot
                if (std::find(extensionsVector.begin(), extensionsVector.end(),
                              extension)
                    != extensionsVector.end()) {
                    int exists = 0;
                    bool ok    = imagecache->get_image_info(ustring(file), 0, 0,
                                                            uexists, OIIO::TypeInt,
                                                            &exists);
                    if (ok && exists)
                        validImages.push_back(file);
                }
            }

            if (validImages.empty()) {
                print(stderr, "Error: No valid images found in directory: {}\n",
                      f);
            } else {
                // Sort the valid images lexicographically
                std::sort(validImages.begin(), validImages.end());
                for (auto& validImage : validImages) {
                    mainWin->add_image(validImage);
                }
            }
        } else {
            mainWin->add_image(f);
        }
    }

    mainWin->current_image(0);

    int r = app.exec();
    // OK to clean up here

    int verbose = ap["verbose"].get<int>();
#ifdef NDEBUG
    if (verbose)
#endif
    {
        size_t mem = Sysutil::memory_used(true);
        print("iv total memory used: {}\n\n", Strutil::memformat(mem));
        print("{}\n", imagecache->getstats(1 + verbose));
    }
    shutdown();
    return r;
}
