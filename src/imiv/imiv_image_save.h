// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_viewer.h"

#include <string>

namespace Imiv {

bool
save_loaded_image(const LoadedImage& image, const std::string& path,
                  std::string& error_message);
bool
save_window_image(const LoadedImage& image, const ViewRecipe& recipe,
                  const PlaceholderUiState& ui_state, const std::string& path,
                  std::string& error_message);
bool
save_selection_image(const LoadedImage& image, const ViewerState& viewer,
                     const std::string& path, std::string& error_message);
bool
save_export_selection_image(const LoadedImage& image, const ViewerState& viewer,
                            const PlaceholderUiState& ui_state,
                            const std::string& path,
                            std::string& error_message);

}  // namespace Imiv
