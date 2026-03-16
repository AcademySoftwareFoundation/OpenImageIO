// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <string>
#include <vector>

namespace Imiv {

enum class RenderBackend { VulkanGlfw = 0, MetalGlfw = 1, OpenGLGlfw = 2 };

struct AppConfig {
    bool verbose         = false;
    bool foreground_mode = false;
    bool rawcolor        = false;
    bool no_autopremult  = false;
    bool open_dialog     = false;
    bool save_dialog     = false;

    std::string ocio_display;
    std::string ocio_image_color_space;
    std::string ocio_view;

    std::vector<std::string> input_paths;
};

RenderBackend
default_backend();
const char*
backend_name(RenderBackend backend);
int
run(const AppConfig& config);

}  // namespace Imiv
