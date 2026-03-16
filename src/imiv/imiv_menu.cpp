// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_menu.h"

#include "imiv_actions.h"
#include "imiv_frame.h"
#include "imiv_ocio.h"
#include "imiv_ui.h"

#include <algorithm>
#include <filesystem>

#include <imgui.h>

#include <OpenImageIO/strutil.h>

using namespace OIIO;

namespace Imiv {

namespace {

    bool app_shortcut(ImGuiKeyChord key_chord)
    {
        return ImGui::Shortcut(key_chord,
                               ImGuiInputFlags_RouteGlobal
                                   | ImGuiInputFlags_RouteUnlessBgFocused);
    }

}  // namespace

void
collect_viewer_shortcuts(ViewerState& viewer, PlaceholderUiState& ui_state,
                         DeveloperUiState& developer_ui,
                         ViewerFrameActions& actions, bool& request_exit)
{
    const bool has_image         = !viewer.image.path.empty();
    const bool has_selection     = has_image_selection(viewer);
    const bool can_prev_subimage = has_image
                                   && (viewer.image.miplevel > 0
                                       || viewer.image.subimage > 0
                                       || viewer.image.nsubimages > 1);
    const bool can_next_subimage
        = has_image
          && (viewer.auto_subimage
              || viewer.image.miplevel + 1 < viewer.image.nmiplevels
              || viewer.image.subimage + 1 < viewer.image.nsubimages);

    const ImGuiIO& global_io = ImGui::GetIO();
    const bool no_mods       = !global_io.KeyCtrl && !global_io.KeyAlt
                         && !global_io.KeySuper;

    if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_O))
        actions.open_requested = true;
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_R) && has_image)
        actions.reload_requested = true;
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_W) && has_image)
        actions.close_requested = true;
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_S) && has_image)
        actions.save_as_requested = true;
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_A) && has_image)
        actions.select_all_requested = true;
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_D) && has_selection)
        actions.deselect_selection_requested = true;
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_Comma))
        ui_state.show_preferences_window = true;
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_Q))
        request_exit = true;
#if !defined(NDEBUG)
    if (app_shortcut(ImGuiKey_F12) && !developer_ui.screenshot_busy)
        developer_ui.request_screenshot = true;
#endif
    if (app_shortcut(ImGuiKey_PageUp))
        actions.prev_requested = true;
    if (app_shortcut(ImGuiKey_PageDown))
        actions.next_requested = true;
    if (no_mods && app_shortcut(ImGuiKey_T))
        actions.toggle_requested = true;
    if ((app_shortcut(ImGuiMod_Ctrl | ImGuiKey_Equal)
         || app_shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Equal)
         || app_shortcut(ImGuiMod_Ctrl | ImGuiKey_KeypadAdd))
        && has_image) {
        request_zoom_scale(actions.pending_zoom, 2.0f, false);
    }
    if ((app_shortcut(ImGuiMod_Ctrl | ImGuiKey_Minus)
         || app_shortcut(ImGuiMod_Ctrl | ImGuiKey_KeypadSubtract))
        && has_image) {
        request_zoom_scale(actions.pending_zoom, 0.5f, false);
    }
    if ((app_shortcut(ImGuiMod_Ctrl | ImGuiKey_0)
         || app_shortcut(ImGuiMod_Ctrl | ImGuiKey_Keypad0))
        && has_image) {
        request_zoom_reset(actions.pending_zoom, false);
    }
    if ((app_shortcut(ImGuiMod_Ctrl | ImGuiKey_Period)
         || app_shortcut(ImGuiMod_Ctrl | ImGuiKey_KeypadDecimal))
        && has_image) {
        actions.recenter_requested = true;
    }
    if (no_mods && app_shortcut(ImGuiKey_F) && has_image)
        actions.fit_window_to_image_requested = true;
    if (app_shortcut(ImGuiMod_Alt | ImGuiKey_F) && has_image) {
        ui_state.fit_image_to_window = !ui_state.fit_image_to_window;
        viewer.fit_request           = true;
    }
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_F))
        actions.full_screen_toggle_requested = true;
    if (ui_state.full_screen_mode && app_shortcut(ImGuiKey_Escape))
        actions.full_screen_toggle_requested = true;
    if (app_shortcut(ImGuiMod_Shift | ImGuiKey_Comma) && can_prev_subimage)
        actions.prev_subimage_requested = true;
    if (app_shortcut(ImGuiMod_Shift | ImGuiKey_Period) && can_next_subimage)
        actions.next_subimage_requested = true;
    if (no_mods && app_shortcut(ImGuiKey_C))
        ui_state.current_channel = 0;
    if (no_mods && app_shortcut(ImGuiKey_R))
        ui_state.current_channel = 1;
    if (no_mods && app_shortcut(ImGuiKey_G))
        ui_state.current_channel = 2;
    if (no_mods && app_shortcut(ImGuiKey_B))
        ui_state.current_channel = 3;
    if (no_mods && app_shortcut(ImGuiKey_A))
        ui_state.current_channel = 4;
    if (no_mods && app_shortcut(ImGuiKey_Comma) && has_image)
        ui_state.current_channel = std::max(0, ui_state.current_channel - 1);
    if (no_mods && app_shortcut(ImGuiKey_Period) && has_image)
        ui_state.current_channel = std::min(4, ui_state.current_channel + 1);
    if (no_mods && app_shortcut(ImGuiKey_1))
        ui_state.color_mode = 2;
    if (no_mods && app_shortcut(ImGuiKey_L))
        ui_state.color_mode = 3;
    if (no_mods && app_shortcut(ImGuiKey_H))
        ui_state.color_mode = 4;
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_I))
        ui_state.show_info_window = !ui_state.show_info_window;
    if (no_mods && app_shortcut(ImGuiKey_P))
        ui_state.show_pixelview_window = !ui_state.show_pixelview_window;
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_A))
        set_area_sample_enabled(viewer, ui_state,
                                !ui_state.show_area_probe_window);
    if (app_shortcut(ImGuiMod_Shift | ImGuiKey_LeftBracket))
        ui_state.exposure -= 0.5f;
    if (app_shortcut(ImGuiKey_LeftBracket))
        ui_state.exposure -= 0.1f;
    if (app_shortcut(ImGuiKey_RightBracket))
        ui_state.exposure += 0.1f;
    if (app_shortcut(ImGuiMod_Shift | ImGuiKey_RightBracket))
        ui_state.exposure += 0.5f;
    if (app_shortcut(ImGuiMod_Shift | ImGuiKey_9))
        ui_state.gamma = std::max(0.1f, ui_state.gamma - 0.1f);
    if (app_shortcut(ImGuiMod_Shift | ImGuiKey_0))
        ui_state.gamma += 0.1f;
    if (app_shortcut(ImGuiKey_Delete) && has_image)
        actions.delete_from_disk_requested = true;
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_L))
        actions.rotate_left_requested = true;
    if (app_shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_R))
        actions.rotate_right_requested = true;
}

void
draw_viewer_main_menu(ViewerState& viewer, PlaceholderUiState& ui_state,
                      DeveloperUiState& developer_ui,
                      ViewerFrameActions& actions, bool& request_exit
#if defined(IMGUI_ENABLE_TEST_ENGINE)
                      ,
                      bool show_test_menu, bool* show_test_engine_windows
#endif
)
{
    const bool has_image         = !viewer.image.path.empty();
    const bool has_selection     = has_image_selection(viewer);
    const bool can_prev_subimage = has_image
                                   && (viewer.image.miplevel > 0
                                       || viewer.image.subimage > 0
                                       || viewer.image.nsubimages > 1);
    const bool can_next_subimage
        = has_image
          && (viewer.auto_subimage
              || viewer.image.miplevel + 1 < viewer.image.nmiplevels
              || viewer.image.subimage + 1 < viewer.image.nsubimages);
    const bool can_prev_mip = has_image && viewer.image.miplevel > 0;
    const bool can_next_mip
        = has_image && (viewer.image.miplevel + 1 < viewer.image.nmiplevels);

    if (!ImGui::BeginMainMenuBar())
        return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open...", "Ctrl+O"))
            actions.open_requested = true;

        if (ImGui::BeginMenu("Open recent...")) {
            if (viewer.recent_images.empty()) {
                ImGui::MenuItem("No recent files", nullptr, false, false);
            } else {
                for (size_t i = 0; i < viewer.recent_images.size(); ++i) {
                    const std::string& recent = viewer.recent_images[i];
                    const std::string label
                        = Strutil::fmt::format("{}: {}##imiv_recent_{}", i + 1,
                                               recent, i);
                    if (ImGui::MenuItem(label.c_str()))
                        actions.recent_open_path = recent;
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Clear recent list", nullptr, false,
                                !viewer.recent_images.empty())) {
                actions.clear_recent_requested = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Reload image", "Ctrl+R", false, has_image))
            actions.reload_requested = true;
        if (ImGui::MenuItem("Close image", "Ctrl+W", false, has_image))
            actions.close_requested = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Save As...", "Ctrl+S", false, has_image))
            actions.save_as_requested = true;
        if (ImGui::MenuItem("Save Window As...", nullptr, false, has_image))
            actions.save_window_as_requested = true;
        if (ImGui::MenuItem("Save Selection As...", nullptr, false, has_image)) {
            actions.save_selection_as_requested = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Move to new window", nullptr, false, has_image)) {
            viewer.status_message
                = "Move to new window is not available in imiv yet";
        }
        if (ImGui::MenuItem("Delete from disk", "Delete", false, has_image))
            actions.delete_from_disk_requested = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Preferences...", "Ctrl+,"))
            ui_state.show_preferences_window = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Ctrl+Q"))
            request_exit = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Select")) {
        bool area_sample_enabled = ui_state.show_area_probe_window;
        if (ImGui::MenuItem("Toggle Area Sample", "Ctrl+A",
                            &area_sample_enabled)) {
            set_area_sample_enabled(viewer, ui_state, area_sample_enabled);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Select All", "Ctrl+Shift+A", false, has_image))
            actions.select_all_requested = true;
        if (ImGui::MenuItem("Deselect", "Ctrl+D", false, has_selection))
            actions.deselect_selection_requested = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Previous Image", "PgUp"))
            actions.prev_requested = true;
        if (ImGui::MenuItem("Next Image", "PgDown"))
            actions.next_requested = true;
        if (ImGui::MenuItem("Toggle image", "T"))
            actions.toggle_requested = true;
        ImGui::MenuItem("Show display/data window borders", nullptr,
                        &ui_state.show_window_guides);
        ImGui::Separator();

        if (ImGui::MenuItem("Zoom In", "Ctrl++", false, has_image))
            request_zoom_scale(actions.pending_zoom, 2.0f, false);
        if (ImGui::MenuItem("Zoom Out", "Ctrl+-", false, has_image))
            request_zoom_scale(actions.pending_zoom, 0.5f, false);
        if (ImGui::MenuItem("Normal Size (1:1)", "Ctrl+0", false, has_image)) {
            request_zoom_reset(actions.pending_zoom, false);
        }
        if (ImGui::MenuItem("Re-center Image", "Ctrl+.", false, has_image))
            actions.recenter_requested = true;
        if (ImGui::MenuItem("Fit Window to Image", "F", false, has_image))
            actions.fit_window_to_image_requested = true;
        if (ImGui::MenuItem("Fit Image to Window", "Alt+F",
                            ui_state.fit_image_to_window, has_image)) {
            ui_state.fit_image_to_window = !ui_state.fit_image_to_window;
            viewer.fit_request           = true;
        }
        if (ImGui::MenuItem("Full screen", "Ctrl+F",
                            ui_state.full_screen_mode)) {
            actions.full_screen_toggle_requested = true;
        }
        ImGui::Separator();

        if (ImGui::MenuItem("Prev Subimage", "<", false, can_prev_subimage))
            actions.prev_subimage_requested = true;
        if (ImGui::MenuItem("Next Subimage", ">", false, can_next_subimage))
            actions.next_subimage_requested = true;
        if (ImGui::MenuItem("Prev MIP level", nullptr, false, can_prev_mip))
            actions.prev_mip_requested = true;
        if (ImGui::MenuItem("Next MIP level", nullptr, false, can_next_mip))
            actions.next_mip_requested = true;

        if (ImGui::BeginMenu("Channels")) {
            if (ImGui::MenuItem("Full Color", "C",
                                ui_state.current_channel == 0))
                ui_state.current_channel = 0;
            if (ImGui::MenuItem("Red", "R", ui_state.current_channel == 1))
                ui_state.current_channel = 1;
            if (ImGui::MenuItem("Green", "G", ui_state.current_channel == 2))
                ui_state.current_channel = 2;
            if (ImGui::MenuItem("Blue", "B", ui_state.current_channel == 3))
                ui_state.current_channel = 3;
            if (ImGui::MenuItem("Alpha", "A", ui_state.current_channel == 4))
                ui_state.current_channel = 4;
            ImGui::Separator();
            if (ImGui::MenuItem("Prev Channel", ",", false, has_image)) {
                ui_state.current_channel = std::max(0, ui_state.current_channel
                                                           - 1);
            }
            if (ImGui::MenuItem("Next Channel", ".", false, has_image)) {
                ui_state.current_channel = std::min(4, ui_state.current_channel
                                                           + 1);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Color mode")) {
            if (ImGui::MenuItem("RGBA", nullptr, ui_state.color_mode == 0))
                ui_state.color_mode = 0;
            if (ImGui::MenuItem("RGB", nullptr, ui_state.color_mode == 1))
                ui_state.color_mode = 1;
            if (ImGui::MenuItem("Single channel", "1", ui_state.color_mode == 2))
                ui_state.color_mode = 2;
            if (ImGui::MenuItem("Luminance", "L", ui_state.color_mode == 3))
                ui_state.color_mode = 3;
            if (ImGui::MenuItem("Single channel (Heatmap)", "H",
                                ui_state.color_mode == 4)) {
                ui_state.color_mode = 4;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("OCIO")) {
            ImGui::MenuItem("Use OCIO", nullptr, &ui_state.use_ocio);
            std::vector<std::string> ocio_color_spaces;
            std::vector<std::string> ocio_displays;
            std::vector<std::string> ocio_views;
            std::string resolved_display;
            std::string resolved_view;
            std::string ocio_error;
            const bool ocio_menu_data_ok = query_ocio_menu_data(
                ui_state, ocio_color_spaces, ocio_displays, ocio_views,
                resolved_display, resolved_view, ocio_error);
            if (ocio_menu_data_ok) {
                const auto contains = [](const std::vector<std::string>& values,
                                         const std::string& value) {
                    return std::find(values.begin(), values.end(), value)
                           != values.end();
                };
                if (ui_state.ocio_image_color_space.empty()
                    || (ui_state.ocio_image_color_space != "auto"
                        && !contains(ocio_color_spaces,
                                     ui_state.ocio_image_color_space))) {
                    ui_state.ocio_image_color_space = "auto";
                }
                if (ui_state.ocio_display.empty()
                    || (ui_state.ocio_display != "default"
                        && ui_state.ocio_display != resolved_display)) {
                    ui_state.ocio_display = "default";
                    ui_state.ocio_view    = "default";
                } else if (ui_state.ocio_view.empty()
                           || (ui_state.ocio_view != "default"
                               && ui_state.ocio_view != resolved_view)) {
                    ui_state.ocio_view = "default";
                }
            }
            if (ImGui::BeginMenu("Image color space")) {
                if (!ocio_menu_data_ok) {
                    ImGui::MenuItem(ocio_error.c_str(), nullptr, false, false);
                } else {
                    for (const std::string& color_space : ocio_color_spaces) {
                        if (ImGui::MenuItem(color_space.c_str(), nullptr,
                                            ui_state.ocio_image_color_space
                                                == color_space)) {
                            ui_state.ocio_image_color_space = color_space;
                        }
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Display/View")) {
                if (!ocio_menu_data_ok) {
                    ImGui::MenuItem(ocio_error.c_str(), nullptr, false, false);
                } else {
                    if (ImGui::MenuItem("default / default", nullptr,
                                        ui_state.ocio_display == "default"
                                            && ui_state.ocio_view
                                                   == "default")) {
                        ui_state.ocio_display = "default";
                        ui_state.ocio_view    = "default";
                    }
                    ImGui::Separator();
                    for (const std::string& display_name : ocio_displays) {
                        if (ImGui::BeginMenu(display_name.c_str())) {
                            if (display_name == resolved_display
                                && ImGui::MenuItem("default", nullptr,
                                                   ui_state.ocio_display
                                                       == "default")) {
                                ui_state.ocio_display = "default";
                                ui_state.ocio_view    = "default";
                            }
                            std::vector<std::string> display_views;
                            std::vector<std::string> display_color_spaces;
                            std::vector<std::string> display_displays;
                            std::string ignored_display;
                            std::string ignored_view;
                            std::string display_error;
                            PlaceholderUiState probe_state = ui_state;
                            probe_state.ocio_display       = display_name;
                            probe_state.ocio_view          = "default";
                            if (!query_ocio_menu_data(
                                    probe_state, display_color_spaces,
                                    display_displays, display_views,
                                    ignored_display, ignored_view,
                                    display_error)) {
                                ImGui::MenuItem(display_error.c_str(), nullptr,
                                                false, false);
                            } else {
                                for (const std::string& view_name :
                                     display_views) {
                                    const bool selected
                                        = ui_state.ocio_display == display_name
                                          && ((ui_state.ocio_view == "default"
                                               && view_name == ignored_view)
                                              || ui_state.ocio_view
                                                     == view_name);
                                    if (ImGui::MenuItem(view_name.c_str(),
                                                        nullptr, selected)) {
                                        ui_state.ocio_display = display_name;
                                        ui_state.ocio_view    = view_name;
                                    }
                                }
                            }
                            ImGui::EndMenu();
                        }
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Exposure/gamma")) {
            if (ImGui::MenuItem("Exposure -1/2 stop", "{"))
                ui_state.exposure -= 0.5f;
            if (ImGui::MenuItem("Exposure -1/10 stop", "["))
                ui_state.exposure -= 0.1f;
            if (ImGui::MenuItem("Exposure +1/10 stop", "]"))
                ui_state.exposure += 0.1f;
            if (ImGui::MenuItem("Exposure +1/2 stop", "}"))
                ui_state.exposure += 0.5f;
            if (ImGui::MenuItem("Gamma -0.1", "("))
                ui_state.gamma = std::max(0.1f, ui_state.gamma - 0.1f);
            if (ImGui::MenuItem("Gamma +0.1", ")"))
                ui_state.gamma += 0.1f;
            if (ImGui::MenuItem("Reset exposure/gamma")) {
                ui_state.exposure = 0.0f;
                ui_state.gamma    = 1.0f;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Tools")) {
        ImGui::MenuItem("Image info...", "Ctrl+I", &ui_state.show_info_window);
        ImGui::MenuItem("Preview controls...", nullptr,
                        &ui_state.show_preview_window);
        ImGui::MenuItem("Pixel closeup view...", "P",
                        &ui_state.show_pixelview_window);

        if (ImGui::BeginMenu("Slide Show")) {
            if (ImGui::MenuItem("Start Slide Show", nullptr,
                                ui_state.slide_show_running)) {
                toggle_slide_show_action(ui_state, viewer);
            }
            if (ImGui::MenuItem("Loop slide show", nullptr,
                                ui_state.slide_loop)) {
                ui_state.slide_loop = !ui_state.slide_loop;
            }
            if (ImGui::MenuItem("Stop at end", nullptr, !ui_state.slide_loop))
                ui_state.slide_loop = false;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Sort")) {
            if (ImGui::MenuItem("By Name"))
                set_sort_mode_action(viewer, ImageSortMode::ByName);
            if (ImGui::MenuItem("By File Path"))
                set_sort_mode_action(viewer, ImageSortMode::ByPath);
            if (ImGui::MenuItem("By Image Date"))
                set_sort_mode_action(viewer, ImageSortMode::ByImageDate);
            if (ImGui::MenuItem("By File Date"))
                set_sort_mode_action(viewer, ImageSortMode::ByFileDate);
            if (ImGui::MenuItem("Reverse current order"))
                toggle_sort_reverse_action(viewer);
            ImGui::EndMenu();
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Rotate Left", "Ctrl+Shift+L"))
            actions.rotate_left_requested = true;
        if (ImGui::MenuItem("Rotate Right", "Ctrl+Shift+R"))
            actions.rotate_right_requested = true;
        if (ImGui::MenuItem("Flip Horizontal"))
            actions.flip_horizontal_requested = true;
        if (ImGui::MenuItem("Flip Vertical"))
            actions.flip_vertical_requested = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About"))
            ImGui::OpenPopup("About imiv");
        ImGui::EndMenu();
    }

#if !defined(NDEBUG)
    if (ImGui::BeginMenu("Developer")) {
        ImGui::MenuItem("ImGui Demo", nullptr,
                        &developer_ui.show_imgui_demo_window);
        ImGui::MenuItem("ImGui Style Editor", nullptr,
                        &developer_ui.show_imgui_style_editor);
        ImGui::MenuItem("ImGui Metrics/Debugger", nullptr,
                        &developer_ui.show_imgui_metrics_window);
        ImGui::MenuItem("ImGui Debug Log", nullptr,
                        &developer_ui.show_imgui_debug_log_window);
        ImGui::MenuItem("ImGui ID Stack Tool", nullptr,
                        &developer_ui.show_imgui_id_stack_window);
        ImGui::MenuItem("ImGui About", nullptr,
                        &developer_ui.show_imgui_about_window);
        ImGui::Separator();
        if (ImGui::MenuItem("Capture Main Window", "F12", false,
                            !developer_ui.screenshot_busy)) {
            developer_ui.request_screenshot = true;
        }
#    if defined(IMGUI_ENABLE_TEST_ENGINE)
        if (show_test_engine_windows != nullptr) {
            ImGui::Separator();
            ImGui::MenuItem("Test Engine Windows", nullptr,
                            show_test_engine_windows);
        }
#    endif
        ImGui::EndMenu();
    }
#endif

#if defined(IMGUI_ENABLE_TEST_ENGINE)
    if (show_test_engine_windows != nullptr && show_test_menu
        && ImGui::BeginMenu("Tests")) {
        ImGui::MenuItem("Show test engine windows", nullptr,
                        show_test_engine_windows);
        ImGui::EndMenu();
    }
#endif

    ImGui::EndMainMenuBar();
}

void
execute_viewer_frame_actions(ViewerState& viewer, PlaceholderUiState& ui_state,
                             ViewerFrameActions& actions
#if defined(IMIV_BACKEND_VULKAN_GLFW)
                             ,
                             GLFWwindow* window, RendererState& vk_state
#endif
)
{
    if (actions.open_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        open_image_dialog_action(vk_state, viewer, ui_state,
                                 ui_state.subimage_index,
                                 ui_state.miplevel_index);
#else
        set_placeholder_status(viewer, "Open image");
#endif
        actions.open_requested = false;
    }
    if (!actions.recent_open_path.empty()) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        load_viewer_image(vk_state, viewer, &ui_state, actions.recent_open_path,
                          ui_state.subimage_index, ui_state.miplevel_index);
#else
        set_placeholder_status(viewer, "Open recent image");
#endif
        actions.recent_open_path.clear();
    }
    if (actions.clear_recent_requested) {
        viewer.recent_images.clear();
        viewer.status_message = "Cleared recent files list";
        viewer.last_error.clear();
        actions.clear_recent_requested = false;
    }
    if (actions.reload_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        reload_current_image_action(vk_state, viewer, ui_state);
#else
        set_placeholder_status(viewer, "Reload image");
#endif
        actions.reload_requested = false;
    }
    if (actions.close_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        close_current_image_action(vk_state, viewer, ui_state);
#else
        set_placeholder_status(viewer, "Close image");
#endif
        actions.close_requested = false;
    }
    if (actions.prev_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        next_sibling_image_action(vk_state, viewer, ui_state, -1);
#else
        set_placeholder_status(viewer, "Previous Image");
#endif
        actions.prev_requested = false;
    }
    if (actions.next_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        next_sibling_image_action(vk_state, viewer, ui_state, 1);
#else
        set_placeholder_status(viewer, "Next Image");
#endif
        actions.next_requested = false;
    }
    if (actions.toggle_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        toggle_image_action(vk_state, viewer, ui_state);
#else
        set_placeholder_status(viewer, "Toggle image");
#endif
        actions.toggle_requested = false;
    }
    if (actions.prev_subimage_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        change_subimage_action(vk_state, viewer, ui_state, -1);
#else
        set_placeholder_status(viewer, "Prev Subimage");
#endif
        actions.prev_subimage_requested = false;
    }
    if (actions.next_subimage_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        change_subimage_action(vk_state, viewer, ui_state, 1);
#else
        set_placeholder_status(viewer, "Next Subimage");
#endif
        actions.next_subimage_requested = false;
    }
    if (actions.prev_mip_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        change_miplevel_action(vk_state, viewer, ui_state, -1);
#else
        set_placeholder_status(viewer, "Prev MIP level");
#endif
        actions.prev_mip_requested = false;
    }
    if (actions.next_mip_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        change_miplevel_action(vk_state, viewer, ui_state, 1);
#else
        set_placeholder_status(viewer, "Next MIP level");
#endif
        actions.next_mip_requested = false;
    }
    if (actions.save_as_requested) {
        save_as_dialog_action(viewer);
        actions.save_as_requested = false;
    }
    if (actions.save_window_as_requested) {
        save_window_as_dialog_action(viewer);
        actions.save_window_as_requested = false;
    }
    if (actions.save_selection_as_requested) {
        save_selection_as_dialog_action(viewer);
        actions.save_selection_as_requested = false;
    }
    if (actions.select_all_requested) {
        select_all_image_action(viewer, ui_state);
        actions.select_all_requested = false;
    }
    if (actions.deselect_selection_requested) {
        deselect_selection_action(viewer, ui_state);
        actions.deselect_selection_requested = false;
    }
    if (actions.fit_window_to_image_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        fit_window_to_image_action(window, viewer, ui_state);
#else
        viewer.status_message = "Fit window to image is unavailable";
#endif
        actions.fit_window_to_image_requested = false;
    }
    if (actions.full_screen_toggle_requested) {
        ui_state.full_screen_mode = !ui_state.full_screen_mode;
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        std::string fullscreen_error;
        set_full_screen_mode(window, viewer, ui_state.full_screen_mode,
                             fullscreen_error);
        if (!fullscreen_error.empty()) {
            viewer.last_error         = fullscreen_error;
            ui_state.full_screen_mode = viewer.fullscreen_applied;
        } else {
            viewer.status_message = ui_state.full_screen_mode
                                        ? "Entered full screen"
                                        : "Exited full screen";
            viewer.last_error.clear();
        }
#endif
        actions.full_screen_toggle_requested = false;
    }
    if (actions.delete_from_disk_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        if (!viewer.image.path.empty()) {
            const std::string to_delete = viewer.image.path;
            close_current_image_action(vk_state, viewer, ui_state);
            std::error_code ec;
            if (std::filesystem::remove(to_delete, ec)) {
                viewer.status_message = Strutil::fmt::format("Deleted {}",
                                                             to_delete);
                viewer.last_error.clear();
            } else {
                viewer.last_error
                    = ec ? Strutil::fmt::format("Delete failed: {}",
                                                ec.message())
                         : "Delete failed";
            }
        }
#endif
        actions.delete_from_disk_requested = false;
    }
    if (actions.rotate_left_requested || actions.rotate_right_requested
        || actions.flip_horizontal_requested
        || actions.flip_vertical_requested) {
        if (!viewer.image.path.empty()) {
            int orientation = clamp_orientation(viewer.image.orientation);
            if (actions.rotate_left_requested) {
                static const int next_orientation[] = { 0, 8, 5, 6, 7,
                                                        4, 1, 2, 3 };
                orientation = next_orientation[orientation];
            }
            if (actions.rotate_right_requested) {
                static const int next_orientation[] = { 0, 6, 7, 8, 5,
                                                        2, 3, 4, 1 };
                orientation = next_orientation[orientation];
            }
            if (actions.flip_horizontal_requested) {
                static const int next_orientation[] = { 0, 2, 1, 4, 3,
                                                        6, 5, 8, 7 };
                orientation = next_orientation[orientation];
            }
            if (actions.flip_vertical_requested) {
                static const int next_orientation[] = { 0, 4, 3, 2, 1,
                                                        8, 7, 6, 5 };
                orientation = next_orientation[orientation];
            }
            viewer.image.orientation = clamp_orientation(orientation);
            viewer.fit_request       = true;
            viewer.status_message
                = Strutil::fmt::format("Orientation set to {}",
                                       viewer.image.orientation);
            viewer.last_error.clear();
        } else {
            viewer.status_message = "No image loaded";
            viewer.last_error.clear();
        }
        actions.rotate_left_requested     = false;
        actions.rotate_right_requested    = false;
        actions.flip_horizontal_requested = false;
        actions.flip_vertical_requested   = false;
    }

#if defined(IMIV_BACKEND_VULKAN_GLFW)
    if (ui_state.slide_show_running && !viewer.image.path.empty()
        && !viewer.loaded_image_paths.empty()) {
        const double now = ImGui::GetTime();
        if (viewer.slide_last_advance_time <= 0.0)
            viewer.slide_last_advance_time = now;
        const double delay = std::max(1, ui_state.slide_duration_seconds);
        if (now - viewer.slide_last_advance_time >= delay) {
            (void)advance_slide_show_action(vk_state, viewer, ui_state);
            viewer.slide_last_advance_time = now;
        }
    } else {
        viewer.slide_last_advance_time = 0.0;
    }
#else
    viewer.slide_last_advance_time = 0.0;
#endif
}

}  // namespace Imiv
