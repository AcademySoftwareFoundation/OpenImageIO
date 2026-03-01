// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_app.h"

#include <string>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/sysutil.h>

using namespace OIIO;

static ArgParse
getargs(int argc, char* argv[])
{
    ArgParse ap;
    // clang-format off
    ap.intro("imiv -- Dear ImGui image viewer\n"
             OIIO_INTRO_STRING)
      .usage("imiv [options] [filename...]")
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
    ap.arg("--open-dialog")
      .help("Open a native file-open dialog and report the selected file path")
      .store_true();
    ap.arg("--save-dialog")
      .help("Open a native file-save dialog and report the selected file path")
      .store_true();

    ap.parse(argc, (const char**)argv);
    return ap;
    // clang-format on
}

int
main(int argc, char* argv[])
{
    Sysutil::setup_crash_stacktrace("stdout");
    Filesystem::convert_native_arguments(argc, (const char**)argv);
    ArgParse ap = getargs(argc, argv);

    if (!ap["foreground_mode"].get<int>())
        Sysutil::put_in_background();

    Imiv::AppConfig config;
    config.verbose                = ap["verbose"].get<int>() != 0;
    config.foreground_mode        = ap["foreground_mode"].get<int>() != 0;
    config.no_autopremult         = ap["no-autopremult"].get<int>() != 0;
    config.rawcolor               = ap["rawcolor"].get<int>() != 0;
    config.open_dialog            = ap["open-dialog"].get<int>() != 0;
    config.save_dialog            = ap["save-dialog"].get<int>() != 0;
    config.ocio_display           = ap["display"].as_string("");
    config.ocio_image_color_space = ap["image-color-space"].as_string("");
    config.ocio_view              = ap["view"].as_string("");
    config.input_paths            = ap["filename"].as_vec<std::string>();
    return Imiv::run(config);
}
