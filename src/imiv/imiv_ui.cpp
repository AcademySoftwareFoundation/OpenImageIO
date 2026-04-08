// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_ui.h"
#include "imiv_loaded_image.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <OpenImageIO/half.h>

namespace Imiv {

template<class T>
T
read_unaligned_value(const unsigned char* ptr)
{
    T value = {};
    std::memcpy(&value, ptr, sizeof(T));
    return value;
}

bool
sample_loaded_pixel(const LoadedImage& image, int x, int y,
                    std::vector<double>& out_channels)
{
    out_channels.clear();
    const unsigned char* src = nullptr;
    LoadedImageLayout layout;
    if (!loaded_image_pixel_pointer(image, x, y, src, &layout))
        return false;

    out_channels.resize(static_cast<size_t>(image.nchannels));
    for (size_t c = 0; c < out_channels.size(); ++c) {
        const unsigned char* channel_ptr = src + c * image.channel_bytes;
        double v                         = 0.0;
        switch (image.type) {
        case UploadDataType::UInt8:
            v = static_cast<double>(*channel_ptr);
            break;
        case UploadDataType::UInt16:
            v = static_cast<double>(
                read_unaligned_value<uint16_t>(channel_ptr));
            break;
        case UploadDataType::UInt32:
            v = static_cast<double>(
                read_unaligned_value<uint32_t>(channel_ptr));
            break;
        case UploadDataType::Half:
            v = static_cast<double>(
                static_cast<float>(read_unaligned_value<half>(channel_ptr)));
            break;
        case UploadDataType::Float:
            v = static_cast<double>(read_unaligned_value<float>(channel_ptr));
            break;
        case UploadDataType::Double:
            v = read_unaligned_value<double>(channel_ptr);
            break;
        default: return false;
        }
        out_channels[c] = v;
    }
    return true;
}

void
draw_padded_message(const char* message, float x_pad, float y_pad)
{
    if (!message || message[0] == '\0')
        return;
    ImVec2 pos = ImGui::GetCursorPos();
    pos.x += x_pad;
    pos.y += y_pad;
    ImGui::SetCursorPos(pos);
    const float wrap_width = ImGui::GetCursorPosX()
                             + std::max(64.0f, ImGui::GetContentRegionAvail().x
                                                   - x_pad);
    ImGui::PushTextWrapPos(wrap_width);
    ImGui::TextUnformatted(message);
    ImGui::PopTextWrapPos();
}

bool
input_text_string(const char* label, std::string& value)
{
    return ImGui::InputText(label, &value);
}

void
push_active_button_style(bool active)
{
    if (!active)
        return;
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(66, 112, 171, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(80, 133, 200, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(57, 95, 146, 255));
}

void
pop_active_button_style(bool active)
{
    if (!active)
        return;
    ImGui::PopStyleColor(3);
}

bool
begin_two_column_table(const char* id, float label_column_width,
                       ImGuiTableFlags flags, const char* label_column_name,
                       const char* value_column_name)
{
    if (!ImGui::BeginTable(id, 2, flags))
        return false;
    ImGui::TableSetupColumn(label_column_name, ImGuiTableColumnFlags_WidthFixed,
                            label_column_width);
    ImGui::TableSetupColumn(value_column_name,
                            ImGuiTableColumnFlags_WidthStretch);
    return true;
}

void
table_labeled_row(const char* label)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
}

void
draw_wrapped_value_row(const char* label, const char* value)
{
    table_labeled_row(label);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(value != nullptr ? value : "");
    ImGui::PopTextWrapPos();
}

void
draw_section_heading(const char* title, float separator_padding_y)
{
    const ImVec2 separator_padding
        = ImVec2(ImGui::GetStyle().SeparatorTextPadding.x, separator_padding_y);
    ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextPadding, separator_padding);
    ImGui::PushStyleColor(ImGuiCol_Text,
                          ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::SeparatorText(title);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void
align_control_right(float width)
{
    const float available_width = ImGui::GetContentRegionAvail().x;
    if (available_width > width)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + available_width - width);
}

bool
draw_right_aligned_checkbox(const char* id, bool& value)
{
    align_control_right(ImGui::GetFrameHeight());
    return ImGui::Checkbox(id, &value);
}

void
draw_right_aligned_text(const char* value)
{
    const char* text = value != nullptr ? value : "";
    align_control_right(ImGui::CalcTextSize(text).x);
    ImGui::TextUnformatted(text);
}

bool
draw_right_aligned_int_stepper(const char* id, int& value, int step,
                               const char* suffix, float button_width,
                               float value_width)
{
    ImGui::PushID(id);
    const float spacing      = ImGui::GetStyle().ItemSpacing.x;
    const float suffix_width = (suffix != nullptr && suffix[0] != '\0')
                                   ? ImGui::CalcTextSize(suffix).x + spacing
                                   : 0.0f;
    const float total_width = value_width + button_width * 2.0f + spacing * 2.0f
                              + suffix_width;
    bool changed = false;
    align_control_right(total_width);
    ImGui::SetNextItemWidth(value_width);
    changed |= ImGui::InputInt("##value", &value, 0, 0);
    ImGui::SameLine(0.0f, spacing);
    if (ImGui::Button("-", ImVec2(button_width, 0.0f))) {
        value -= step;
        changed = true;
    }
    ImGui::SameLine(0.0f, spacing);
    if (ImGui::Button("+", ImVec2(button_width, 0.0f))) {
        value += step;
        changed = true;
    }
    if (suffix != nullptr && suffix[0] != '\0') {
        ImGui::SameLine(0.0f, spacing);
        ImGui::TextUnformatted(suffix);
    }
    ImGui::PopID();
    return changed;
}

void
draw_disabled_wrapped_text(const char* message)
{
    ImGui::PushStyleColor(ImGuiCol_Text,
                          ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(message != nullptr ? message : "");
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

}  // namespace Imiv
