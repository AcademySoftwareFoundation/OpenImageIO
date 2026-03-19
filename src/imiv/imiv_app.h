// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_backend.h"

#include <string>
#include <vector>

namespace Imiv {

struct AppConfig {
    bool verbose         = false;
    bool foreground_mode = false;
    bool rawcolor        = false;
    bool no_autopremult  = false;
    bool open_dialog     = false;
    bool save_dialog     = false;
    bool list_backends   = false;

    BackendKind requested_backend = BackendKind::Auto;

    std::string ocio_display;
    std::string ocio_image_color_space;
    std::string ocio_view;

    std::vector<std::string> input_paths;
};

int
run(const AppConfig& config);

}  // namespace Imiv
