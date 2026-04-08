// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_file_actions.h"

#include "imiv_actions.h"
#include "imiv_file_dialog.h"
#include "imiv_image_library.h"
#include "imiv_image_save.h"
#include "imiv_workspace.h"

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <imgui.h>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>

using namespace OIIO;

namespace Imiv {

namespace {

    enum class SaveDialogKind {
        SaveImage = 0,
        SaveWindow,
        SaveSelection,
        ExportSelection
    };

    std::filesystem::path executable_directory_path()
    {
        const std::string program_path = Sysutil::this_program_path();
        if (program_path.empty())
            return std::filesystem::path();
        return std::filesystem::path(program_path).parent_path();
    }

    std::filesystem::path default_screenshot_output_path()
    {
        std::filesystem::path base_dir = executable_directory_path();
        if (base_dir.empty())
            base_dir = std::filesystem::current_path();
        base_dir /= "screenshots";

        std::tm local_tm      = {};
        const std::time_t now = std::time(nullptr);
#if defined(_WIN32)
        localtime_s(&local_tm, &now);
#else
        localtime_r(&now, &local_tm);
#endif

        char timestamp[64] = {};
        if (std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S",
                          &local_tm)
            == 0) {
            std::snprintf(timestamp, sizeof(timestamp), "%lld",
                          static_cast<long long>(now));
        }

        std::filesystem::path path
            = base_dir / Strutil::fmt::format("imiv_{}.png", timestamp);
        for (int suffix = 1; std::filesystem::exists(path); ++suffix) {
            path = base_dir
                   / Strutil::fmt::format("imiv_{}_{:02d}.png", timestamp,
                                          suffix);
        }
        return path;
    }

    std::vector<ViewerState*>
    collect_action_target_viewers(MultiViewWorkspace* workspace,
                                  ViewerState& active_view)
    {
        std::vector<ViewerState*> viewers;
        if (workspace == nullptr) {
            viewers.push_back(&active_view);
            return viewers;
        }
        viewers.reserve(workspace->view_windows.size());
        for (const std::unique_ptr<ImageViewWindow>& view :
             workspace->view_windows) {
            if (view != nullptr)
                viewers.push_back(&view->viewer);
        }
        if (viewers.empty())
            viewers.push_back(&active_view);
        return viewers;
    }

    bool open_directory_into_library(RendererState& renderer_state,
                                     ViewerState& viewer,
                                     ImageLibraryState& library,
                                     PlaceholderUiState& ui_state,
                                     MultiViewWorkspace* workspace,
                                     const std::string& directory_path)
    {
        std::vector<std::string> folder_paths;
        std::string error_message;
        if (!collect_directory_image_paths(directory_path, library.sort_mode,
                                           library.sort_reverse, folder_paths,
                                           error_message)) {
            viewer.last_error = error_message;
            viewer.status_message.clear();
            return false;
        }
        if (folder_paths.empty()) {
            viewer.last_error
                = Strutil::fmt::format("No supported image files found in '{}'",
                                       directory_path);
            viewer.status_message.clear();
            return false;
        }

        append_loaded_image_paths(library, folder_paths);
        const std::vector<ViewerState*> viewers
            = collect_action_target_viewers(workspace, viewer);
        sort_loaded_image_paths(library, viewers);
        if (workspace != nullptr)
            sync_workspace_library_state(*workspace, library);
        else
            sync_viewer_library_state(viewer, library);

        for (const std::string& candidate : folder_paths) {
            if (!set_current_loaded_image_path(library, viewer, candidate))
                continue;
            if (load_viewer_image(renderer_state, viewer, library, &ui_state,
                                  candidate, ui_state.subimage_index,
                                  ui_state.miplevel_index)) {
                viewer.status_message = Strutil::fmt::format(
                    "Opened folder {} ({} supported images)", directory_path,
                    folder_paths.size());
                return true;
            }
        }

        if (viewer.last_error.empty()) {
            viewer.last_error
                = Strutil::fmt::format("Failed to load any image from '{}'",
                                       directory_path);
        }
        return false;
    }

    std::string parent_directory_for_dialog(const std::string& path)
    {
        if (path.empty())
            return std::string();
        std::filesystem::path p(path);
        if (!p.has_parent_path())
            return std::string();
        return p.parent_path().string();
    }

    std::string open_dialog_default_path(const ViewerState& viewer,
                                         const ImageLibraryState& library)
    {
        if (!viewer.image.path.empty())
            return parent_directory_for_dialog(viewer.image.path);
        if (!library.recent_images.empty())
            return parent_directory_for_dialog(library.recent_images.front());
        return std::string();
    }

    std::string save_dialog_default_name(const ViewerState& viewer)
    {
        if (viewer.image.path.empty())
            return "image.exr";
        std::filesystem::path p(viewer.image.path);
        if (p.filename().empty())
            return "image.exr";
        return p.filename().string();
    }

    std::string save_selection_default_name(const ViewerState& viewer)
    {
        if (viewer.image.path.empty())
            return "selection.exr";
        std::filesystem::path p(viewer.image.path);
        const std::string stem = p.stem().empty() ? "selection"
                                                  : p.stem().string();
        const std::string ext  = p.extension().empty() ? ".exr"
                                                       : p.extension().string();
        return stem + "_selection" + ext;
    }

    std::string export_selection_default_name(const ViewerState& viewer)
    {
        if (viewer.image.path.empty())
            return "selection_export.exr";
        std::filesystem::path p(viewer.image.path);
        const std::string stem = p.stem().empty() ? "selection_export"
                                                  : p.stem().string();
        const std::string ext  = p.extension().empty() ? ".exr"
                                                       : p.extension().string();
        return stem + "_selection_export" + ext;
    }

    std::string save_window_default_name(const ViewerState& viewer)
    {
        if (viewer.image.path.empty())
            return "window.exr";
        std::filesystem::path p(viewer.image.path);
        const std::string stem = p.stem().empty() ? "window"
                                                  : p.stem().string();
        const std::string ext  = p.extension().empty() ? ".exr"
                                                       : p.extension().string();
        return stem + "_window" + ext;
    }

    struct SaveDialogSpec {
        const char* missing_image_error     = "No image loaded to save";
        const char* missing_selection_error = nullptr;
        const char* cancel_message          = "Save cancelled";
        const char* failure_prefix          = "save failed";
        std::string default_name;
    };

    SaveDialogSpec save_dialog_spec(SaveDialogKind kind,
                                    const ViewerState& viewer)
    {
        SaveDialogSpec spec;
        switch (kind) {
        case SaveDialogKind::SaveImage:
            spec.default_name = save_dialog_default_name(viewer);
            return spec;
        case SaveDialogKind::SaveWindow:
            spec.cancel_message = "Save window cancelled";
            spec.default_name   = save_window_default_name(viewer);
            return spec;
        case SaveDialogKind::SaveSelection:
            spec.missing_selection_error = "No selection to save";
            spec.cancel_message          = "Save selection cancelled";
            spec.default_name            = save_selection_default_name(viewer);
            return spec;
        case SaveDialogKind::ExportSelection:
            spec.missing_image_error     = "No image loaded to export";
            spec.missing_selection_error = "No selection to export";
            spec.cancel_message          = "Export selection cancelled";
            spec.failure_prefix          = "export failed";
            spec.default_name = export_selection_default_name(viewer);
            return spec;
        }
        spec.default_name = save_dialog_default_name(viewer);
        return spec;
    }

    bool run_save_dialog_operation(SaveDialogKind kind, ViewerState& viewer,
                                   const PlaceholderUiState* ui_state,
                                   const std::string& path,
                                   std::string& error_message)
    {
        switch (kind) {
        case SaveDialogKind::SaveImage:
            return save_loaded_image(viewer.image, path, error_message);
        case SaveDialogKind::SaveWindow:
            if (ui_state == nullptr) {
                error_message = "window save action is not configured";
                return false;
            }
            return save_window_image(viewer.image, viewer.recipe, *ui_state,
                                     path, error_message);
        case SaveDialogKind::SaveSelection:
            return save_selection_image(viewer.image, viewer, path,
                                        error_message);
        case SaveDialogKind::ExportSelection:
            if (ui_state == nullptr) {
                error_message = "selection export action is not configured";
                return false;
            }
            return save_export_selection_image(viewer.image, viewer, *ui_state,
                                               path, error_message);
        }
        error_message = "save action is not configured";
        return false;
    }

    void set_save_dialog_success_message(SaveDialogKind kind,
                                         ViewerState& viewer,
                                         const std::string& path)
    {
        switch (kind) {
        case SaveDialogKind::SaveImage:
            viewer.status_message = Strutil::fmt::format("Saved {}", path);
            break;
        case SaveDialogKind::SaveWindow:
            viewer.status_message = Strutil::fmt::format("Saved window {}",
                                                         path);
            break;
        case SaveDialogKind::SaveSelection: {
            const int width  = viewer.selection_xend - viewer.selection_xbegin;
            const int height = viewer.selection_yend - viewer.selection_ybegin;
            viewer.status_message
                = Strutil::fmt::format("Saved selection {} ({}x{})", path,
                                       width, height);
            break;
        }
        case SaveDialogKind::ExportSelection: {
            const int width  = viewer.selection_xend - viewer.selection_xbegin;
            const int height = viewer.selection_yend - viewer.selection_ybegin;
            viewer.status_message
                = Strutil::fmt::format("Exported selection {} ({}x{})", path,
                                       width, height);
            break;
        }
        }
        viewer.last_error.clear();
    }

    void run_save_dialog_action(SaveDialogKind kind, ViewerState& viewer,
                                const PlaceholderUiState* ui_state)
    {
        const SaveDialogSpec spec = save_dialog_spec(kind, viewer);
        if (viewer.image.path.empty()) {
            viewer.last_error = spec.missing_image_error;
            return;
        }
        if (spec.missing_selection_error != nullptr
            && !has_image_selection(viewer)) {
            viewer.last_error = spec.missing_selection_error;
            return;
        }

        const ImageLibraryState empty_library;
        const std::string default_path
            = open_dialog_default_path(viewer, empty_library);
        FileDialog::DialogReply reply
            = FileDialog::save_image_file(default_path, spec.default_name);
        if (reply.result == FileDialog::Result::Okay) {
            std::string error_message;
            if (run_save_dialog_operation(kind, viewer, ui_state, reply.path,
                                          error_message)) {
                set_save_dialog_success_message(kind, viewer, reply.path);
                return;
            }
            viewer.last_error = Strutil::fmt::format("{}: {}",
                                                     spec.failure_prefix,
                                                     error_message);
            return;
        }
        if (reply.result == FileDialog::Result::Cancel) {
            viewer.status_message = spec.cancel_message;
            viewer.last_error.clear();
            return;
        }
        viewer.last_error = reply.message.empty() ? "Save dialog failed"
                                                  : reply.message;
    }

}  // namespace

void
save_as_dialog_action(ViewerState& viewer)
{
    run_save_dialog_action(SaveDialogKind::SaveImage, viewer, nullptr);
}

void
save_window_as_dialog_action(ViewerState& viewer,
                             const PlaceholderUiState& ui_state)
{
    run_save_dialog_action(SaveDialogKind::SaveWindow, viewer, &ui_state);
}

void
save_selection_as_dialog_action(ViewerState& viewer)
{
    run_save_dialog_action(SaveDialogKind::SaveSelection, viewer, nullptr);
}

void
export_selection_as_dialog_action(ViewerState& viewer,
                                  const PlaceholderUiState& ui_state)
{
    run_save_dialog_action(SaveDialogKind::ExportSelection, viewer, &ui_state);
}

void
open_image_dialog_action(RendererState& renderer_state, ViewerState& viewer,
                         ImageLibraryState& library,
                         PlaceholderUiState& ui_state, int requested_subimage,
                         int requested_miplevel)
{
    FileDialog::DialogReply reply = FileDialog::open_image_files(
        open_dialog_default_path(viewer, library));
    if (reply.result == FileDialog::Result::Okay) {
        if (reply.paths.empty() && !reply.path.empty())
            reply.paths.push_back(reply.path);

        int first_added_index = -1;
        append_loaded_image_paths(library, reply.paths, &first_added_index);
        sync_viewer_library_state(viewer, library);

        std::string target_path;
        if (first_added_index >= 0
            && first_added_index
                   < static_cast<int>(library.loaded_image_paths.size())) {
            target_path = library.loaded_image_paths[static_cast<size_t>(
                first_added_index)];
        } else {
            for (const std::string& path : reply.paths) {
                if (!set_current_loaded_image_path(library, viewer, path))
                    continue;
                if (viewer.current_path_index < 0
                    || viewer.current_path_index >= static_cast<int>(
                           library.loaded_image_paths.size())) {
                    continue;
                }
                target_path = library.loaded_image_paths[static_cast<size_t>(
                    viewer.current_path_index)];
                break;
            }
        }

        if (!target_path.empty()) {
            load_viewer_image(renderer_state, viewer, library, &ui_state,
                              target_path, requested_subimage,
                              requested_miplevel);
        } else {
            viewer.last_error = "No selected image paths were accepted";
        }
    } else if (reply.result == FileDialog::Result::Cancel) {
        viewer.status_message = "Open cancelled";
        viewer.last_error.clear();
    } else {
        viewer.last_error = reply.message;
    }
}

void
open_folder_dialog_action(RendererState& renderer_state, ViewerState& viewer,
                          ImageLibraryState& library,
                          PlaceholderUiState& ui_state,
                          MultiViewWorkspace* workspace)
{
    FileDialog::DialogReply reply = FileDialog::open_folder(
        open_dialog_default_path(viewer, library));
    if (reply.result == FileDialog::Result::Okay) {
        if (!reply.path.empty()) {
            open_directory_into_library(renderer_state, viewer, library,
                                        ui_state, workspace, reply.path);
        } else {
            viewer.last_error = "No folder was selected";
            viewer.status_message.clear();
        }
    } else if (reply.result == FileDialog::Result::Cancel) {
        viewer.status_message = "Open folder cancelled";
        viewer.last_error.clear();
    } else {
        viewer.last_error = reply.message;
        viewer.status_message.clear();
    }
}

bool
capture_main_viewport_screenshot_action(RendererState& renderer_state,
                                        ViewerState& viewer,
                                        std::string& out_path)
{
    out_path.clear();
    viewer.last_error.clear();

    int width  = std::max(0, renderer_state.framebuffer_width);
    int height = std::max(0, renderer_state.framebuffer_height);
    if (width <= 0 || height <= 0) {
        viewer.last_error = "screenshot failed: main viewport size is invalid";
        return false;
    }

    const std::filesystem::path output_path = default_screenshot_output_path();
    std::error_code ec;
    std::filesystem::create_directories(output_path.parent_path(), ec);
    if (ec) {
        viewer.last_error = Strutil::fmt::format(
            "screenshot failed: could not create output directory '{}': {}",
            output_path.parent_path().string(), ec.message());
        return false;
    }

    std::vector<unsigned int> pixels(static_cast<size_t>(width)
                                     * static_cast<size_t>(height));
    if (!renderer_screen_capture(ImGui::GetMainViewport()->ID, 0, 0, width,
                                 height, pixels.data(), &renderer_state)) {
        viewer.last_error = "screenshot failed: framebuffer readback failed";
        return false;
    }

    ImageSpec spec(width, height, 4, TypeDesc::UINT8);
    ImageBuf output(spec);
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(
        pixels.data());
    if (!output.set_pixels(ROI::All(), TypeDesc::UINT8, bytes)) {
        viewer.last_error = output.geterror().empty()
                                ? "screenshot failed: could not populate image"
                                : Strutil::fmt::format("screenshot failed: {}",
                                                       output.geterror());
        return false;
    }
    if (!output.write(output_path.string())) {
        viewer.last_error = output.geterror().empty()
                                ? "screenshot failed: image write failed"
                                : Strutil::fmt::format("screenshot failed: {}",
                                                       output.geterror());
        return false;
    }

    out_path              = output_path.string();
    viewer.status_message = Strutil::fmt::format("Saved screenshot {}",
                                                 output_path.string());
    viewer.last_error.clear();
    return true;
}

}  // namespace Imiv
