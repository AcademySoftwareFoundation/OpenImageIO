// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_vulkan_types.h"

namespace Imiv {

#if defined(IMIV_WITH_VULKAN)

void
destroy_texture_upload_submit_resources(VulkanState& vk_state,
                                        VulkanTexture& texture);
bool
poll_texture_upload_submission(VulkanState& vk_state, VulkanTexture& texture,
                               bool wait_for_completion,
                               std::string& error_message);
void
destroy_texture_preview_submit_resources(VulkanState& vk_state,
                                         VulkanTexture& texture);

#endif

}  // namespace Imiv
