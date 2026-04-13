// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_viewer.h"

#include "imiv_image_library.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>

using namespace OIIO;

namespace Imiv {

void
reset_view_navigation_state(ViewerState& viewer)
{
    viewer.scroll                            = ImVec2(0.0f, 0.0f);
    viewer.norm_scroll                       = ImVec2(0.5f, 0.5f);
    viewer.max_scroll                        = ImVec2(0.0f, 0.0f);
    viewer.zoom_pivot_screen                 = ImVec2(0.0f, 0.0f);
    viewer.zoom_pivot_source_uv              = ImVec2(0.5f, 0.5f);
    viewer.zoom_pivot_pending                = false;
    viewer.zoom_pivot_frames_left            = 0;
    viewer.scroll_sync_frames_left           = 2;
    viewer.auto_subimage                     = false;
    viewer.pending_auto_subimage             = -1;
    viewer.pending_auto_subimage_zoom        = 1.0f;
    viewer.pending_auto_subimage_norm_scroll = ImVec2(0.5f, 0.5f);
    viewer.selection_active                  = false;
    viewer.selection_xbegin                  = 0;
    viewer.selection_ybegin                  = 0;
    viewer.selection_xend                    = 0;
    viewer.selection_yend                    = 0;
    viewer.selection_press_active            = false;
    viewer.selection_drag_active             = false;
    viewer.selection_drag_start_uv           = ImVec2(0.0f, 0.0f);
    viewer.selection_drag_end_uv             = ImVec2(0.0f, 0.0f);
    viewer.selection_drag_start_screen       = ImVec2(0.0f, 0.0f);
    viewer.pan_drag_active                   = false;
    viewer.zoom_drag_active                  = false;
    viewer.drag_prev_mouse                   = ImVec2(0.0f, 0.0f);
}

bool
has_image_selection(const ViewerState& viewer)
{
    return viewer.selection_active
           && viewer.selection_xend > viewer.selection_xbegin
           && viewer.selection_yend > viewer.selection_ybegin;
}

void
clear_image_selection(ViewerState& viewer)
{
    viewer.selection_active            = false;
    viewer.selection_xbegin            = 0;
    viewer.selection_ybegin            = 0;
    viewer.selection_xend              = 0;
    viewer.selection_yend              = 0;
    viewer.selection_press_active      = false;
    viewer.selection_drag_active       = false;
    viewer.selection_drag_start_uv     = ImVec2(0.0f, 0.0f);
    viewer.selection_drag_end_uv       = ImVec2(0.0f, 0.0f);
    viewer.selection_drag_start_screen = ImVec2(0.0f, 0.0f);
}

void
set_image_selection(ViewerState& viewer, int xbegin, int ybegin, int xend,
                    int yend)
{
    if (viewer.image.width <= 0 || viewer.image.height <= 0) {
        clear_image_selection(viewer);
        return;
    }

    const int xmin = std::clamp(std::min(xbegin, xend), 0, viewer.image.width);
    const int xmax = std::clamp(std::max(xbegin, xend), 0, viewer.image.width);
    const int ymin = std::clamp(std::min(ybegin, yend), 0, viewer.image.height);
    const int ymax = std::clamp(std::max(ybegin, yend), 0, viewer.image.height);
    if (xmax <= xmin || ymax <= ymin) {
        clear_image_selection(viewer);
        return;
    }

    viewer.selection_active = true;
    viewer.selection_xbegin = xmin;
    viewer.selection_ybegin = ymin;
    viewer.selection_xend   = xmax;
    viewer.selection_yend   = ymax;
}

void
clamp_view_recipe(ViewRecipe& recipe)
{
    if (recipe.current_channel < 0)
        recipe.current_channel = 0;
    if (recipe.current_channel > 4)
        recipe.current_channel = 4;
    if (recipe.color_mode < 0)
        recipe.color_mode = 0;
    if (recipe.color_mode > 4)
        recipe.color_mode = 4;
    if (recipe.gamma < 0.1f)
        recipe.gamma = 0.1f;
    recipe.offset = std::clamp(recipe.offset, -1.0f, 1.0f);
    if (recipe.ocio_display.empty())
        recipe.ocio_display = "default";
    if (recipe.ocio_view.empty())
        recipe.ocio_view = "default";
    if (recipe.ocio_image_color_space.empty())
        recipe.ocio_image_color_space = "auto";
}



void
reset_view_recipe(ViewRecipe& recipe)
{
    recipe.current_channel = 0;
    recipe.color_mode      = 0;
    recipe.exposure        = 0.0f;
    recipe.gamma           = 1.0f;
    recipe.offset          = 0.0f;
}



void
apply_view_recipe_to_ui_state(const ViewRecipe& recipe,
                              PlaceholderUiState& ui_state)
{
    ui_state.use_ocio               = recipe.use_ocio;
    ui_state.linear_interpolation   = recipe.linear_interpolation;
    ui_state.current_channel        = recipe.current_channel;
    ui_state.color_mode             = recipe.color_mode;
    ui_state.exposure               = recipe.exposure;
    ui_state.gamma                  = recipe.gamma;
    ui_state.offset                 = recipe.offset;
    ui_state.ocio_display           = recipe.ocio_display;
    ui_state.ocio_view              = recipe.ocio_view;
    ui_state.ocio_image_color_space = recipe.ocio_image_color_space;
}



void
capture_view_recipe_from_ui_state(const PlaceholderUiState& ui_state,
                                  ViewRecipe& recipe)
{
    recipe.use_ocio               = ui_state.use_ocio;
    recipe.linear_interpolation   = ui_state.linear_interpolation;
    recipe.current_channel        = ui_state.current_channel;
    recipe.color_mode             = ui_state.color_mode;
    recipe.exposure               = ui_state.exposure;
    recipe.gamma                  = ui_state.gamma;
    recipe.offset                 = ui_state.offset;
    recipe.ocio_display           = ui_state.ocio_display;
    recipe.ocio_view              = ui_state.ocio_view;
    recipe.ocio_image_color_space = ui_state.ocio_image_color_space;
}



void
clamp_placeholder_ui_state(PlaceholderUiState& ui_state)
{
    auto clamp_odd = [](int value, int min_value, int max_value) {
        int clamped = std::clamp(value, min_value, max_value);
        if ((clamped & 1) == 0) {
            if (clamped < max_value)
                ++clamped;
            else
                --clamped;
        }
        return std::clamp(clamped, min_value, max_value);
    };
    auto clamp_color = [](ImVec4& color) {
        color.x = std::clamp(color.x, 0.0f, 1.0f);
        color.y = std::clamp(color.y, 0.0f, 1.0f);
        color.z = std::clamp(color.z, 0.0f, 1.0f);
        color.w = std::clamp(color.w, 0.0f, 1.0f);
    };

    if (ui_state.max_memory_ic_mb < 64)
        ui_state.max_memory_ic_mb = 64;
    if (ui_state.slide_duration_seconds < 1)
        ui_state.slide_duration_seconds = 1;
    ui_state.closeup_pixels     = clamp_odd(ui_state.closeup_pixels, 9, 25);
    ui_state.closeup_avg_pixels = clamp_odd(ui_state.closeup_avg_pixels, 3, 25);
    if (ui_state.closeup_avg_pixels > ui_state.closeup_pixels)
        ui_state.closeup_avg_pixels = ui_state.closeup_pixels;
    ui_state.transparency_check_size
        = std::clamp(ui_state.transparency_check_size, 4, 128);
    if (ui_state.mouse_mode < 0)
        ui_state.mouse_mode = 0;
    if (ui_state.mouse_mode > 4)
        ui_state.mouse_mode = 4;
    ui_state.style_preset = static_cast<int>(
        sanitize_app_style_preset(ui_state.style_preset));
    ui_state.renderer_backend = static_cast<int>(
        sanitize_backend_kind(ui_state.renderer_backend));
    if (ui_state.subimage_index < 0)
        ui_state.subimage_index = 0;
    if (ui_state.miplevel_index < 0)
        ui_state.miplevel_index = 0;
    ui_state.ocio_config_source
        = std::clamp(ui_state.ocio_config_source,
                     static_cast<int>(OcioConfigSource::Global),
                     static_cast<int>(OcioConfigSource::User));
    clamp_color(ui_state.transparency_light_color);
    clamp_color(ui_state.transparency_dark_color);
    clamp_color(ui_state.image_window_bg_color);
}



void
reset_per_image_preview_state(ViewRecipe& recipe)
{
    reset_view_recipe(recipe);
}



void
append_longinfo_row(LoadedImage& image, const char* label,
                    const std::string& value)
{
    if (label == nullptr || label[0] == '\0')
        return;
    image.longinfo_rows.emplace_back(label, value);
}

std::string
extract_image_color_space_metadata(const ImageSpec& spec)
{
    static constexpr const char* candidates[] = {
        "oiio:ColorSpace",
        "ColorSpace",
        "colorspace",
    };
    for (const char* attr_name : candidates) {
        const std::string value = std::string(
            Strutil::strip(spec.get_string_attribute(attr_name)));
        if (!value.empty())
            return value;
    }
    return std::string();
}

void
build_longinfo_rows(LoadedImage& image, const ImageBuf& source,
                    const ImageSpec& spec)
{
    image.longinfo_rows.clear();
    if (spec.depth <= 1) {
        append_longinfo_row(image, "Dimensions",
                            Strutil::fmt::format("{} x {} pixels", spec.width,
                                                 spec.height));
    } else {
        append_longinfo_row(image, "Dimensions",
                            Strutil::fmt::format("{} x {} x {} pixels",
                                                 spec.width, spec.height,
                                                 spec.depth));
    }
    append_longinfo_row(image, "Channels",
                        Strutil::fmt::format("{}", spec.nchannels));

    std::string channel_list;
    for (int i = 0; i < spec.nchannels; ++i) {
        if (i > 0)
            channel_list += ", ";
        channel_list += spec.channelnames[i];
    }
    append_longinfo_row(image, "Channel list", channel_list);
    append_longinfo_row(image, "File format",
                        std::string(source.file_format_name()));
    append_longinfo_row(image, "Data format", std::string(spec.format.c_str()));
    append_longinfo_row(image, "Data size",
                        Strutil::fmt::format(
                            "{:.2f} MB", static_cast<double>(spec.image_bytes())
                                             / (1024.0 * 1024.0)));
    append_longinfo_row(image, "Image origin",
                        Strutil::fmt::format("{}, {}, {}", spec.x, spec.y,
                                             spec.z));
    append_longinfo_row(image, "Full/display size",
                        Strutil::fmt::format("{} x {} x {}", spec.full_width,
                                             spec.full_height,
                                             spec.full_depth));
    append_longinfo_row(image, "Full/display origin",
                        Strutil::fmt::format("{}, {}, {}", spec.full_x,
                                             spec.full_y, spec.full_z));
    if (spec.tile_width) {
        append_longinfo_row(image, "Scanline/tile",
                            Strutil::fmt::format("tiled {} x {} x {}",
                                                 spec.tile_width,
                                                 spec.tile_height,
                                                 spec.tile_depth));
    } else {
        append_longinfo_row(image, "Scanline/tile", "scanline");
    }
    if (spec.alpha_channel >= 0) {
        append_longinfo_row(image, "Alpha channel",
                            Strutil::fmt::format("{}", spec.alpha_channel));
    }
    if (spec.z_channel >= 0) {
        append_longinfo_row(image, "Depth (z) channel",
                            Strutil::fmt::format("{}", spec.z_channel));
    }

    ParamValueList attribs = spec.extra_attribs;
    attribs.sort(false);
    for (auto&& p : attribs) {
        append_longinfo_row(image, p.name().c_str(),
                            spec.metadata_val(p, true));
    }
}



bool
load_image_for_compute(const std::string& path, int requested_subimage,
                       int requested_miplevel, bool rawcolor,
                       LoadedImage& image, std::string& error_message)
{
    ImageSpec input_config;
    const ImageSpec* input_config_ptr = nullptr;
    if (rawcolor) {
        input_config.attribute("oiio:RawColor", 1);
        input_config_ptr = &input_config;
    }

    std::shared_ptr<ImageCache> imagecache = ImageCache::create(true);
    ImageBuf source(path, 0, 0, imagecache, input_config_ptr);
    if (!source.read(0, 0, true, TypeUnknown)) {
        error_message = source.geterror();
        if (error_message.empty())
            error_message = "failed to read image";
        return false;
    }

    int nsubimages = source.nsubimages();
    if (nsubimages <= 0)
        nsubimages = 1;
    const int resolved_subimage = std::clamp(requested_subimage, 0,
                                             nsubimages - 1);
    if (resolved_subimage != 0) {
        source.reset(path, resolved_subimage, 0, imagecache, input_config_ptr);
        if (!source.read(resolved_subimage, 0, true, TypeUnknown)) {
            error_message = source.geterror();
            if (error_message.empty())
                error_message = "failed to read subimage";
            return false;
        }
    }

    int nmiplevels = source.nmiplevels();
    if (nmiplevels <= 0)
        nmiplevels = 1;
    const int resolved_miplevel = std::clamp(requested_miplevel, 0,
                                             nmiplevels - 1);
    if (resolved_miplevel != source.miplevel()) {
        source.reset(path, resolved_subimage, resolved_miplevel, imagecache,
                     input_config_ptr);
        if (!source.read(resolved_subimage, resolved_miplevel, true,
                         TypeUnknown)) {
            error_message = source.geterror();
            if (error_message.empty())
                error_message = "failed to read miplevel";
            return false;
        }
    }

    const ImageSpec& spec = source.spec();
    if (spec.width <= 0 || spec.height <= 0 || spec.nchannels <= 0) {
        error_message = "image has invalid dimensions or channel count";
        return false;
    }

    UploadDataType upload_type = UploadDataType::Unknown;
    TypeDesc read_format       = TypeUnknown;
    if (!map_spec_type_to_upload(spec.format, upload_type, read_format)) {
        upload_type = UploadDataType::Float;
        read_format = TypeFloat;
    }

    const size_t width         = static_cast<size_t>(spec.width);
    const size_t height        = static_cast<size_t>(spec.height);
    const size_t channel_count = static_cast<size_t>(spec.nchannels);
    const size_t channel_bytes = upload_data_type_size(upload_type);
    if (channel_bytes == 0) {
        error_message = "unsupported pixel data type";
        return false;
    }
    const size_t row_pitch  = width * channel_count * channel_bytes;
    const size_t total_size = row_pitch * height;

    std::vector<unsigned char> pixels(total_size);
    if (!source.get_pixels(source.roi(), read_format, pixels.data())) {
        error_message = source.geterror();
        if (error_message.empty())
            error_message = "failed to fetch pixel data";
        return false;
    }

    image.path                 = path;
    image.metadata_color_space = extract_image_color_space_metadata(spec);
    image.data_format_name     = std::string(spec.format.c_str());
    image.width                = spec.width;
    image.height               = spec.height;
    image.orientation          = clamp_orientation(
        spec.get_int_attribute("Orientation", 1));
    image.nchannels       = spec.nchannels;
    image.subimage        = resolved_subimage;
    image.miplevel        = resolved_miplevel;
    image.nsubimages      = nsubimages;
    image.nmiplevels      = nmiplevels;
    image.type            = upload_type;
    image.channel_bytes   = channel_bytes;
    image.row_pitch_bytes = row_pitch;
    image.pixels          = std::move(pixels);
    image.channel_names.clear();
    image.channel_names.reserve(static_cast<size_t>(spec.nchannels));
    for (int c = 0; c < spec.nchannels; ++c)
        image.channel_names.emplace_back(spec.channelnames[c]);
    build_longinfo_rows(image, source, spec);

    if (spec.format != read_format) {
        print(
            stderr,
            "imiv: source format '{}' converted to '{}' for compute upload path\n",
            spec.format.c_str(), read_format.c_str());
    }
    return true;
}



bool
should_reset_preview_on_load(const ViewerState& viewer, const std::string& path)
{
    if (path.empty())
        return false;
    if (viewer.image.path.empty())
        return true;
    const std::filesystem::path current_path(viewer.image.path);
    const std::filesystem::path next_path(path);
    return current_path.lexically_normal() != next_path.lexically_normal();
}



int
clamp_orientation(int orientation)
{
    return std::clamp(orientation, 1, 8);
}



bool
orientation_swaps_axes(int orientation)
{
    orientation = clamp_orientation(orientation);
    return orientation == 5 || orientation == 6 || orientation == 7
           || orientation == 8;
}



void
oriented_image_dimensions(const LoadedImage& image, int& out_width,
                          int& out_height)
{
    if (orientation_swaps_axes(image.orientation)) {
        out_width  = image.height;
        out_height = image.width;
    } else {
        out_width  = image.width;
        out_height = image.height;
    }
}

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



namespace {

}  // namespace
}  // namespace Imiv
