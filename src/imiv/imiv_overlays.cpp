// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_ui.h"
#include "imiv_ui_metrics.h"

#include "imiv_actions.h"
#include "imiv_test_engine.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

#include <imgui.h>

#include <OpenImageIO/strutil.h>

using namespace OIIO;

namespace Imiv {

namespace {

    bool image_selection_rect_to_screen(const ViewerState& viewer,
                                        const ImageCoordinateMap& map,
                                        ImVec2& out_min, ImVec2& out_max)
    {
        if (!has_image_selection(viewer) || !map.valid
            || viewer.image.width <= 0 || viewer.image.height <= 0) {
            return false;
        }

        const float x0 = static_cast<float>(viewer.selection_xbegin)
                         / static_cast<float>(viewer.image.width);
        const float x1 = static_cast<float>(viewer.selection_xend)
                         / static_cast<float>(viewer.image.width);
        const float y0 = static_cast<float>(viewer.selection_ybegin)
                         / static_cast<float>(viewer.image.height);
        const float y1 = static_cast<float>(viewer.selection_yend)
                         / static_cast<float>(viewer.image.height);

        const ImVec2 corners_uv[] = { ImVec2(x0, y0), ImVec2(x1, y0),
                                      ImVec2(x1, y1), ImVec2(x0, y1) };
        ImVec2 screen_corners[4];
        for (int i = 0; i < 4; ++i) {
            if (!source_uv_to_screen(map, corners_uv[i], screen_corners[i]))
                return false;
        }

        out_min = screen_corners[0];
        out_max = screen_corners[0];
        for (int i = 1; i < 4; ++i) {
            out_min.x = std::min(out_min.x, screen_corners[i].x);
            out_min.y = std::min(out_min.y, screen_corners[i].y);
            out_max.x = std::max(out_max.x, screen_corners[i].x);
            out_max.y = std::max(out_max.y, screen_corners[i].y);
        }
        return true;
    }

    const char* channel_view_name(int mode)
    {
        switch (mode) {
        case 0: return "Full Color";
        case 1: return "Red";
        case 2: return "Green";
        case 3: return "Blue";
        case 4: return "Alpha";
        default: break;
        }
        return "Unknown";
    }

    const char* color_mode_name(int mode)
    {
        switch (mode) {
        case 0: return "RGBA";
        case 1: return "RGB";
        case 2: return "Single channel";
        case 3: return "Luminance";
        case 4: return "Heatmap";
        default: break;
        }
        return "Unknown";
    }

    const char* mouse_mode_name(int mode)
    {
        switch (mode) {
        case 0: return "Navigate";
        case 1: return "Pan";
        case 2: return "Wipe";
        case 3: return "Area Sample";
        case 4: return "Annotate";
        default: break;
        }
        return "Navigate";
    }

    std::string upper_ascii_copy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return std::toupper(c); });
        return value;
    }

    std::string status_channel_layout_text(const LoadedImage& image)
    {
        switch (image.nchannels) {
        case 1: return "GRAY";
        case 2: return "RG";
        case 3: return "RGB";
        case 4: return "RGBA";
        default: break;
        }
        return Strutil::fmt::format("{}CH", std::max(0, image.nchannels));
    }

    std::string status_image_text(const ViewerState& viewer)
    {
        if (viewer.image.path.empty())
            return "No image loaded";
        const std::filesystem::path path(viewer.image.path);
        const std::string filename = path.filename().empty()
                                         ? viewer.image.path
                                         : path.filename().string();
        return filename;
    }

    std::string status_specs_text(const ViewerState& viewer)
    {
        if (viewer.image.path.empty())
            return "";

        std::string type_name = viewer.image.data_format_name;
        if (type_name.empty()) {
            type_name = std::string(
                upload_data_type_to_typedesc(viewer.image.type).c_str());
        }
        if (type_name.empty() || type_name == "unknown")
            type_name = std::string(upload_data_type_name(viewer.image.type));
        type_name = upper_ascii_copy(type_name);

        return Strutil::fmt::format("{}x{}  {}  {}", viewer.image.width,
                                    viewer.image.height,
                                    status_channel_layout_text(viewer.image),
                                    type_name);
    }

    std::string status_preview_text(const ViewerState& viewer,
                                    const PlaceholderUiState& ui)
    {
        if (viewer.image.path.empty())
            return "";

        const float zoom  = std::max(viewer.zoom, 0.00001f);
        const float z_num = zoom >= 1.0f ? zoom : 1.0f;
        const float z_den = zoom >= 1.0f ? 1.0f : (1.0f / zoom);
        std::string text  = Strutil::fmt::format(
            "zoom {:.2f}:{:.2f}  exp {:+.1f}  gam {:.2f}  shift {:+.2f}", z_num,
            z_den, ui.exposure, ui.gamma, ui.offset);
        if (ui.color_mode != 0 || ui.current_channel != 0) {
            std::string mode = color_mode_name(ui.color_mode);
            if (ui.color_mode == 2 || ui.color_mode == 4) {
                mode += Strutil::fmt::format(" {}", ui.current_channel);
            } else {
                mode += Strutil::fmt::format(" ({})", channel_view_name(
                                                          ui.current_channel));
            }
            text += Strutil::fmt::format("  view {}", mode);
        }
        if (viewer.image.nsubimages > 1) {
            if (viewer.auto_subimage) {
                text += Strutil::fmt::format("  subimg AUTO ({}/{})",
                                             viewer.image.subimage + 1,
                                             viewer.image.nsubimages);
            } else {
                text += Strutil::fmt::format("  subimg {}/{}",
                                             viewer.image.subimage + 1,
                                             viewer.image.nsubimages);
            }
        }
        if (viewer.image.nmiplevels > 1) {
            text += Strutil::fmt::format("  MIP {}/{}",
                                         viewer.image.miplevel + 1,
                                         viewer.image.nmiplevels);
        }
        if (viewer.image.orientation != 1) {
            text += Strutil::fmt::format("  orient {}",
                                         viewer.image.orientation);
        }
        if (ui.show_mouse_mode_selector) {
            text += Strutil::fmt::format("  mouse {}",
                                         mouse_mode_name(ui.mouse_mode));
        }
        return text;
    }

}  // namespace

void
draw_image_selection_overlay(const ViewerState& viewer,
                             const ImageCoordinateMap& map)
{
    if (!map.valid)
        return;

    ImVec2 rect_min(0.0f, 0.0f);
    ImVec2 rect_max(0.0f, 0.0f);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->PushClipRect(map.viewport_rect_min, map.viewport_rect_max, true);
    if (viewer.selection_drag_active) {
        ImVec2 start_screen(0.0f, 0.0f);
        ImVec2 end_screen(0.0f, 0.0f);
        if (source_uv_to_screen(map, viewer.selection_drag_start_uv,
                                start_screen)
            && source_uv_to_screen(map, viewer.selection_drag_end_uv,
                                   end_screen)) {
            rect_min = ImVec2(std::min(start_screen.x, end_screen.x),
                              std::min(start_screen.y, end_screen.y));
            rect_max = ImVec2(std::max(start_screen.x, end_screen.x),
                              std::max(start_screen.y, end_screen.y));
            draw_list->AddRectFilled(rect_min, rect_max,
                                     IM_COL32(72, 196, 255, 42), 0.0f);
            draw_list->AddRect(rect_min, rect_max, IM_COL32(72, 196, 255, 255),
                               0.0f, 0, 1.2f);
        }
    } else if (image_selection_rect_to_screen(viewer, map, rect_min, rect_max)) {
        draw_list->AddRect(rect_min, rect_max, IM_COL32(72, 196, 255, 255),
                           0.0f, 0, 1.2f);
    } else {
        draw_list->PopClipRect();
        return;
    }
    draw_list->PopClipRect();
    register_layout_dump_synthetic_rect("rect", "Image selection overlay",
                                        rect_min, rect_max);
}

void
draw_embedded_status_bar(ViewerState& viewer, PlaceholderUiState& ui)
{
    const std::string filename_text = status_image_text(viewer);
    const std::string specs_text    = status_specs_text(viewer);
    const std::string preview_text  = status_preview_text(viewer, ui);
    const bool show_progress        = false;

    int columns = 3;
    if (show_progress)
        ++columns;
    if (ui.show_mouse_mode_selector)
        ++columns;
    ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV
                                  | ImGuiTableFlags_PadOuterX
                                  | ImGuiTableFlags_SizingStretchProp
                                  | ImGuiTableFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,
                        UiMetrics::kStatusBarCellPadding);
    if (ImGui::BeginTable("##imiv_status_bar", columns, table_flags)) {
        ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthStretch,
                                2.2f);
        ImGui::TableSetupColumn("Specs", ImGuiTableColumnFlags_WidthStretch,
                                1.8f);
        ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch,
                                2.2f);
        if (show_progress) {
            ImGui::TableSetupColumn("Load", ImGuiTableColumnFlags_WidthFixed,
                                    UiMetrics::StatusBar::kLoadColumnWidth);
        }
        if (ui.show_mouse_mode_selector) {
            ImGui::TableSetupColumn("Mouse", ImGuiTableColumnFlags_WidthFixed,
                                    UiMetrics::StatusBar::kMouseColumnWidth);
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(filename_text.c_str());
        register_layout_dump_synthetic_item("text", filename_text.c_str());

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(specs_text.c_str());
        register_layout_dump_synthetic_item("text", specs_text.c_str());

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(preview_text.c_str());
        register_layout_dump_synthetic_item("text", preview_text.c_str());

        if (show_progress) {
            ImGui::TableNextColumn();
            ImGui::ProgressBar(0.0f, ImVec2(-1.0f, 0.0f), "idle");
        }

        if (ui.show_mouse_mode_selector) {
            ImGui::TableNextColumn();
            if (ui.show_area_probe_window) {
                ImGui::TextUnformatted("Area Sample");
                register_layout_dump_synthetic_item("text", "Area Sample");
            } else {
                static const char* mouse_modes[] = { "Navigate", "Pan", "Wipe",
                                                     "Annotate" };
                static const int mouse_mode_values[] = { 0, 1, 2, 4 };
                int combo_index                      = 0;
                for (int i = 0; i < IM_ARRAYSIZE(mouse_mode_values); ++i) {
                    if (ui.mouse_mode == mouse_mode_values[i]) {
                        combo_index = i;
                        break;
                    }
                }
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::Combo("##mouse_mode", &combo_index, mouse_modes,
                                 IM_ARRAYSIZE(mouse_modes))) {
                    set_mouse_mode_action(viewer, ui,
                                          mouse_mode_values[combo_index]);
                }
            }
        }

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

}  // namespace Imiv
