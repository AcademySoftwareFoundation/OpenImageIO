// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_viewer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>

using namespace OIIO;

namespace Imiv {

constexpr const char* k_imiv_settings_filename     = "imiv.inf";
constexpr const char* k_imiv_legacy_prefs_filename = "imiv_prefs.ini";
constexpr const char* k_imiv_legacy_imgui_filename = "imiv.ini";
constexpr const char* k_imiv_app_section_name      = "[ImivApp][State]";
constexpr size_t k_max_recent_images               = 16;

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


bool
parse_bool_value(const std::string& value, bool& out)
{
    const string_view trimmed = Strutil::strip(value);
    if (trimmed.empty())
        return false;
    if (trimmed == "1" || Strutil::iequals(trimmed, "true")
        || Strutil::iequals(trimmed, "yes")
        || Strutil::iequals(trimmed, "on")) {
        out = true;
        return true;
    }
    if (trimmed == "0" || Strutil::iequals(trimmed, "false")
        || Strutil::iequals(trimmed, "no")
        || Strutil::iequals(trimmed, "off")) {
        out = false;
        return true;
    }
    return false;
}



bool
parse_int_value(const std::string& value, int& out)
{
    const std::string trimmed = std::string(Strutil::strip(value));
    if (trimmed.empty())
        return false;
    char* end = nullptr;
    long x    = std::strtol(trimmed.c_str(), &end, 10);
    if (end == trimmed.c_str() || *end != '\0')
        return false;
    if (x < std::numeric_limits<int>::min()
        || x > std::numeric_limits<int>::max())
        return false;
    out = static_cast<int>(x);
    return true;
}



bool
parse_float_value(const std::string& value, float& out)
{
    const std::string trimmed = std::string(Strutil::strip(value));
    if (trimmed.empty())
        return false;
    char* end = nullptr;
    float x   = std::strtof(trimmed.c_str(), &end);
    if (end == trimmed.c_str() || *end != '\0')
        return false;
    out = x;
    return true;
}

std::filesystem::path
legacy_prefs_file_path()
{
    return std::filesystem::path(k_imiv_legacy_prefs_filename);
}

std::filesystem::path
legacy_imgui_ini_file_path()
{
    return std::filesystem::path(k_imiv_legacy_imgui_filename);
}

std::filesystem::path
prefs_directory_path()
{
    const string_view override_dir = Sysutil::getenv("IMIV_CONFIG_HOME");
    if (!override_dir.empty())
        return std::filesystem::path(std::string(override_dir)) / "OpenImageIO"
               / "imiv";

#if defined(_WIN32)
    string_view base_dir = Sysutil::getenv("APPDATA");
    if (base_dir.empty())
        base_dir = Sysutil::getenv("LOCALAPPDATA");
    if (!base_dir.empty())
        return std::filesystem::path(std::string(base_dir)) / "OpenImageIO"
               / "imiv";
#elif defined(__APPLE__)
    const string_view home = Sysutil::getenv("HOME");
    if (!home.empty()) {
        return std::filesystem::path(std::string(home)) / "Library"
               / "Application Support" / "OpenImageIO" / "imiv";
    }
#else
    const string_view xdg_config_home = Sysutil::getenv("XDG_CONFIG_HOME");
    if (!xdg_config_home.empty()) {
        return std::filesystem::path(std::string(xdg_config_home))
               / "OpenImageIO" / "imiv";
    }
    const string_view home = Sysutil::getenv("HOME");
    if (!home.empty())
        return std::filesystem::path(std::string(home)) / ".config"
               / "OpenImageIO" / "imiv";
#endif

    return std::filesystem::path();
}

std::filesystem::path
config_dir_legacy_prefs_file_path()
{
    const std::filesystem::path prefs_dir = prefs_directory_path();
    if (prefs_dir.empty())
        return legacy_prefs_file_path();
    return prefs_dir / k_imiv_legacy_prefs_filename;
}

std::filesystem::path
persistent_state_file_path()
{
    const std::filesystem::path prefs_dir = prefs_directory_path();
    if (prefs_dir.empty())
        return std::filesystem::path(k_imiv_settings_filename);
    return prefs_dir / k_imiv_settings_filename;
}

std::filesystem::path
persistent_state_file_path_for_load()
{
    const std::filesystem::path prefs_path = persistent_state_file_path();
    std::error_code ec;
    if (std::filesystem::exists(prefs_path, ec))
        return prefs_path;
    const std::filesystem::path legacy_config_path
        = config_dir_legacy_prefs_file_path();
    ec.clear();
    if (std::filesystem::exists(legacy_config_path, ec))
        return legacy_config_path;
    return legacy_prefs_file_path();
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

    if (ui_state.max_memory_ic_mb < 64)
        ui_state.max_memory_ic_mb = 64;
    if (ui_state.slide_duration_seconds < 1)
        ui_state.slide_duration_seconds = 1;
    ui_state.closeup_pixels     = clamp_odd(ui_state.closeup_pixels, 9, 25);
    ui_state.closeup_avg_pixels = clamp_odd(ui_state.closeup_avg_pixels, 3, 25);
    if (ui_state.closeup_avg_pixels > ui_state.closeup_pixels)
        ui_state.closeup_avg_pixels = ui_state.closeup_pixels;
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
}



void
reset_per_image_preview_state(ViewRecipe& recipe)
{
    reset_view_recipe(recipe);
}



bool
load_persistent_state(PlaceholderUiState& ui_state, ViewerState& viewer,
                      ImageLibraryState& library, std::string& error_message)
{
    error_message.clear();
    const std::filesystem::path path = persistent_state_file_path_for_load();
    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
        return true;

    std::ifstream input(path);
    if (!input) {
        error_message = Strutil::fmt::format("failed to open '{}'",
                                             path.string());
        return false;
    }

    std::string line;
    bool in_app_section = false;
    bool saw_sections   = false;
    while (std::getline(input, line)) {
        const std::string trimmed = std::string(Strutil::strip(line));
        if (trimmed.empty() || trimmed[0] == '#')
            continue;
        if (trimmed.front() == '[') {
            saw_sections   = true;
            in_app_section = (trimmed == k_imiv_app_section_name);
            continue;
        }
        if (saw_sections && !in_app_section)
            continue;

        const size_t eq = trimmed.find('=');
        if (eq == std::string::npos)
            continue;

        const std::string key = std::string(
            Strutil::strip(trimmed.substr(0, eq)));
        const std::string value = trimmed.substr(eq + 1);
        bool bool_value         = false;
        int int_value           = 0;

        if (key == "pixelview_follows_mouse") {
            if (parse_bool_value(value, bool_value))
                ui_state.pixelview_follows_mouse = bool_value;
        } else if (key == "pixelview_left_corner") {
            if (parse_bool_value(value, bool_value))
                ui_state.pixelview_left_corner = bool_value;
        } else if (key == "linear_interpolation") {
            if (parse_bool_value(value, bool_value))
                viewer.recipe.linear_interpolation = bool_value;
        } else if (key == "dark_palette") {
            if (parse_bool_value(value, bool_value))
                ui_state.style_preset = static_cast<int>(
                    bool_value ? AppStylePreset::ImGuiDark
                               : AppStylePreset::ImGuiLight);
        } else if (key == "style_preset") {
            if (parse_int_value(value, int_value))
                ui_state.style_preset = int_value;
        } else if (key == "renderer_backend") {
            BackendKind backend_kind = BackendKind::Auto;
            if (parse_backend_kind(value, backend_kind)) {
                ui_state.renderer_backend = static_cast<int>(backend_kind);
            } else if (parse_int_value(value, int_value)) {
                ui_state.renderer_backend = int_value;
            }
        } else if (key == "auto_mipmap") {
            if (parse_bool_value(value, bool_value))
                ui_state.auto_mipmap = bool_value;
        } else if (key == "fit_image_to_window") {
            if (parse_bool_value(value, bool_value))
                ui_state.fit_image_to_window = bool_value;
        } else if (key == "show_mouse_mode_selector") {
            if (parse_bool_value(value, bool_value))
                ui_state.show_mouse_mode_selector = bool_value;
        } else if (key == "full_screen_mode") {
            if (parse_bool_value(value, bool_value))
                ui_state.full_screen_mode = bool_value;
        } else if (key == "window_always_on_top") {
            if (parse_bool_value(value, bool_value))
                ui_state.window_always_on_top = bool_value;
        } else if (key == "slide_show_running") {
            if (parse_bool_value(value, bool_value))
                ui_state.slide_show_running = bool_value;
        } else if (key == "slide_loop") {
            if (parse_bool_value(value, bool_value))
                ui_state.slide_loop = bool_value;
        } else if (key == "use_ocio") {
            if (parse_bool_value(value, bool_value))
                viewer.recipe.use_ocio = bool_value;
        } else if (key == "ocio_config_source") {
            if (parse_int_value(value, int_value))
                ui_state.ocio_config_source = int_value;
        } else if (key == "max_memory_ic_mb") {
            if (parse_int_value(value, int_value))
                ui_state.max_memory_ic_mb = int_value;
        } else if (key == "slide_duration_seconds") {
            if (parse_int_value(value, int_value))
                ui_state.slide_duration_seconds = int_value;
        } else if (key == "closeup_pixels") {
            if (parse_int_value(value, int_value))
                ui_state.closeup_pixels = int_value;
        } else if (key == "closeup_avg_pixels") {
            if (parse_int_value(value, int_value))
                ui_state.closeup_avg_pixels = int_value;
        } else if (key == "current_channel") {
            if (parse_int_value(value, int_value))
                viewer.recipe.current_channel = int_value;
        } else if (key == "color_mode") {
            if (parse_int_value(value, int_value))
                viewer.recipe.color_mode = int_value;
        } else if (key == "subimage_index") {
            if (parse_int_value(value, int_value))
                ui_state.subimage_index = int_value;
        } else if (key == "miplevel_index") {
            if (parse_int_value(value, int_value))
                ui_state.miplevel_index = int_value;
        } else if (key == "mouse_mode") {
            if (parse_int_value(value, int_value))
                ui_state.mouse_mode = int_value;
        } else if (key == "exposure") {
            float float_value = 0.0f;
            if (parse_float_value(value, float_value))
                viewer.recipe.exposure = float_value;
        } else if (key == "gamma") {
            float float_value = 0.0f;
            if (parse_float_value(value, float_value))
                viewer.recipe.gamma = float_value;
        } else if (key == "offset") {
            float float_value = 0.0f;
            if (parse_float_value(value, float_value))
                viewer.recipe.offset = float_value;
        } else if (key == "ocio_display") {
            viewer.recipe.ocio_display = std::string(Strutil::strip(value));
        } else if (key == "ocio_view") {
            viewer.recipe.ocio_view = std::string(Strutil::strip(value));
        } else if (key == "ocio_image_color_space") {
            viewer.recipe.ocio_image_color_space = std::string(
                Strutil::strip(value));
        } else if (key == "ocio_user_config_path") {
            ui_state.ocio_user_config_path = std::string(Strutil::strip(value));
        } else if (key == "sort_mode") {
            if (parse_int_value(value, int_value)) {
                int_value         = std::clamp(int_value, 0, 3);
                library.sort_mode = static_cast<ImageSortMode>(int_value);
            }
        } else if (key == "sort_reverse") {
            if (parse_bool_value(value, bool_value))
                library.sort_reverse = bool_value;
        } else if (key == "recent_image") {
            add_recent_image_path(library, std::string(Strutil::strip(value)));
        }
    }

    clamp_placeholder_ui_state(ui_state);
    clamp_view_recipe(viewer.recipe);
    apply_view_recipe_to_ui_state(viewer.recipe, ui_state);
    if (!input.eof()) {
        error_message = Strutil::fmt::format("failed while reading '{}'",
                                             path.string());
        return false;
    }
    return true;
}



bool
save_persistent_state(const PlaceholderUiState& ui_state,
                      const ViewerState& viewer,
                      const ImageLibraryState& library,
                      const char* imgui_ini_text, size_t imgui_ini_size,
                      std::string& error_message)
{
    error_message.clear();
    const std::filesystem::path path      = persistent_state_file_path();
    const std::filesystem::path temp_path = path.string() + ".tmp";
    std::error_code ec;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            error_message = Strutil::fmt::format("failed to create '{}': {}",
                                                 path.parent_path().string(),
                                                 ec.message());
            return false;
        }
    }

    std::ofstream output(temp_path, std::ios::trunc);
    if (!output) {
        error_message = Strutil::fmt::format("failed to open '{}'",
                                             temp_path.string());
        return false;
    }

    if (imgui_ini_text != nullptr && imgui_ini_size > 0) {
        output.write(imgui_ini_text,
                     static_cast<std::streamsize>(imgui_ini_size));
        if (imgui_ini_text[imgui_ini_size - 1] != '\n')
            output << "\n";
        output << "\n";
    }

    output << k_imiv_app_section_name << "\n";
    output << "pixelview_follows_mouse="
           << (ui_state.pixelview_follows_mouse ? 1 : 0) << "\n";
    output << "pixelview_left_corner="
           << (ui_state.pixelview_left_corner ? 1 : 0) << "\n";
    output << "linear_interpolation="
           << (viewer.recipe.linear_interpolation ? 1 : 0) << "\n";
    output << "style_preset=" << ui_state.style_preset << "\n";
    output << "renderer_backend="
           << backend_cli_name(sanitize_backend_kind(ui_state.renderer_backend))
           << "\n";
    output << "auto_mipmap=" << (ui_state.auto_mipmap ? 1 : 0) << "\n";
    output << "fit_image_to_window=" << (ui_state.fit_image_to_window ? 1 : 0)
           << "\n";
    output << "show_mouse_mode_selector="
           << (ui_state.show_mouse_mode_selector ? 1 : 0) << "\n";
    output << "full_screen_mode=" << (ui_state.full_screen_mode ? 1 : 0)
           << "\n";
    output << "window_always_on_top=" << (ui_state.window_always_on_top ? 1 : 0)
           << "\n";
    output << "slide_show_running=" << (ui_state.slide_show_running ? 1 : 0)
           << "\n";
    output << "slide_loop=" << (ui_state.slide_loop ? 1 : 0) << "\n";
    output << "use_ocio=" << (viewer.recipe.use_ocio ? 1 : 0) << "\n";
    output << "ocio_config_source=" << ui_state.ocio_config_source << "\n";
    output << "max_memory_ic_mb=" << ui_state.max_memory_ic_mb << "\n";
    output << "slide_duration_seconds=" << ui_state.slide_duration_seconds
           << "\n";
    output << "closeup_pixels=" << ui_state.closeup_pixels << "\n";
    output << "closeup_avg_pixels=" << ui_state.closeup_avg_pixels << "\n";
    output << "current_channel=" << viewer.recipe.current_channel << "\n";
    output << "color_mode=" << viewer.recipe.color_mode << "\n";
    output << "subimage_index=" << ui_state.subimage_index << "\n";
    output << "miplevel_index=" << ui_state.miplevel_index << "\n";
    output << "mouse_mode=" << ui_state.mouse_mode << "\n";
    output << "exposure=" << viewer.recipe.exposure << "\n";
    output << "gamma=" << viewer.recipe.gamma << "\n";
    output << "offset=" << viewer.recipe.offset << "\n";
    output << "ocio_display=" << viewer.recipe.ocio_display << "\n";
    output << "ocio_view=" << viewer.recipe.ocio_view << "\n";
    output << "ocio_image_color_space=" << viewer.recipe.ocio_image_color_space
           << "\n";
    output << "ocio_user_config_path=" << ui_state.ocio_user_config_path
           << "\n";
    output << "sort_mode=" << static_cast<int>(library.sort_mode) << "\n";
    output << "sort_reverse=" << (library.sort_reverse ? 1 : 0) << "\n";
    for (const std::string& recent : library.recent_images)
        output << "recent_image=" << recent << "\n";
    output.flush();
    if (!output) {
        error_message = Strutil::fmt::format("failed while writing '{}'",
                                             temp_path.string());
        output.close();
        std::error_code rm_ec;
        std::filesystem::remove(temp_path, rm_ec);
        return false;
    }
    output.close();

    ec.clear();
    std::filesystem::rename(temp_path, path, ec);
    if (ec) {
        std::error_code remove_ec;
        std::filesystem::remove(path, remove_ec);
        ec.clear();
        std::filesystem::rename(temp_path, path, ec);
    }
    if (ec) {
        error_message = Strutil::fmt::format("failed to replace '{}': {}",
                                             path.string(), ec.message());
        std::error_code rm_ec;
        std::filesystem::remove(temp_path, rm_ec);
        return false;
    }
    return true;
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



bool
has_supported_image_extension(const std::filesystem::path& path)
{
    std::string ext = path.extension().string();
    Strutil::to_lower(ext);
    if (ext.empty())
        return false;

    static const std::unordered_set<std::string> readable_extensions = []() {
        std::unordered_set<std::string> result;
        const auto extension_map = get_extension_map();
        const auto input_formats
            = Strutil::splits(get_string_attribute("input_format_list"), ",");
        std::unordered_set<std::string> readable_formats;
        readable_formats.reserve(input_formats.size());
        for (std::string format_name : input_formats) {
            Strutil::to_lower(format_name);
            if (!format_name.empty())
                readable_formats.insert(std::move(format_name));
        }

        for (const auto& entry : extension_map) {
            std::string format_name = entry.first;
            Strutil::to_lower(format_name);
            if (readable_formats.find(format_name) == readable_formats.end())
                continue;
            for (std::string one_ext : entry.second) {
                Strutil::to_lower(one_ext);
                if (one_ext.empty())
                    continue;
                if (one_ext.front() != '.')
                    one_ext.insert(one_ext.begin(), '.');
                result.insert(std::move(one_ext));
            }
        }
        return result;
    }();
    return readable_extensions.find(ext) != readable_extensions.end();
}



bool
datetime_to_time_t(string_view datetime, std::time_t& out_time)
{
    int year  = 0;
    int month = 0;
    int day   = 0;
    int hour  = 0;
    int min   = 0;
    int sec   = 0;
    if (!Strutil::scan_datetime(datetime, year, month, day, hour, min, sec))
        return false;

    std::tm tm_value = {};
    tm_value.tm_sec  = sec;
    tm_value.tm_min  = min;
    tm_value.tm_hour = hour;
    tm_value.tm_mday = day;
    tm_value.tm_mon  = month - 1;
    tm_value.tm_year = year - 1900;
    out_time         = std::mktime(&tm_value);
    return out_time != static_cast<std::time_t>(-1);
}



bool
file_last_write_time(const std::string& path, std::time_t& out_time)
{
    std::error_code ec;
    const auto file_time = std::filesystem::last_write_time(path, ec);
    if (ec)
        return false;
    const auto system_time
        = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            file_time - std::filesystem::file_time_type::clock::now()
            + std::chrono::system_clock::now());
    out_time = std::chrono::system_clock::to_time_t(system_time);
    return true;
}



bool
image_datetime(const std::string& path, std::time_t& out_time)
{
    auto input = ImageInput::open(path);
    if (!input)
        return false;
    const ImageSpec spec = input->spec();
    input->close();

    const std::string datetime = spec.get_string_attribute("DateTime");
    if (datetime.empty())
        return false;
    return datetime_to_time_t(datetime, out_time);
}



std::string
normalize_path_for_viewer_list(const std::string& path)
{
    if (path.empty())
        return std::string();
    std::filesystem::path p(path);
    std::error_code ec;
    if (!p.is_absolute()) {
        const std::filesystem::path abs = std::filesystem::absolute(p, ec);
        if (!ec)
            p = abs;
    }
    return p.lexically_normal().string();
}



int
find_path_index(const std::vector<std::string>& paths, const std::string& path)
{
    if (path.empty())
        return -1;
    const auto it = std::find(paths.begin(), paths.end(), path);
    if (it == paths.end())
        return -1;
    return static_cast<int>(std::distance(paths.begin(), it));
}



std::string
filename_key(const std::string& path)
{
    return std::filesystem::path(path).filename().string();
}



std::string
path_key(const std::string& path)
{
    return std::filesystem::path(path).lexically_normal().string();
}



void
sort_image_path_list(std::vector<std::string>& paths, ImageSortMode sort_mode,
                     bool sort_reverse)
{
    if (paths.empty())
        return;

    switch (sort_mode) {
    case ImageSortMode::ByName:
        std::sort(paths.begin(), paths.end(),
                  [&](const std::string& a, const std::string& b) {
                      const std::string a_name = filename_key(a);
                      const std::string b_name = filename_key(b);
                      if (a_name == b_name)
                          return path_key(a) < path_key(b);
                      return a_name < b_name;
                  });
        break;
    case ImageSortMode::ByPath:
        std::sort(paths.begin(), paths.end(),
                  [&](const std::string& a, const std::string& b) {
                      return path_key(a) < path_key(b);
                  });
        break;
    case ImageSortMode::ByImageDate:
        std::sort(paths.begin(), paths.end(),
                  [&](const std::string& a, const std::string& b) {
                      std::time_t a_time = {};
                      std::time_t b_time = {};
                      const bool a_ok    = image_datetime(a, a_time)
                                        || file_last_write_time(a, a_time);
                      const bool b_ok = image_datetime(b, b_time)
                                        || file_last_write_time(b, b_time);
                      if (a_ok != b_ok)
                          return a_ok;
                      if (!a_ok && !b_ok)
                          return filename_key(a) < filename_key(b);
                      if (a_time == b_time)
                          return filename_key(a) < filename_key(b);
                      return a_time < b_time;
                  });
        break;
    case ImageSortMode::ByFileDate:
        std::sort(paths.begin(), paths.end(),
                  [&](const std::string& a, const std::string& b) {
                      std::time_t a_time = {};
                      std::time_t b_time = {};
                      const bool a_ok    = file_last_write_time(a, a_time);
                      const bool b_ok    = file_last_write_time(b, b_time);
                      if (a_ok != b_ok)
                          return a_ok;
                      if (!a_ok && !b_ok)
                          return filename_key(a) < filename_key(b);
                      if (a_time == b_time)
                          return filename_key(a) < filename_key(b);
                      return a_time < b_time;
                  });
        break;
    }

    if (sort_reverse)
        std::reverse(paths.begin(), paths.end());
}



bool
collect_directory_image_paths(const std::string& directory_path,
                              ImageSortMode sort_mode, bool sort_reverse,
                              std::vector<std::string>& out_paths,
                              std::string& error_message)
{
    out_paths.clear();
    error_message.clear();

    std::error_code ec;
    std::filesystem::path dir(directory_path);
    if (dir.empty()) {
        error_message = "directory path is empty";
        return false;
    }
    if (!std::filesystem::exists(dir, ec) || ec) {
        error_message = Strutil::fmt::format("directory '{}' does not exist",
                                             dir.string());
        return false;
    }
    if (!std::filesystem::is_directory(dir, ec) || ec) {
        error_message = Strutil::fmt::format("'{}' is not a directory",
                                             dir.string());
        return false;
    }

    std::filesystem::directory_iterator it(
        dir, std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) {
        error_message = Strutil::fmt::format("failed to scan '{}': {}",
                                             dir.string(), ec.message());
        return false;
    }

    for (; it != std::filesystem::directory_iterator(); it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        const std::filesystem::directory_entry& entry = *it;
        if (!entry.is_regular_file(ec)) {
            ec.clear();
            continue;
        }
        if (!has_supported_image_extension(entry.path()))
            continue;
        out_paths.push_back(
            normalize_path_for_viewer_list(entry.path().string()));
    }

    sort_image_path_list(out_paths, sort_mode, sort_reverse);
    return true;
}



bool
add_loaded_image_path(ImageLibraryState& library, const std::string& path,
                      int* out_index)
{
    if (out_index != nullptr)
        *out_index = -1;
    const std::string normalized = normalize_path_for_viewer_list(path);
    if (normalized.empty())
        return false;

    int index = find_path_index(library.loaded_image_paths, normalized);
    if (index < 0) {
        library.loaded_image_paths.push_back(normalized);
        index = static_cast<int>(library.loaded_image_paths.size()) - 1;
    }
    if (out_index != nullptr)
        *out_index = index;
    return index >= 0;
}



bool
append_loaded_image_paths(ImageLibraryState& library,
                          const std::vector<std::string>& paths,
                          int* out_first_added_index)
{
    if (out_first_added_index != nullptr)
        *out_first_added_index = -1;

    bool added_any = false;
    std::string first_added_path;
    for (const std::string& path : paths) {
        const std::string normalized = normalize_path_for_viewer_list(path);
        if (normalized.empty())
            continue;
        if (find_path_index(library.loaded_image_paths, normalized) >= 0)
            continue;
        library.loaded_image_paths.push_back(normalized);
        if (first_added_path.empty())
            first_added_path = normalized;
        added_any = true;
    }

    if (!added_any)
        return false;

    if (out_first_added_index != nullptr && !first_added_path.empty())
        *out_first_added_index = find_path_index(library.loaded_image_paths,
                                                 first_added_path);
    return true;
}



bool
remove_loaded_image_path(ImageLibraryState& library, ViewerState* viewer,
                         const std::string& path)
{
    const std::string normalized = normalize_path_for_viewer_list(path);
    const int remove_index       = find_path_index(library.loaded_image_paths,
                                                   normalized);
    if (remove_index < 0)
        return false;

    library.loaded_image_paths.erase(library.loaded_image_paths.begin()
                                     + remove_index);
    if (viewer != nullptr) {
        if (viewer->current_path_index == remove_index) {
            viewer->current_path_index = -1;
        } else if (viewer->current_path_index > remove_index) {
            --viewer->current_path_index;
        }

        if (viewer->last_path_index == remove_index) {
            viewer->last_path_index = -1;
        } else if (viewer->last_path_index > remove_index) {
            --viewer->last_path_index;
        }
    }
    return true;
}



bool
set_current_loaded_image_path(const ImageLibraryState& library,
                              ViewerState& viewer, const std::string& path)
{
    const std::string normalized = normalize_path_for_viewer_list(path);
    const int new_index          = find_path_index(library.loaded_image_paths,
                                                   normalized);
    if (new_index < 0)
        return false;
    if (viewer.current_path_index >= 0
        && viewer.current_path_index != new_index)
        viewer.last_path_index = viewer.current_path_index;
    viewer.current_path_index = new_index;
    return true;
}



bool
pick_loaded_image_path(const ImageLibraryState& library,
                       const ViewerState& viewer, int delta,
                       std::string& out_path)
{
    out_path.clear();
    if (library.loaded_image_paths.empty() || viewer.current_path_index < 0)
        return false;
    const int count = static_cast<int>(library.loaded_image_paths.size());
    int index       = viewer.current_path_index + delta;
    while (index < 0)
        index += count;
    index %= count;
    out_path = library.loaded_image_paths[static_cast<size_t>(index)];
    return !out_path.empty();
}



void
sort_loaded_image_paths(ImageLibraryState& library,
                        const std::vector<ViewerState*>& viewers)
{
    std::vector<std::string> current_paths;
    std::vector<std::string> last_paths;
    current_paths.reserve(viewers.size());
    last_paths.reserve(viewers.size());
    for (const ViewerState* viewer : viewers) {
        current_paths.emplace_back(viewer != nullptr ? viewer->image.path
                                                     : std::string());
        const std::string last_path
            = (viewer != nullptr && viewer->last_path_index >= 0
               && viewer->last_path_index
                      < static_cast<int>(library.loaded_image_paths.size()))
                  ? library.loaded_image_paths[static_cast<size_t>(
                        viewer->last_path_index)]
                  : std::string();
        last_paths.emplace_back(last_path);
    }

    sort_image_path_list(library.loaded_image_paths, library.sort_mode,
                         library.sort_reverse);
    for (size_t i = 0, e = viewers.size(); i < e; ++i) {
        ViewerState* viewer = viewers[i];
        if (viewer == nullptr)
            continue;
        viewer->current_path_index
            = find_path_index(library.loaded_image_paths,
                              normalize_path_for_viewer_list(current_paths[i]));
        viewer->last_path_index
            = find_path_index(library.loaded_image_paths,
                              normalize_path_for_viewer_list(last_paths[i]));
    }
}



void
add_recent_image_path(ImageLibraryState& library, const std::string& path)
{
    const std::string normalized = normalize_path_for_viewer_list(path);
    if (normalized.empty())
        return;

    auto it = std::remove(library.recent_images.begin(),
                          library.recent_images.end(), normalized);
    library.recent_images.erase(it, library.recent_images.end());
    library.recent_images.insert(library.recent_images.begin(), normalized);
    if (library.recent_images.size() > k_max_recent_images)
        library.recent_images.resize(k_max_recent_images);
}



namespace {

    void copy_viewer_library_state(ViewerState& dst, const ViewerState& src,
                                   const ImageLibraryState& library)
    {
        const std::string current_path = dst.image.path;
        const std::string last_path
            = (dst.last_path_index >= 0
               && dst.last_path_index
                      < static_cast<int>(src.loaded_image_paths.size()))
                  ? src.loaded_image_paths[static_cast<size_t>(
                        dst.last_path_index)]
                  : std::string();
        dst.loaded_image_paths = library.loaded_image_paths;
        dst.recent_images      = library.recent_images;
        dst.sort_mode          = library.sort_mode;
        dst.sort_reverse       = library.sort_reverse;
        dst.current_path_index
            = find_path_index(dst.loaded_image_paths,
                              normalize_path_for_viewer_list(current_path));
        dst.last_path_index
            = find_path_index(dst.loaded_image_paths,
                              normalize_path_for_viewer_list(last_path));
    }

}  // namespace



ImageViewWindow&
ensure_primary_image_view(MultiViewWorkspace& workspace)
{
    if (workspace.view_windows.empty()) {
        std::unique_ptr<ImageViewWindow> view(new ImageViewWindow());
        view->id                 = workspace.next_view_id++;
        workspace.active_view_id = view->id;
        workspace.view_windows.push_back(std::move(view));
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



ImageViewWindow&
append_image_view(MultiViewWorkspace& workspace)
{
    std::unique_ptr<ImageViewWindow> view(new ImageViewWindow());
    view->id = workspace.next_view_id++;
    workspace.view_windows.push_back(std::move(view));
    return *workspace.view_windows.back();
}



void
sync_workspace_library_state(MultiViewWorkspace& workspace,
                             const ViewerState& source_view,
                             const ImageLibraryState& library)
{
    for (const std::unique_ptr<ImageViewWindow>& view :
         workspace.view_windows) {
        if (view == nullptr)
            continue;
        copy_viewer_library_state(view->viewer, source_view, library);
    }
}



void
erase_closed_image_views(MultiViewWorkspace& workspace)
{
    if (workspace.view_windows.size() <= 1)
        return;

    workspace.view_windows.erase(
        std::remove_if(
            workspace.view_windows.begin(), workspace.view_windows.end(),
            [&](const std::unique_ptr<ImageViewWindow>& view) {
                return view != nullptr && !view->open
                       && view.get() != workspace.view_windows.front().get();
            }),
        workspace.view_windows.end());

    if (find_image_view(workspace, workspace.active_view_id) == nullptr) {
        const ImageViewWindow& primary = ensure_primary_image_view(workspace);
        workspace.active_view_id       = primary.id;
    }
}



}  // namespace Imiv
