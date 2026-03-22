// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_renderer.h"

#include "imiv_renderer_backend.h"
#include "imiv_viewer.h"

namespace Imiv {

namespace {

const RendererBackendVTable*
renderer_backend_vtable(BackendKind kind)
{
    switch (kind) {
#if IMIV_WITH_VULKAN
    case BackendKind::Vulkan: return renderer_backend_vulkan_vtable();
#else
    case BackendKind::Vulkan: break;
#endif
#if IMIV_WITH_METAL
    case BackendKind::Metal: return renderer_backend_metal_vtable();
#else
    case BackendKind::Metal: break;
#endif
#if IMIV_WITH_OPENGL
    case BackendKind::OpenGL: return renderer_backend_opengl_vtable();
#else
    case BackendKind::OpenGL: break;
#endif
    case BackendKind::Auto: break;
    }
    return nullptr;
}

const RendererBackendVTable*
renderer_dispatch_vtable(const RendererState& renderer_state)
{
    if (renderer_state.vtable != nullptr)
        return renderer_state.vtable;
    return renderer_backend_vtable(renderer_state.active_backend);
}

const RendererBackendVTable*
texture_dispatch_vtable(const RendererTexture& texture)
{
    if (texture.vtable != nullptr)
        return texture.vtable;
    return renderer_backend_vtable(texture.backend_kind);
}

}  // namespace

void
renderer_select_backend(RendererState& renderer_state, BackendKind backend)
{
    renderer_state.active_backend = backend;
    renderer_state.vtable         = renderer_backend_vtable(backend);
}

BackendKind
renderer_active_backend(const RendererState& renderer_state)
{
    return renderer_state.active_backend;
}

bool
renderer_probe_backend_runtime_support(BackendKind backend,
                                       std::string& error_message)
{
    const RendererBackendVTable* vtable = renderer_backend_vtable(backend);
    if (vtable == nullptr || vtable->probe_runtime_support == nullptr) {
        error_message = "renderer backend is unavailable";
        return false;
    }
    return vtable->probe_runtime_support(error_message);
}

bool
renderer_texture_is_loading(const RendererTexture& texture)
{
    const RendererBackendVTable* vtable = texture_dispatch_vtable(texture);
    if (vtable == nullptr || vtable->texture_is_loading == nullptr)
        return false;
    return vtable->texture_is_loading(texture);
}

void
renderer_get_viewer_texture_refs(const ViewerState& viewer,
                                 const PlaceholderUiState& ui_state,
                                 ImTextureRef& main_texture_ref,
                                 bool& has_main_texture,
                                 ImTextureRef& closeup_texture_ref,
                                 bool& has_closeup_texture)
{
    main_texture_ref    = ImTextureRef();
    closeup_texture_ref = ImTextureRef();
    has_main_texture    = false;
    has_closeup_texture = false;
    const RendererBackendVTable* vtable = texture_dispatch_vtable(
        viewer.texture);
    if (vtable == nullptr || vtable->get_viewer_texture_refs == nullptr)
        return;
    vtable->get_viewer_texture_refs(viewer, ui_state, main_texture_ref,
                                    has_main_texture, closeup_texture_ref,
                                    has_closeup_texture);
}

bool
renderer_create_texture(RendererState& renderer_state, const LoadedImage& image,
                        RendererTexture& texture, std::string& error_message)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable == nullptr || vtable->create_texture == nullptr) {
        error_message = "renderer backend is unavailable";
        return false;
    }
    const bool ok = vtable->create_texture(renderer_state, image, texture,
                                           error_message);
    if (ok) {
        texture.backend_kind = renderer_state.active_backend;
        texture.vtable       = vtable;
    }
    return ok;
}

void
renderer_destroy_texture(RendererState& renderer_state,
                         RendererTexture& texture)
{
    const RendererBackendVTable* vtable = texture_dispatch_vtable(texture);
    if (vtable != nullptr && vtable->destroy_texture != nullptr)
        vtable->destroy_texture(renderer_state, texture);
    texture.vtable              = nullptr;
    texture.backend_kind        = BackendKind::Auto;
    texture.backend             = nullptr;
    texture.preview_initialized = false;
}

bool
renderer_update_preview_texture(RendererState& renderer_state,
                                RendererTexture& texture,
                                const LoadedImage* image,
                                const PlaceholderUiState& ui_state,
                                const RendererPreviewControls& controls,
                                std::string& error_message)
{
    const RendererBackendVTable* vtable = texture_dispatch_vtable(texture);
    if (vtable == nullptr || vtable->update_preview_texture == nullptr) {
        error_message = "renderer backend is unavailable";
        return false;
    }
    return vtable->update_preview_texture(renderer_state, texture, image,
                                          ui_state, controls, error_message);
}

bool
renderer_quiesce_texture_preview_submission(RendererState& renderer_state,
                                            RendererTexture& texture,
                                            std::string& error_message)
{
    const RendererBackendVTable* vtable = texture_dispatch_vtable(texture);
    if (vtable == nullptr
        || vtable->quiesce_texture_preview_submission == nullptr) {
        error_message.clear();
        return true;
    }
    return vtable->quiesce_texture_preview_submission(renderer_state, texture,
                                                      error_message);
}

bool
renderer_setup_instance(RendererState& renderer_state,
                        ImVector<const char*>& instance_extensions,
                        std::string& error_message)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable == nullptr || vtable->setup_instance == nullptr) {
        error_message = "renderer backend is unavailable";
        return false;
    }
    return vtable->setup_instance(renderer_state, instance_extensions,
                                  error_message);
}

bool
renderer_setup_device(RendererState& renderer_state, std::string& error_message)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable == nullptr || vtable->setup_device == nullptr) {
        error_message = "renderer backend is unavailable";
        return false;
    }
    return vtable->setup_device(renderer_state, error_message);
}

bool
renderer_setup_window(RendererState& renderer_state, int width, int height,
                      std::string& error_message)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable == nullptr || vtable->setup_window == nullptr) {
        error_message = "renderer backend is unavailable";
        return false;
    }
    return vtable->setup_window(renderer_state, width, height, error_message);
}

bool
renderer_create_surface(RendererState& renderer_state, GLFWwindow* window,
                        std::string& error_message)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable == nullptr || vtable->create_surface == nullptr) {
        error_message = "renderer backend is unavailable";
        return false;
    }
    return vtable->create_surface(renderer_state, window, error_message);
}

void
renderer_destroy_surface(RendererState& renderer_state)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->destroy_surface != nullptr)
        vtable->destroy_surface(renderer_state);
}

void
renderer_cleanup_window(RendererState& renderer_state)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->cleanup_window != nullptr)
        vtable->cleanup_window(renderer_state);
}

void
renderer_cleanup(RendererState& renderer_state)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->cleanup != nullptr)
        vtable->cleanup(renderer_state);
}

bool
renderer_wait_idle(RendererState& renderer_state, std::string& error_message)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable == nullptr || vtable->wait_idle == nullptr) {
        error_message.clear();
        return true;
    }
    return vtable->wait_idle(renderer_state, error_message);
}

bool
renderer_imgui_init(RendererState& renderer_state, std::string& error_message)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable == nullptr || vtable->imgui_init == nullptr) {
        error_message = "renderer backend is unavailable";
        return false;
    }
    return vtable->imgui_init(renderer_state, error_message);
}

void
renderer_imgui_shutdown(RendererState& renderer_state)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->imgui_shutdown != nullptr)
        vtable->imgui_shutdown();
}

void
renderer_imgui_new_frame(RendererState& renderer_state)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->imgui_new_frame != nullptr)
        vtable->imgui_new_frame(renderer_state);
}

bool
renderer_needs_main_window_resize(RendererState& renderer_state, int width,
                                  int height)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable == nullptr || vtable->needs_main_window_resize == nullptr)
        return false;
    return vtable->needs_main_window_resize(renderer_state, width, height);
}

void
renderer_resize_main_window(RendererState& renderer_state, int width,
                            int height)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->resize_main_window != nullptr)
        vtable->resize_main_window(renderer_state, width, height);
}

void
renderer_set_main_clear_color(RendererState& renderer_state, float r, float g,
                              float b, float a)
{
    renderer_state.clear_color[0] = r;
    renderer_state.clear_color[1] = g;
    renderer_state.clear_color[2] = b;
    renderer_state.clear_color[3] = a;
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->set_main_clear_color != nullptr)
        vtable->set_main_clear_color(renderer_state, r, g, b, a);
}

void
renderer_prepare_platform_windows(RendererState& renderer_state)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->prepare_platform_windows != nullptr)
        vtable->prepare_platform_windows(renderer_state);
}

void
renderer_finish_platform_windows(RendererState& renderer_state)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->finish_platform_windows != nullptr)
        vtable->finish_platform_windows(renderer_state);
}

void
renderer_frame_render(RendererState& renderer_state, ImDrawData* draw_data)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->frame_render != nullptr)
        vtable->frame_render(renderer_state, draw_data);
}

void
renderer_frame_present(RendererState& renderer_state)
{
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable != nullptr && vtable->frame_present != nullptr)
        vtable->frame_present(renderer_state);
}

bool
renderer_screen_capture(ImGuiID viewport_id, int x, int y, int w, int h,
                        unsigned int* pixels, void* user_data)
{
    if (user_data == nullptr)
        return false;
    const RendererState& renderer_state = *static_cast<const RendererState*>(
        user_data);
    const RendererBackendVTable* vtable = renderer_dispatch_vtable(
        renderer_state);
    if (vtable == nullptr || vtable->screen_capture == nullptr)
        return false;
    return vtable->screen_capture(viewport_id, x, y, w, h, pixels, user_data);
}

}  // namespace Imiv
