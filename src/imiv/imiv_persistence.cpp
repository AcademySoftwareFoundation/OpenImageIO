// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_persistence.h"

#include "imiv_image_library.h"
#include "imiv_parse.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>

using namespace OIIO;

namespace Imiv {
namespace {

    constexpr const char* k_imiv_settings_filename     = "imiv.inf";
    constexpr const char* k_imiv_legacy_prefs_filename = "imiv_prefs.ini";
    constexpr const char* k_imiv_legacy_imgui_filename = "imiv.ini";
    constexpr const char* k_imiv_app_section_name      = "[ImivApp][State]";

    std::filesystem::path prefs_directory_path()
    {
        const std::string_view override_dir = Sysutil::getenv(
            "IMIV_CONFIG_HOME");
        if (!override_dir.empty()) {
            return std::filesystem::path(std::string(override_dir))
                   / "OpenImageIO" / "imiv";
        }

#if defined(_WIN32)
        std::string_view base_dir = Sysutil::getenv("APPDATA");
        if (base_dir.empty())
            base_dir = Sysutil::getenv("LOCALAPPDATA");
        if (!base_dir.empty()) {
            return std::filesystem::path(std::string(base_dir)) / "OpenImageIO"
                   / "imiv";
        }
#elif defined(__APPLE__)
        const std::string_view home = Sysutil::getenv("HOME");
        if (!home.empty()) {
            return std::filesystem::path(std::string(home)) / "Library"
                   / "Application Support" / "OpenImageIO" / "imiv";
        }
#else
        const std::string_view xdg_config_home = Sysutil::getenv(
            "XDG_CONFIG_HOME");
        if (!xdg_config_home.empty()) {
            return std::filesystem::path(std::string(xdg_config_home))
                   / "OpenImageIO" / "imiv";
        }
        const std::string_view home = Sysutil::getenv("HOME");
        if (!home.empty()) {
            return std::filesystem::path(std::string(home)) / ".config"
                   / "OpenImageIO" / "imiv";
        }
#endif

        return std::filesystem::path();
    }

    std::filesystem::path config_dir_legacy_prefs_file_path()
    {
        const std::filesystem::path prefs_dir = prefs_directory_path();
        if (prefs_dir.empty())
            return std::filesystem::path(k_imiv_legacy_prefs_filename);
        return prefs_dir / k_imiv_legacy_prefs_filename;
    }

    std::filesystem::path persistent_state_file_path()
    {
        const std::filesystem::path prefs_dir = prefs_directory_path();
        if (prefs_dir.empty())
            return std::filesystem::path(k_imiv_settings_filename);
        return prefs_dir / k_imiv_settings_filename;
    }

    std::filesystem::path persistent_state_file_path_for_load()
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
        return std::filesystem::path(k_imiv_legacy_prefs_filename);
    }

    std::string strip_to_string(std::string_view value)
    {
        return std::string(Strutil::strip(value));
    }

    void apply_bool_pref(std::string_view value, bool& out_value)
    {
        bool parsed = false;
        if (parse_bool_string(value, parsed))
            out_value = parsed;
    }

    void apply_int_pref(std::string_view value, int& out_value)
    {
        int parsed = 0;
        if (parse_int_string(value, parsed))
            out_value = parsed;
    }

    void apply_float_pref(std::string_view value, float& out_value)
    {
        float parsed = 0.0f;
        if (parse_float_string(value, parsed))
            out_value = parsed;
    }

    void apply_persistent_state_entry(const std::string& key,
                                      const std::string& value,
                                      PlaceholderUiState& ui_state,
                                      ViewerState& viewer,
                                      ImageLibraryState& library)
    {
        if (key == "pixelview_follows_mouse") {
            apply_bool_pref(value, ui_state.pixelview_follows_mouse);
        } else if (key == "pixelview_left_corner") {
            apply_bool_pref(value, ui_state.pixelview_left_corner);
        } else if (key == "linear_interpolation") {
            apply_bool_pref(value, viewer.recipe.linear_interpolation);
        } else if (key == "dark_palette") {
            bool legacy_dark_palette = false;
            if (parse_bool_string(value, legacy_dark_palette)) {
                ui_state.style_preset = static_cast<int>(
                    legacy_dark_palette ? AppStylePreset::ImGuiDark
                                        : AppStylePreset::ImGuiLight);
            }
        } else if (key == "style_preset") {
            apply_int_pref(value, ui_state.style_preset);
        } else if (key == "renderer_backend") {
            BackendKind backend_kind = BackendKind::Auto;
            if (parse_backend_kind(value, backend_kind)) {
                ui_state.renderer_backend = static_cast<int>(backend_kind);
            } else {
                apply_int_pref(value, ui_state.renderer_backend);
            }
        } else if (key == "display_format") {
            DisplayFormatPreference display_format
                = DisplayFormatPreference::Auto;
            if (parse_display_format_preference(value, display_format)) {
                ui_state.display_format = static_cast<int>(display_format);
            } else {
                apply_int_pref(value, ui_state.display_format);
            }
        } else if (key == "auto_mipmap") {
            apply_bool_pref(value, ui_state.auto_mipmap);
        } else if (key == "fit_image_to_window") {
            apply_bool_pref(value, ui_state.fit_image_to_window);
        } else if (key == "show_transparency") {
            apply_bool_pref(value, ui_state.show_transparency);
        } else if (key == "image_window_bg_override") {
            apply_bool_pref(value, ui_state.image_window_bg_override);
        } else if (key == "show_mouse_mode_selector") {
            apply_bool_pref(value, ui_state.show_mouse_mode_selector);
        } else if (key == "full_screen_mode") {
            apply_bool_pref(value, ui_state.full_screen_mode);
        } else if (key == "window_always_on_top") {
            apply_bool_pref(value, ui_state.window_always_on_top);
        } else if (key == "slide_show_running") {
            apply_bool_pref(value, ui_state.slide_show_running);
        } else if (key == "slide_loop") {
            apply_bool_pref(value, ui_state.slide_loop);
        } else if (key == "use_ocio") {
            apply_bool_pref(value, viewer.recipe.use_ocio);
        } else if (key == "ocio_config_source") {
            apply_int_pref(value, ui_state.ocio_config_source);
        } else if (key == "max_memory_ic_mb") {
            apply_int_pref(value, ui_state.max_memory_ic_mb);
        } else if (key == "slide_duration_seconds") {
            apply_int_pref(value, ui_state.slide_duration_seconds);
        } else if (key == "closeup_pixels") {
            apply_int_pref(value, ui_state.closeup_pixels);
        } else if (key == "closeup_avg_pixels") {
            apply_int_pref(value, ui_state.closeup_avg_pixels);
        } else if (key == "transparency_check_size") {
            apply_int_pref(value, ui_state.transparency_check_size);
        } else if (key == "current_channel") {
            apply_int_pref(value, viewer.recipe.current_channel);
        } else if (key == "color_mode") {
            apply_int_pref(value, viewer.recipe.color_mode);
        } else if (key == "subimage_index") {
            apply_int_pref(value, ui_state.subimage_index);
        } else if (key == "miplevel_index") {
            apply_int_pref(value, ui_state.miplevel_index);
        } else if (key == "mouse_mode") {
            apply_int_pref(value, ui_state.mouse_mode);
        } else if (key == "exposure") {
            apply_float_pref(value, viewer.recipe.exposure);
        } else if (key == "gamma") {
            apply_float_pref(value, viewer.recipe.gamma);
        } else if (key == "offset") {
            apply_float_pref(value, viewer.recipe.offset);
        } else if (key == "transparency_light_r") {
            apply_float_pref(value, ui_state.transparency_light_color.x);
        } else if (key == "transparency_light_g") {
            apply_float_pref(value, ui_state.transparency_light_color.y);
        } else if (key == "transparency_light_b") {
            apply_float_pref(value, ui_state.transparency_light_color.z);
        } else if (key == "transparency_light_a") {
            apply_float_pref(value, ui_state.transparency_light_color.w);
        } else if (key == "transparency_dark_r") {
            apply_float_pref(value, ui_state.transparency_dark_color.x);
        } else if (key == "transparency_dark_g") {
            apply_float_pref(value, ui_state.transparency_dark_color.y);
        } else if (key == "transparency_dark_b") {
            apply_float_pref(value, ui_state.transparency_dark_color.z);
        } else if (key == "transparency_dark_a") {
            apply_float_pref(value, ui_state.transparency_dark_color.w);
        } else if (key == "image_window_bg_r") {
            apply_float_pref(value, ui_state.image_window_bg_color.x);
        } else if (key == "image_window_bg_g") {
            apply_float_pref(value, ui_state.image_window_bg_color.y);
        } else if (key == "image_window_bg_b") {
            apply_float_pref(value, ui_state.image_window_bg_color.z);
        } else if (key == "image_window_bg_a") {
            apply_float_pref(value, ui_state.image_window_bg_color.w);
        } else if (key == "ocio_display") {
            viewer.recipe.ocio_display = strip_to_string(value);
        } else if (key == "ocio_view") {
            viewer.recipe.ocio_view = strip_to_string(value);
        } else if (key == "ocio_image_color_space") {
            viewer.recipe.ocio_image_color_space = strip_to_string(value);
        } else if (key == "ocio_user_config_path") {
            ui_state.ocio_user_config_path = strip_to_string(value);
        } else if (key == "sort_mode") {
            int parsed_sort_mode = static_cast<int>(library.sort_mode);
            if (parse_int_string(value, parsed_sort_mode)) {
                parsed_sort_mode  = std::clamp(parsed_sort_mode, 0, 3);
                library.sort_mode = static_cast<ImageSortMode>(
                    parsed_sort_mode);
            }
        } else if (key == "sort_reverse") {
            apply_bool_pref(value, library.sort_reverse);
        } else if (key == "recent_image") {
            add_recent_image_path(library, strip_to_string(value));
        }
    }

    void write_persistent_state_entries(std::ofstream& output,
                                        const PlaceholderUiState& ui_state,
                                        const ViewerState& viewer,
                                        const ImageLibraryState& library)
    {
        output << k_imiv_app_section_name << "\n";
        output << "pixelview_follows_mouse="
               << (ui_state.pixelview_follows_mouse ? 1 : 0) << "\n";
        output << "pixelview_left_corner="
               << (ui_state.pixelview_left_corner ? 1 : 0) << "\n";
        output << "linear_interpolation="
               << (viewer.recipe.linear_interpolation ? 1 : 0) << "\n";
        output << "style_preset=" << ui_state.style_preset << "\n";
        output << "renderer_backend="
               << backend_cli_name(
                      sanitize_backend_kind(ui_state.renderer_backend))
               << "\n";
        output << "display_format="
               << display_format_cli_name(sanitize_display_format_preference(
                      ui_state.display_format))
               << "\n";
        output << "auto_mipmap=" << (ui_state.auto_mipmap ? 1 : 0) << "\n";
        output << "fit_image_to_window="
               << (ui_state.fit_image_to_window ? 1 : 0) << "\n";
        output << "show_transparency=" << (ui_state.show_transparency ? 1 : 0)
               << "\n";
        output << "image_window_bg_override="
               << (ui_state.image_window_bg_override ? 1 : 0) << "\n";
        output << "show_mouse_mode_selector="
               << (ui_state.show_mouse_mode_selector ? 1 : 0) << "\n";
        output << "full_screen_mode=" << (ui_state.full_screen_mode ? 1 : 0)
               << "\n";
        output << "window_always_on_top="
               << (ui_state.window_always_on_top ? 1 : 0) << "\n";
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
        output << "transparency_check_size=" << ui_state.transparency_check_size
               << "\n";
        output << "current_channel=" << viewer.recipe.current_channel << "\n";
        output << "color_mode=" << viewer.recipe.color_mode << "\n";
        output << "subimage_index=" << ui_state.subimage_index << "\n";
        output << "miplevel_index=" << ui_state.miplevel_index << "\n";
        output << "mouse_mode=" << ui_state.mouse_mode << "\n";
        output << "exposure=" << viewer.recipe.exposure << "\n";
        output << "gamma=" << viewer.recipe.gamma << "\n";
        output << "offset=" << viewer.recipe.offset << "\n";
        output << "transparency_light_r=" << ui_state.transparency_light_color.x
               << "\n";
        output << "transparency_light_g=" << ui_state.transparency_light_color.y
               << "\n";
        output << "transparency_light_b=" << ui_state.transparency_light_color.z
               << "\n";
        output << "transparency_light_a=" << ui_state.transparency_light_color.w
               << "\n";
        output << "transparency_dark_r=" << ui_state.transparency_dark_color.x
               << "\n";
        output << "transparency_dark_g=" << ui_state.transparency_dark_color.y
               << "\n";
        output << "transparency_dark_b=" << ui_state.transparency_dark_color.z
               << "\n";
        output << "transparency_dark_a=" << ui_state.transparency_dark_color.w
               << "\n";
        output << "image_window_bg_r=" << ui_state.image_window_bg_color.x
               << "\n";
        output << "image_window_bg_g=" << ui_state.image_window_bg_color.y
               << "\n";
        output << "image_window_bg_b=" << ui_state.image_window_bg_color.z
               << "\n";
        output << "image_window_bg_a=" << ui_state.image_window_bg_color.w
               << "\n";
        output << "ocio_display=" << viewer.recipe.ocio_display << "\n";
        output << "ocio_view=" << viewer.recipe.ocio_view << "\n";
        output << "ocio_image_color_space="
               << viewer.recipe.ocio_image_color_space << "\n";
        output << "ocio_user_config_path=" << ui_state.ocio_user_config_path
               << "\n";
        output << "sort_mode=" << static_cast<int>(library.sort_mode) << "\n";
        output << "sort_reverse=" << (library.sort_reverse ? 1 : 0) << "\n";
        for (const std::string& recent : library.recent_images)
            output << "recent_image=" << recent << "\n";
    }

}  // namespace

std::filesystem::path
imgui_ini_load_path()
{
    const std::filesystem::path settings_load_path
        = persistent_state_file_path_for_load();
    std::error_code ec;
    if (settings_load_path.filename() == k_imiv_settings_filename
        && std::filesystem::exists(settings_load_path, ec) && !ec) {
        return settings_load_path;
    }

    const std::filesystem::path legacy_imgui_path = std::filesystem::path(
        k_imiv_legacy_imgui_filename);
    ec.clear();
    if (std::filesystem::exists(legacy_imgui_path, ec) && !ec)
        return legacy_imgui_path;
    return std::filesystem::path();
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
        const std::string trimmed = strip_to_string(line);
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

        const std::string key   = strip_to_string(trimmed.substr(0, eq));
        const std::string value = trimmed.substr(eq + 1);
        apply_persistent_state_entry(key, value, ui_state, viewer, library);
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

    write_persistent_state_entries(output, ui_state, viewer, library);
    output.flush();
    if (!output) {
        error_message = Strutil::fmt::format("failed while writing '{}'",
                                             temp_path.string());
        output.close();
        std::error_code remove_ec;
        std::filesystem::remove(temp_path, remove_ec);
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
        std::error_code remove_ec;
        std::filesystem::remove(temp_path, remove_ec);
        return false;
    }
    return true;
}

}  // namespace Imiv
