// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_vulkan_types.h"

#include <imgui.h>

#if defined(IMIV_WITH_VULKAN)
#    include <imgui_impl_vulkan.h>
#endif

namespace Imiv {

#if defined(IMIV_WITH_VULKAN)

bool
setup_vulkan_window(VulkanState& vk_state, int width, int height,
                    std::string& error_message)
{
    VkBool32 has_wsi = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(vk_state.physical_device,
                                         vk_state.queue_family,
                                         vk_state.surface, &has_wsi);
    if (has_wsi != VK_TRUE) {
        error_message = "no WSI support on selected device";
        return false;
    }

    const VkFormat request_surface_formats[]
        = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR request_color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    vk_state.window_data.Surface       = vk_state.surface;
    vk_state.window_data.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
        vk_state.physical_device, vk_state.window_data.Surface,
        request_surface_formats,
        static_cast<int>(IM_ARRAYSIZE(request_surface_formats)),
        request_color_space);

    const VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
    vk_state.window_data.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
        vk_state.physical_device, vk_state.window_data.Surface, present_modes,
        static_cast<int>(IM_ARRAYSIZE(present_modes)));

    ImGui_ImplVulkanH_CreateOrResizeWindow(
        vk_state.instance, vk_state.physical_device, vk_state.device,
        &vk_state.window_data, vk_state.queue_family, vk_state.allocator, width,
        height, vk_state.min_image_count, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    name_window_frame_objects(vk_state);
    return true;
}

void
destroy_vulkan_surface(VulkanState& vk_state)
{
    if (vk_state.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(vk_state.instance, vk_state.surface,
                            vk_state.allocator);
        vk_state.surface = VK_NULL_HANDLE;
    }
}

void
cleanup_vulkan_window(VulkanState& vk_state)
{
    if (vk_state.window_data.Swapchain != VK_NULL_HANDLE) {
        ImGui_ImplVulkanH_DestroyWindow(vk_state.instance, vk_state.device,
                                        &vk_state.window_data,
                                        vk_state.allocator);
        vk_state.window_data = ImGui_ImplVulkanH_Window();
    }
    destroy_vulkan_surface(vk_state);
}

#endif

}  // namespace Imiv
