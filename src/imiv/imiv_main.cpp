// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_app.h"

#include <string>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/strutil.h>
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
    ap.arg("--backend")
      .help("Renderer backend request: auto, vulkan, metal, opengl")
      .metavar("STRING")
      .defaultval("")
      .action(ArgParse::store());
    ap.arg("--list-backends")
      .help("List backend support compiled into this imiv binary and exit")
      .store_true();
    ap.arg("--devmode")
      .help("Enable Developer menu and developer tools")
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

    Imiv::AppConfig config;
    config.verbose                = ap["verbose"].get<int>() != 0;
    config.foreground_mode        = ap["foreground_mode"].get<int>() != 0;
    config.no_autopremult         = ap["no-autopremult"].get<int>() != 0;
    config.rawcolor               = ap["rawcolor"].get<int>() != 0;
    config.open_dialog            = ap["open-dialog"].get<int>() != 0;
    config.save_dialog            = ap["save-dialog"].get<int>() != 0;
    config.list_backends          = ap["list-backends"].get<int>() != 0;
    config.developer_mode         = ap["devmode"].get<int>() != 0;
    config.developer_mode_explicit = config.developer_mode;
    config.ocio_display           = ap["display"].as_string("");
    config.ocio_image_color_space = ap["image-color-space"].as_string("");
    config.ocio_view              = ap["view"].as_string("");
    config.input_paths            = ap["filename"].as_vec<std::string>();

    const std::string backend_arg = ap["backend"].as_string("");
    if (!backend_arg.empty()
        && !Imiv::parse_backend_kind(backend_arg, config.requested_backend)) {
        print(stderr,
              "imiv: invalid backend '{}'; expected auto, vulkan, metal, or opengl\n",
              backend_arg);
        return EXIT_FAILURE;
    }

    if (config.list_backends) {
        std::string probe_error;
        if (!Imiv::refresh_runtime_backend_info(config.verbose, probe_error)
            && config.verbose && !probe_error.empty()) {
            print(stderr, "imiv: backend availability probe setup failed: {}\n",
                  probe_error);
        }
        print("imiv backend support for this build:\n");
        for (const Imiv::BackendRuntimeInfo& info : Imiv::runtime_backend_info()) {
            std::string description = info.build_info.compiled ? "built"
                                                               : "not built";
            if (info.build_info.compiled) {
                if (info.available) {
                    description += ", available";
                } else if (!info.unavailable_reason.empty()) {
                    description += Strutil::fmt::format(
                        ", unavailable: {}", info.unavailable_reason);
                } else {
                    description += ", unavailable";
                }
            }
            if (info.build_info.active_build)
                description += ", build default backend";
            if (info.build_info.platform_default)
                description += ", platform default";
            print("  {} ({}) : {}\n", info.build_info.display_name,
                  info.build_info.cli_name, description);
        }
        return EXIT_SUCCESS;
    }

    if (!config.foreground_mode)
        Sysutil::put_in_background();

    return Imiv::run(config);
}
