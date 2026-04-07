// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_viewer.h"

namespace Imiv {

ImageViewWindow&
ensure_primary_image_view(MultiViewWorkspace& workspace);
ImageViewWindow*
find_image_view(MultiViewWorkspace& workspace, int view_id);
const ImageViewWindow*
find_image_view(const MultiViewWorkspace& workspace, int view_id);
ImageViewWindow*
active_image_view(MultiViewWorkspace& workspace);
const ImageViewWindow*
active_image_view(const MultiViewWorkspace& workspace);
ImageViewWindow&
append_image_view(MultiViewWorkspace& workspace);
void
sync_workspace_library_state(MultiViewWorkspace& workspace,
                             const ImageLibraryState& library);
void
erase_closed_image_views(MultiViewWorkspace& workspace);

}  // namespace Imiv
