// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace Imiv {

struct PlaceholderUiState;
struct ViewerState;
struct ImageLibraryState;

std::filesystem::path
imgui_ini_load_path();
bool
load_persistent_state(PlaceholderUiState& ui_state, ViewerState& viewer,
                      ImageLibraryState& library, std::string& error_message);
bool
save_persistent_state(const PlaceholderUiState& ui_state,
                      const ViewerState& viewer,
                      const ImageLibraryState& library,
                      const char* imgui_ini_text, size_t imgui_ini_size,
                      std::string& error_message);

}  // namespace Imiv
