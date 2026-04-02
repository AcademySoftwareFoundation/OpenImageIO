// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <imgui_impl_metal.h>

#if defined(__OBJC__) && !defined(IMGUI_DISABLE)

@protocol MTLTexture;
@protocol MTLSamplerState;

IMGUI_IMPL_API ImTextureID
ImGui_ImplMetal_CreateUserTextureID(id<MTLTexture> texture,
                                    id<MTLSamplerState> sampler_state);
IMGUI_IMPL_API void
ImGui_ImplMetal_DestroyUserTextureID(ImTextureID tex_id);

#endif
