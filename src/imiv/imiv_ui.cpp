// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_ui.h"
#include "imiv_loaded_image.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

#include <imgui.h>

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

}  // namespace Imiv
