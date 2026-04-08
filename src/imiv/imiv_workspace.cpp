// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_workspace.h"

#include "imiv_image_library.h"

#include <utility>

namespace Imiv {

ImageViewWindow&
append_image_view(MultiViewWorkspace& workspace)
{
    std::unique_ptr<ImageViewWindow> view(new ImageViewWindow());
    view->id = workspace.next_view_id++;
    workspace.view_windows.push_back(std::move(view));
    return *workspace.view_windows.back();
}

ImageViewWindow&
ensure_primary_image_view(MultiViewWorkspace& workspace)
{
    if (workspace.view_windows.empty()) {
        ImageViewWindow& primary = append_image_view(workspace);
        workspace.active_view_id = primary.id;
    }
    return *workspace.view_windows.front();
}



ImageViewWindow*
find_image_view(MultiViewWorkspace& workspace, int view_id)
{
    for (const std::unique_ptr<ImageViewWindow>& view :
         workspace.view_windows) {
        if (view != nullptr && view->id == view_id)
            return view.get();
    }
    return nullptr;
}



const ImageViewWindow*
find_image_view(const MultiViewWorkspace& workspace, int view_id)
{
    for (const std::unique_ptr<ImageViewWindow>& view :
         workspace.view_windows) {
        if (view != nullptr && view->id == view_id)
            return view.get();
    }
    return nullptr;
}



ImageViewWindow*
active_image_view(MultiViewWorkspace& workspace)
{
    ImageViewWindow* active = find_image_view(workspace,
                                              workspace.active_view_id);
    if (active != nullptr)
        return active;
    if (workspace.view_windows.empty())
        return nullptr;
    workspace.active_view_id = workspace.view_windows.front()->id;
    return workspace.view_windows.front().get();
}



const ImageViewWindow*
active_image_view(const MultiViewWorkspace& workspace)
{
    const ImageViewWindow* active = find_image_view(workspace,
                                                    workspace.active_view_id);
    if (active != nullptr)
        return active;
    return workspace.view_windows.empty()
               ? nullptr
               : workspace.view_windows.front().get();
}

void
sync_workspace_library_state(MultiViewWorkspace& workspace,
                             const ImageLibraryState& library)
{
    for (const std::unique_ptr<ImageViewWindow>& view :
         workspace.view_windows) {
        if (view == nullptr)
            continue;
        sync_viewer_library_state(view->viewer, library);
    }
}



void
erase_closed_image_views(MultiViewWorkspace& workspace)
{
    if (workspace.view_windows.size() <= 1)
        return;

    const int primary_view_id = workspace.view_windows.front() != nullptr
                                    ? workspace.view_windows.front()->id
                                    : 0;
    size_t write_index        = 0;
    const size_t view_count   = workspace.view_windows.size();
    for (size_t read_index = 0; read_index < view_count; ++read_index) {
        std::unique_ptr<ImageViewWindow>& view
            = workspace.view_windows[read_index];
        if (view != nullptr && !view->open && view->id != primary_view_id)
            continue;
        if (write_index != read_index)
            workspace.view_windows[write_index] = std::move(view);
        ++write_index;
    }
    workspace.view_windows.resize(write_index);

    if (find_image_view(workspace, workspace.active_view_id) == nullptr) {
        const ImageViewWindow& primary = ensure_primary_image_view(workspace);
        workspace.active_view_id       = primary.id;
    }
}

}  // namespace Imiv
