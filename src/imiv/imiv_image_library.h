// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_viewer.h"

namespace Imiv {

bool
collect_directory_image_paths(const std::string& directory_path,
                              ImageSortMode sort_mode, bool sort_reverse,
                              std::vector<std::string>& out_paths,
                              std::string& error_message);
bool
add_loaded_image_path(ImageLibraryState& library, const std::string& path,
                      int* out_index = nullptr);
bool
append_loaded_image_paths(ImageLibraryState& library,
                          const std::vector<std::string>& paths,
                          int* out_first_added_index = nullptr);
bool
remove_loaded_image_path(ImageLibraryState& library, ViewerState* viewer,
                         const std::string& path);
bool
set_current_loaded_image_path(const ImageLibraryState& library,
                              ViewerState& viewer, const std::string& path);
bool
pick_loaded_image_path(const ImageLibraryState& library,
                       const ViewerState& viewer, int delta,
                       std::string& out_path);
void
sort_loaded_image_paths(ImageLibraryState& library,
                        const std::vector<ViewerState*>& viewers);
void
sync_viewer_library_state(ViewerState& viewer,
                          const ImageLibraryState& library);
void
add_recent_image_path(ImageLibraryState& library, const std::string& path);

}  // namespace Imiv
