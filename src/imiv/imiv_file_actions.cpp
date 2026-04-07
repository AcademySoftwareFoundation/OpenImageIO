// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_file_actions.h"

#include "imiv_actions.h"
#include "imiv_file_dialog.h"
#include "imiv_image_library.h"
#include "imiv_loaded_image.h"
#include "imiv_ocio.h"
#include "imiv_workspace.h"

#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <imgui.h>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>
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

    bool save_loaded_image(const LoadedImage& image, const std::string& path,
                           std::string& error_message);
    bool save_window_image(const LoadedImage& image, const ViewRecipe& recipe,
                           const PlaceholderUiState& ui_state,
                           const std::string& path, std::string& error_message);
    bool save_selection_image(const LoadedImage& image,
                              const ViewerState& viewer,
                              const std::string& path,
                              std::string& error_message);
    bool save_export_selection_image(const LoadedImage& image,
                                     const ViewerState& viewer,
                                     const PlaceholderUiState& ui_state,
                                     const std::string& path,
                                     std::string& error_message);

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

    bool build_selection_source_image(const LoadedImage& image,
                                      const ViewerState& viewer,
                                      ImageBuf& result,
                                      std::string& error_message)
    {
        error_message.clear();
        if (!has_image_selection(viewer)) {
            error_message = "no active selection";
            return false;
        }

        ImageBuf source;
        if (!imagebuf_from_loaded_image(image, source, error_message))
            return false;

        const ROI selection_roi(viewer.selection_xbegin, viewer.selection_xend,
                                viewer.selection_ybegin, viewer.selection_yend,
                                0, 1, 0, image.nchannels);
        result = ImageBufAlgo::cut(source, selection_roi);
        if (result.has_error()) {
            error_message = result.geterror();
            if (error_message.empty())
                error_message = "failed to crop selected image region";
            return false;
        }

        result.specmod().attribute("Orientation", image.orientation);
        return true;
    }

    bool save_selection_image(const LoadedImage& image,
                              const ViewerState& viewer,
                              const std::string& path,
                              std::string& error_message)
    {
        error_message.clear();
        if (path.empty()) {
            error_message = "save path is empty";
            return false;
        }

        ImageBuf result;
        if (!build_selection_source_image(image, viewer, result,
                                          error_message)) {
            return false;
        }

        const int orientation = result.spec().get_int_attribute("Orientation",
                                                                1);
        if (orientation != 1) {
            ImageBuf oriented = ImageBufAlgo::reorient(result);
            if (oriented.has_error()) {
                error_message = oriented.geterror();
                if (error_message.empty())
                    error_message = "failed to orient selection export";
                return false;
            }
            result = std::move(oriented);
        }

        result.specmod().attribute("Orientation", 1);
        if (!result.write(path, result.spec().format)) {
            error_message = result.geterror();
            if (error_message.empty())
                error_message = "image write failed";
            return false;
        }
        return true;
    }

    float selected_channel(float r, float g, float b, float a, int channel)
    {
        if (channel == 1)
            return r;
        if (channel == 2)
            return g;
        if (channel == 3)
            return b;
        if (channel == 4)
            return a;
        return r;
    }

    void apply_heatmap(float value, float& out_r, float& out_g, float& out_b)
    {
        const float t = std::clamp(value, 0.0f, 1.0f);
        if (t < 0.33f) {
            const float u = t / 0.33f;
            out_r         = 0.0f;
            out_g         = 0.9f * u;
            out_b         = 0.5f + (1.0f - 0.5f) * u;
            return;
        }
        if (t < 0.66f) {
            const float u = (t - 0.33f) / 0.33f;
            out_r         = u;
            out_g         = 0.9f + (1.0f - 0.9f) * u;
            out_b         = 1.0f - u;
            return;
        }
        const float u = (t - 0.66f) / 0.34f;
        out_r         = 1.0f;
        out_g         = 1.0f - u;
        out_b         = 0.0f;
    }

    void apply_window_recipe_to_rgba(std::vector<float>& rgba_pixels,
                                     const ViewRecipe& recipe,
                                     bool exposure_gamma_already_applied)
    {
        const size_t pixel_count = rgba_pixels.size() / 4;
        for (size_t pixel = 0; pixel < pixel_count; ++pixel) {
            float& r = rgba_pixels[pixel * 4 + 0];
            float& g = rgba_pixels[pixel * 4 + 1];
            float& b = rgba_pixels[pixel * 4 + 2];
            float& a = rgba_pixels[pixel * 4 + 3];

            r += recipe.offset;
            g += recipe.offset;
            b += recipe.offset;

            if (recipe.color_mode == 1) {
                a = 1.0f;
            } else if (recipe.color_mode == 2) {
                const float v = selected_channel(r, g, b, a,
                                                 recipe.current_channel);
                r             = v;
                g             = v;
                b             = v;
                a             = 1.0f;
            } else if (recipe.color_mode == 3) {
                const float y = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                r             = y;
                g             = y;
                b             = y;
                a             = 1.0f;
            } else if (recipe.color_mode == 4) {
                const float v = selected_channel(r, g, b, a,
                                                 recipe.current_channel);
                apply_heatmap(v, r, g, b);
                a = 1.0f;
            }

            if (recipe.current_channel > 0 && recipe.color_mode != 2
                && recipe.color_mode != 4) {
                const float v = selected_channel(r, g, b, a,
                                                 recipe.current_channel);
                r             = v;
                g             = v;
                b             = v;
                a             = 1.0f;
            }

            if (!exposure_gamma_already_applied) {
                const float exposure_scale = std::exp2(recipe.exposure);
                r = std::pow(std::max(r * exposure_scale, 0.0f),
                             1.0f / std::max(recipe.gamma, 0.01f));
                g = std::pow(std::max(g * exposure_scale, 0.0f),
                             1.0f / std::max(recipe.gamma, 0.01f));
                b = std::pow(std::max(b * exposure_scale, 0.0f),
                             1.0f / std::max(recipe.gamma, 0.01f));
            }
        }
    }

    bool build_view_export_rgba_image(const ImageBuf& source_image,
                                      const LoadedImage& image_metadata,
                                      const ViewRecipe& recipe,
                                      const PlaceholderUiState& ui_state,
                                      ImageBuf& output,
                                      std::string& error_message)
    {
        error_message.clear();
        ImageBuf oriented = source_image;
        const int orientation
            = source_image.spec().get_int_attribute("Orientation", 1);
        if (orientation != 1) {
            oriented = ImageBufAlgo::reorient(source_image);
            if (oriented.has_error()) {
                error_message = oriented.geterror();
                if (error_message.empty())
                    error_message = "failed to orient export image";
                return false;
            }
        }

        const int width     = oriented.spec().width;
        const int height    = oriented.spec().height;
        const int nchannels = oriented.nchannels();
        if (width <= 0 || height <= 0 || nchannels <= 0) {
            error_message = "window export source image is invalid";
            return false;
        }

        std::vector<float> src_pixels(static_cast<size_t>(width)
                                      * static_cast<size_t>(height)
                                      * static_cast<size_t>(nchannels));
        if (!oriented.get_pixels(ROI::All(), TypeFloat, src_pixels.data())) {
            error_message = oriented.geterror();
            if (error_message.empty())
                error_message = "failed to read source pixels for window export";
            return false;
        }

        std::vector<float> rgba_pixels(static_cast<size_t>(width)
                                           * static_cast<size_t>(height) * 4u,
                                       0.0f);
        const size_t pixel_count = static_cast<size_t>(width)
                                   * static_cast<size_t>(height);
        for (size_t pixel = 0; pixel < pixel_count; ++pixel) {
            const float* src
                = &src_pixels[pixel * static_cast<size_t>(nchannels)];
            float& r = rgba_pixels[pixel * 4 + 0];
            float& g = rgba_pixels[pixel * 4 + 1];
            float& b = rgba_pixels[pixel * 4 + 2];
            float& a = rgba_pixels[pixel * 4 + 3];
            if (nchannels == 1) {
                r = src[0];
                g = src[0];
                b = src[0];
                a = 1.0f;
            } else if (nchannels == 2) {
                r = src[0];
                g = src[0];
                b = src[0];
                a = src[1];
            } else {
                r = src[0];
                g = src[1];
                b = src[2];
                a = nchannels >= 4 ? src[3] : 1.0f;
            }
        }

        bool ocio_applied = false;
        if (recipe.use_ocio) {
            PlaceholderUiState export_ui_state = ui_state;
            apply_view_recipe_to_ui_state(recipe, export_ui_state);
            OCIO::ConstProcessorRcPtr processor;
            std::string resolved_display;
            std::string resolved_view;
            if (!build_ocio_cpu_display_processor(
                    export_ui_state, &image_metadata, recipe.exposure,
                    recipe.gamma, processor, resolved_display, resolved_view,
                    error_message)) {
                return false;
            }
            if (!processor) {
                error_message = "OCIO window export processor is unavailable";
                return false;
            }
            try {
                OCIO::ConstCPUProcessorRcPtr cpu_processor
                    = processor->getDefaultCPUProcessor();
                if (!cpu_processor) {
                    error_message
                        = "OCIO CPU processor is unavailable for window export";
                    return false;
                }
                OCIO::PackedImageDesc desc(rgba_pixels.data(), width, height,
                                           4);
                cpu_processor->apply(desc);
            } catch (const OCIO::Exception& e) {
                error_message = e.what();
                return false;
            }
            ocio_applied = true;
        }

        apply_window_recipe_to_rgba(rgba_pixels, recipe, ocio_applied);

        ImageSpec spec(width, height, 4, TypeFloat);
        spec.attribute("Orientation", 1);
        spec.channelnames = { "R", "G", "B", "A" };
        output.reset(spec);
        if (!output.set_pixels(ROI::All(), TypeFloat, rgba_pixels.data())) {
            error_message = output.geterror();
            if (error_message.empty())
                error_message = "failed to store window export pixels";
            return false;
        }
        return true;
    }

    bool build_window_export_rgba_image(const LoadedImage& image,
                                        const ViewRecipe& recipe,
                                        const PlaceholderUiState& ui_state,
                                        ImageBuf& output,
                                        std::string& error_message)
    {
        error_message.clear();

        ImageBuf source;
        if (!imagebuf_from_loaded_image(image, source, error_message))
            return false;

        return build_view_export_rgba_image(source, image, recipe, ui_state,
                                            output, error_message);
    }

    bool save_window_image(const LoadedImage& image, const ViewRecipe& recipe,
                           const PlaceholderUiState& ui_state,
                           const std::string& path, std::string& error_message)
    {
        error_message.clear();
        if (path.empty()) {
            error_message = "save path is empty";
            return false;
        }

        ImageBuf output;
        if (!build_window_export_rgba_image(image, recipe, ui_state, output,
                                            error_message)) {
            return false;
        }

        if (!output.write(path, TypeFloat)) {
            error_message = output.geterror();
            if (error_message.empty())
                error_message = "image write failed";
            return false;
        }
        return true;
    }

    bool save_export_selection_image(const LoadedImage& image,
                                     const ViewerState& viewer,
                                     const PlaceholderUiState& ui_state,
                                     const std::string& path,
                                     std::string& error_message)
    {
        error_message.clear();
        if (path.empty()) {
            error_message = "save path is empty";
            return false;
        }

        ImageBuf selection;
        if (!build_selection_source_image(image, viewer, selection,
                                          error_message)) {
            return false;
        }

        ImageBuf output;
        if (!build_view_export_rgba_image(selection, image, viewer.recipe,
                                          ui_state, output, error_message)) {
            return false;
        }

        if (!output.write(path, TypeFloat)) {
            error_message = output.geterror();
            if (error_message.empty())
                error_message = "image write failed";
            return false;
        }
        return true;
    }

    bool save_loaded_image(const LoadedImage& image, const std::string& path,
                           std::string& error_message)
    {
        error_message.clear();
        if (path.empty()) {
            error_message = "save path is empty";
            return false;
        }
        if (image.width <= 0 || image.height <= 0 || image.nchannels <= 0) {
            error_message = "no valid image is loaded";
            return false;
        }

        const TypeDesc format = upload_data_type_to_typedesc(image.type);
        if (format == TypeUnknown) {
            error_message = "unsupported source pixel type for save";
            return false;
        }

        const size_t width         = static_cast<size_t>(image.width);
        const size_t height        = static_cast<size_t>(image.height);
        const size_t channels      = static_cast<size_t>(image.nchannels);
        const size_t min_row_pitch = width * channels * image.channel_bytes;
        if (image.row_pitch_bytes < min_row_pitch) {
            error_message = "image row pitch is invalid";
            return false;
        }
        const size_t required_bytes = image.row_pitch_bytes * height;
        if (image.pixels.size() < required_bytes) {
            error_message = "image pixel buffer is incomplete";
            return false;
        }

        ImageSpec spec(image.width, image.height, image.nchannels, format);
        ImageBuf output(spec);

        const std::byte* begin = reinterpret_cast<const std::byte*>(
            image.pixels.data());
        const cspan<std::byte> byte_span(begin, image.pixels.size());
        const stride_t xstride = static_cast<stride_t>(image.nchannels
                                                       * image.channel_bytes);
        const stride_t ystride = static_cast<stride_t>(image.row_pitch_bytes);
        if (!output.set_pixels(ROI::All(), format, byte_span, begin, xstride,
                               ystride, AutoStride)) {
            error_message = output.geterror();
            if (error_message.empty())
                error_message = "failed to copy pixels into save buffer";
            return false;
        }

        if (!output.write(path, format)) {
            error_message = output.geterror();
            if (error_message.empty())
                error_message = "image write failed";
            return false;
        }
        return true;
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
