// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_renderer_backend.h"

#include "imiv_platform_glfw.h"

#include <imgui_impl_metal.h>

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

namespace Imiv {

struct RendererBackendState {
    GLFWwindow* window                           = nullptr;
    id<MTLDevice> device                         = nil;
    id<MTLCommandQueue> command_queue            = nil;
    CAMetalLayer* layer                          = nil;
    MTLRenderPassDescriptor* render_pass         = nil;
    id<CAMetalDrawable> current_drawable         = nil;
    id<MTLCommandBuffer> current_command_buffer  = nil;
    id<MTLRenderCommandEncoder> current_encoder  = nil;
    bool imgui_initialized                       = false;
};

namespace {

RendererBackendState*
backend_state(RendererState& renderer_state)
{
    return static_cast<RendererBackendState*>(renderer_state.backend);
}

bool
ensure_backend_state(RendererState& renderer_state)
{
    if (renderer_state.backend != nullptr)
        return true;
    renderer_state.backend = new RendererBackendState();
    return renderer_state.backend != nullptr;
}

void
update_drawable_size(RendererState& renderer_state)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr || state->window == nullptr || state->layer == nil)
        return;
    int width  = 0;
    int height = 0;
    platform_glfw_get_framebuffer_size(state->window, width, height);
    renderer_state.framebuffer_width  = width;
    renderer_state.framebuffer_height = height;
    state->layer.drawableSize         = CGSizeMake(width, height);
}

}  // namespace

bool
renderer_backend_get_viewer_texture_refs(const ViewerState& viewer,
                                         const PlaceholderUiState& ui_state,
                                         ImTextureRef& main_texture_ref,
                                         bool& has_main_texture,
                                         ImTextureRef& closeup_texture_ref,
                                         bool& has_closeup_texture)
{
    (void)viewer;
    (void)ui_state;
    (void)main_texture_ref;
    (void)has_main_texture;
    (void)closeup_texture_ref;
    (void)has_closeup_texture;
    return false;
}

bool
renderer_backend_create_texture(RendererState& renderer_state,
                                const LoadedImage& image,
                                RendererTexture& texture,
                                std::string& error_message)
{
    (void)renderer_state;
    (void)image;
    (void)texture;
    error_message = "Metal image upload is not implemented yet";
    return false;
}

void
renderer_backend_destroy_texture(RendererState& renderer_state,
                                 RendererTexture& texture)
{
    (void)renderer_state;
    (void)texture;
}

bool
renderer_backend_update_preview_texture(RendererState& renderer_state,
                                        RendererTexture& texture,
                                        const LoadedImage* image,
                                        const PlaceholderUiState& ui_state,
                                        const RendererPreviewControls& controls,
                                        std::string& error_message)
{
    (void)renderer_state;
    (void)texture;
    (void)image;
    (void)ui_state;
    (void)controls;
    error_message = "Metal preview rendering is not implemented yet";
    return false;
}

bool
renderer_backend_quiesce_texture_preview_submission(
    RendererState& renderer_state, RendererTexture& texture,
    std::string& error_message)
{
    (void)renderer_state;
    (void)texture;
    error_message.clear();
    return true;
}

bool
renderer_backend_setup_instance(RendererState& renderer_state,
                                ImVector<const char*>& instance_extensions,
                                std::string& error_message)
{
    (void)instance_extensions;
    if (!ensure_backend_state(renderer_state)) {
        error_message = "failed to allocate Metal renderer state";
        return false;
    }
    error_message.clear();
    return true;
}

bool
renderer_backend_create_surface(RendererState& renderer_state,
                                GLFWwindow* window,
                                std::string& error_message)
{
    if (!ensure_backend_state(renderer_state)) {
        error_message = "failed to allocate Metal renderer state";
        return false;
    }
    backend_state(renderer_state)->window = window;
    error_message.clear();
    return true;
}

bool
renderer_backend_setup_device(RendererState& renderer_state,
                              std::string& error_message)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr) {
        error_message = "Metal renderer state is not initialized";
        return false;
    }
    state->device = MTLCreateSystemDefaultDevice();
    if (state->device == nil) {
        error_message = "MTLCreateSystemDefaultDevice failed";
        return false;
    }
    state->command_queue = [state->device newCommandQueue];
    if (state->command_queue == nil) {
        error_message = "failed to create Metal command queue";
        return false;
    }
    error_message.clear();
    return true;
}

bool
renderer_backend_setup_window(RendererState& renderer_state, int width,
                              int height, std::string& error_message)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr || state->window == nullptr || state->device == nil) {
        error_message = "Metal window/device is not initialized";
        return false;
    }
    NSWindow* ns_window = glfwGetCocoaWindow(state->window);
    if (ns_window == nil) {
        error_message = "failed to get Cocoa window from GLFW";
        return false;
    }
    state->layer             = [CAMetalLayer layer];
    state->layer.device      = state->device;
    state->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    ns_window.contentView.layer      = state->layer;
    ns_window.contentView.wantsLayer = YES;
    state->render_pass = [MTLRenderPassDescriptor new];
    renderer_state.framebuffer_width  = width;
    renderer_state.framebuffer_height = height;
    update_drawable_size(renderer_state);
    error_message.clear();
    return true;
}

void
renderer_backend_destroy_surface(RendererState& renderer_state)
{
    if (RendererBackendState* state = backend_state(renderer_state))
        state->window = nullptr;
}

void
renderer_backend_cleanup_window(RendererState& renderer_state)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr)
        return;
    state->current_drawable        = nil;
    state->current_command_buffer  = nil;
    state->current_encoder         = nil;
}

void
renderer_backend_cleanup(RendererState& renderer_state)
{
    delete backend_state(renderer_state);
    renderer_state.backend = nullptr;
}

bool
renderer_backend_wait_idle(RendererState& renderer_state,
                           std::string& error_message)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state != nullptr && state->current_command_buffer != nil)
        [state->current_command_buffer waitUntilCompleted];
    error_message.clear();
    return true;
}

bool
renderer_backend_imgui_init(RendererState& renderer_state,
                            std::string& error_message)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr || state->device == nil) {
        error_message = "Metal device is not initialized";
        return false;
    }
    ImGui_ImplMetal_Init(state->device);
    state->imgui_initialized = true;
    error_message.clear();
    return true;
}

void
renderer_backend_imgui_shutdown()
{
    ImGui_ImplMetal_Shutdown();
}

void
renderer_backend_imgui_new_frame(RendererState& renderer_state)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr || state->layer == nil || state->render_pass == nil)
        return;
    update_drawable_size(renderer_state);
    state->current_drawable = [state->layer nextDrawable];
    if (state->current_drawable == nil)
        return;
    state->render_pass.colorAttachments[0].texture
        = state->current_drawable.texture;
    state->render_pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    state->render_pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    state->render_pass.colorAttachments[0].clearColor = MTLClearColorMake(
        renderer_state.clear_color[0] * renderer_state.clear_color[3],
        renderer_state.clear_color[1] * renderer_state.clear_color[3],
        renderer_state.clear_color[2] * renderer_state.clear_color[3],
        renderer_state.clear_color[3]);
    ImGui_ImplMetal_NewFrame(state->render_pass);
}

bool
renderer_backend_needs_main_window_resize(RendererState& renderer_state,
                                          int width, int height)
{
    return renderer_state.framebuffer_width != width
           || renderer_state.framebuffer_height != height;
}

void
renderer_backend_resize_main_window(RendererState& renderer_state, int width,
                                    int height)
{
    renderer_state.framebuffer_width  = width;
    renderer_state.framebuffer_height = height;
    update_drawable_size(renderer_state);
}

void
renderer_backend_set_main_clear_color(RendererState& renderer_state, float r,
                                      float g, float b, float a)
{
    renderer_state.clear_color[0] = r;
    renderer_state.clear_color[1] = g;
    renderer_state.clear_color[2] = b;
    renderer_state.clear_color[3] = a;
}

void
renderer_backend_prepare_platform_windows(RendererState& renderer_state)
{
    (void)renderer_state;
}

void
renderer_backend_finish_platform_windows(RendererState& renderer_state)
{
    (void)renderer_state;
}

void
renderer_backend_frame_render(RendererState& renderer_state,
                              ImDrawData* draw_data)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr || state->current_drawable == nil
        || state->command_queue == nil || state->render_pass == nil) {
        return;
    }
    state->current_command_buffer = [state->command_queue commandBuffer];
    state->current_encoder = [state->current_command_buffer
        renderCommandEncoderWithDescriptor:state->render_pass];
    ImGui_ImplMetal_RenderDrawData(draw_data, state->current_command_buffer,
                                   state->current_encoder);
}

void
renderer_backend_frame_present(RendererState& renderer_state)
{
    RendererBackendState* state = backend_state(renderer_state);
    if (state == nullptr || state->current_command_buffer == nil
        || state->current_encoder == nil || state->current_drawable == nil) {
        return;
    }
    [state->current_encoder endEncoding];
    [state->current_command_buffer presentDrawable:state->current_drawable];
    [state->current_command_buffer commit];
    state->current_encoder        = nil;
    state->current_command_buffer = nil;
    state->current_drawable       = nil;
}

bool
renderer_backend_screen_capture(ImGuiID viewport_id, int x, int y, int w,
                                int h, unsigned int* pixels, void* user_data)
{
    (void)viewport_id;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)pixels;
    (void)user_data;
    return false;
}

}  // namespace Imiv
