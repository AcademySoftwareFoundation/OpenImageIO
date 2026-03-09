// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_app.h"
#include "imiv_file_dialog.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <imgui.h>

#if defined(IMIV_BACKEND_VULKAN_GLFW)
#    include <imgui_impl_glfw.h>
#    include <imgui_impl_vulkan.h>
#    define GLFW_INCLUDE_NONE
#    define GLFW_INCLUDE_VULKAN
#    include <GLFW/glfw3.h>
#endif

#if defined(IMGUI_ENABLE_TEST_ENGINE)
#    include <imgui_te_context.h>
#    include <imgui_te_engine.h>
#    include <imgui_te_exporters.h>
#    include <imgui_te_ui.h>
#endif

#include <OpenImageIO/half.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>

using namespace OIIO;

namespace Imiv {

namespace {

    constexpr const char* k_dockspace_host_title = "imiv DockSpace";
    constexpr const char* k_image_window_title   = "Image";
    constexpr const char* k_imiv_prefs_filename  = "imiv_prefs.ini";
    constexpr size_t k_max_recent_images         = 16;

    enum class UploadDataType : uint32_t {
        UInt8   = 0,
        UInt16  = 1,
        UInt32  = 2,
        Half    = 3,
        Float   = 4,
        Double  = 5,
        Unknown = 255
    };

    size_t upload_data_type_size(UploadDataType type)
    {
        switch (type) {
        case UploadDataType::UInt8: return 1;
        case UploadDataType::UInt16: return 2;
        case UploadDataType::UInt32: return 4;
        case UploadDataType::Half: return 2;
        case UploadDataType::Float: return 4;
        case UploadDataType::Double: return 8;
        default: break;
        }
        return 0;
    }

    const char* upload_data_type_name(UploadDataType type)
    {
        switch (type) {
        case UploadDataType::UInt8: return "u8";
        case UploadDataType::UInt16: return "u16";
        case UploadDataType::UInt32: return "u32";
        case UploadDataType::Half: return "half";
        case UploadDataType::Float: return "float";
        case UploadDataType::Double: return "double";
        default: break;
        }
        return "unknown";
    }

    TypeDesc upload_data_type_to_typedesc(UploadDataType type)
    {
        switch (type) {
        case UploadDataType::UInt8: return TypeUInt8;
        case UploadDataType::UInt16: return TypeUInt16;
        case UploadDataType::UInt32: return TypeUInt32;
        case UploadDataType::Half: return TypeHalf;
        case UploadDataType::Float: return TypeFloat;
        case UploadDataType::Double: return TypeDesc::DOUBLE;
        default: break;
        }
        return TypeUnknown;
    }

    bool map_spec_type_to_upload(TypeDesc spec_type,
                                 UploadDataType& upload_type,
                                 TypeDesc& read_format)
    {
        const TypeDesc::BASETYPE base = static_cast<TypeDesc::BASETYPE>(
            spec_type.basetype);
        if (base == TypeDesc::UINT8) {
            upload_type = UploadDataType::UInt8;
            read_format = TypeUInt8;
            return true;
        }
        if (base == TypeDesc::UINT16) {
            upload_type = UploadDataType::UInt16;
            read_format = TypeUInt16;
            return true;
        }
        if (base == TypeDesc::UINT32) {
            upload_type = UploadDataType::UInt32;
            read_format = TypeUInt32;
            return true;
        }
        if (base == TypeDesc::HALF) {
            upload_type = UploadDataType::Half;
            read_format = TypeHalf;
            return true;
        }
        if (base == TypeDesc::FLOAT) {
            upload_type = UploadDataType::Float;
            read_format = TypeFloat;
            return true;
        }
        if (base == TypeDesc::DOUBLE) {
            upload_type = UploadDataType::Double;
            read_format = TypeDesc::DOUBLE;
            return true;
        }
        return false;
    }

    struct LoadedImage {
        std::string path;
        int width              = 0;
        int height             = 0;
        int orientation        = 1;  // EXIF orientation [1..8]
        int nchannels          = 0;
        int subimage           = 0;
        int miplevel           = 0;
        int nsubimages         = 1;
        int nmiplevels         = 1;
        UploadDataType type    = UploadDataType::Unknown;
        size_t channel_bytes   = 0;
        size_t row_pitch_bytes = 0;
        std::vector<unsigned char> pixels;
        std::vector<std::string> channel_names;
        std::vector<std::pair<std::string, std::string>> longinfo_rows;
    };

#if defined(IMIV_BACKEND_VULKAN_GLFW)

    struct PreviewControls {
        float exposure           = 0.0f;
        float gamma              = 1.0f;
        float offset             = 0.0f;
        int color_mode           = 0;
        int channel              = 0;
        int use_ocio             = 0;
        int orientation          = 1;
        int linear_interpolation = 0;
    };

    struct VulkanTexture {
        VkImage source_image         = VK_NULL_HANDLE;
        VkImageView source_view      = VK_NULL_HANDLE;
        VkDeviceMemory source_memory = VK_NULL_HANDLE;

        VkImage image         = VK_NULL_HANDLE;  // preview/output image
        VkImageView view      = VK_NULL_HANDLE;  // preview/output view
        VkDeviceMemory memory = VK_NULL_HANDLE;  // preview/output memory

        VkFramebuffer preview_framebuffer     = VK_NULL_HANDLE;
        VkDescriptorSet preview_source_set    = VK_NULL_HANDLE;
        VkSampler sampler                     = VK_NULL_HANDLE;
        VkSampler nearest_mag_sampler         = VK_NULL_HANDLE;
        VkSampler pixelview_sampler           = VK_NULL_HANDLE;
        VkDescriptorSet set                   = VK_NULL_HANDLE;
        VkDescriptorSet nearest_mag_set       = VK_NULL_HANDLE;
        VkDescriptorSet pixelview_set         = VK_NULL_HANDLE;
        int width                             = 0;
        int height                            = 0;
        bool preview_initialized              = false;
        bool preview_dirty                    = false;
        bool preview_params_valid             = false;
        PreviewControls last_preview_controls = {};
    };

    struct UploadComputePushConstants {
        uint32_t width           = 0;
        uint32_t height          = 0;
        uint32_t row_pitch_bytes = 0;
        uint32_t pixel_stride    = 0;
        uint32_t channel_count   = 0;
        uint32_t data_type       = 0;
    };

    struct PreviewPushConstants {
        float exposure      = 0.0f;
        float gamma         = 1.0f;
        float offset        = 0.0f;
        int32_t color_mode  = 0;
        int32_t channel     = 0;
        int32_t use_ocio    = 0;
        int32_t orientation = 1;
    };

    struct VulkanState {
        VkAllocationCallbacks* allocator         = nullptr;
        VkInstance instance                      = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
        uint32_t api_version                     = VK_API_VERSION_1_0;
        VkPhysicalDevice physical_device         = VK_NULL_HANDLE;
        VkDevice device                          = VK_NULL_HANDLE;
        uint32_t queue_family                    = static_cast<uint32_t>(-1);
        VkQueueFamilyProperties queue_family_properties = {};
        VkQueue queue                                   = VK_NULL_HANDLE;
        VkPipelineCache pipeline_cache                  = VK_NULL_HANDLE;
        VkDescriptorPool descriptor_pool                = VK_NULL_HANDLE;
        VkSurfaceKHR surface                            = VK_NULL_HANDLE;
        ImGui_ImplVulkanH_Window window_data;
        uint32_t min_image_count                 = 2;
        bool swapchain_rebuild                   = false;
        bool validation_layer_enabled            = false;
        bool debug_utils_enabled                 = false;
        bool verbose_logging                     = false;
        bool verbose_validation_output           = false;
        bool log_imgui_texture_updates           = false;
        bool queue_requires_full_image_copies    = false;
        bool warned_about_full_imgui_uploads     = false;
        bool compute_upload_ready                = false;
        bool compute_supports_float64            = false;
        VkFormat compute_output_format           = VK_FORMAT_UNDEFINED;
        VkDescriptorPool compute_descriptor_pool = VK_NULL_HANDLE;
        VkDescriptorSetLayout compute_descriptor_set_layout = VK_NULL_HANDLE;
        VkPipelineLayout compute_pipeline_layout            = VK_NULL_HANDLE;
        VkPipeline compute_pipeline                         = VK_NULL_HANDLE;
        VkPipeline compute_pipeline_fp64                    = VK_NULL_HANDLE;

        VkDescriptorPool preview_descriptor_pool            = VK_NULL_HANDLE;
        VkDescriptorSetLayout preview_descriptor_set_layout = VK_NULL_HANDLE;
        VkPipelineLayout preview_pipeline_layout            = VK_NULL_HANDLE;
        VkPipeline preview_pipeline                         = VK_NULL_HANDLE;
        VkRenderPass preview_render_pass                    = VK_NULL_HANDLE;
        PFN_vkSetDebugUtilsObjectNameEXT set_debug_object_name_fn = nullptr;
    };

#endif

    enum class ImageSortMode : uint8_t {
        ByName      = 0,
        ByPath      = 1,
        ByImageDate = 2,
        ByFileDate  = 3
    };

    struct ViewerState {
        LoadedImage image;
        std::string status_message;
        std::string last_error;
        float zoom               = 1.0f;
        bool fit_request         = true;
        ImVec2 scroll            = ImVec2(0.0f, 0.0f);
        ImVec2 norm_scroll       = ImVec2(0.5f, 0.5f);
        ImVec2 max_scroll        = ImVec2(0.0f, 0.0f);
        ImVec2 zoom_pivot_screen = ImVec2(0.0f,
                                          0.0f);  // screen-space pivot anchor
        ImVec2 zoom_pivot_source_uv
            = ImVec2(0.5f, 0.5f);  // original image normalized [0..1]
        bool zoom_pivot_pending     = false;
        int zoom_pivot_frames_left  = 0;
        int scroll_sync_frames_left = 0;
        std::vector<std::string> sibling_images;
        int sibling_index = -1;
        std::string toggle_image_path;
        std::vector<std::string> recent_images;
        ImageSortMode sort_mode = ImageSortMode::ByName;
        bool sort_reverse       = false;
        bool probe_valid        = false;
        int probe_x             = 0;
        int probe_y             = 0;
        std::vector<double> probe_channels;
        bool fullscreen_applied        = false;
        int windowed_x                 = 100;
        int windowed_y                 = 100;
        int windowed_width             = 1600;
        int windowed_height            = 900;
        double slide_last_advance_time = 0.0;
#if defined(IMIV_BACKEND_VULKAN_GLFW)
        VulkanTexture texture;
#endif
    };



    struct PlaceholderUiState {
        bool show_info_window         = false;
        bool show_preferences_window  = false;
        bool show_preview_window      = false;
        bool show_pixelview_window    = false;
        bool show_area_probe_window   = false;
        bool show_window_guides       = false;
        bool show_mouse_mode_selector = false;
        bool fit_image_to_window      = false;
        bool full_screen_mode         = false;
        bool slide_show_running       = false;
        bool slide_loop               = true;
        bool use_ocio                 = false;
        bool pixelview_follows_mouse  = false;
        bool pixelview_left_corner    = true;
        bool linear_interpolation     = false;
        bool dark_palette             = true;
        bool auto_mipmap              = false;
        bool image_window_force_dock  = true;

        int max_memory_ic_mb       = 2048;
        int slide_duration_seconds = 10;
        int closeup_pixels         = 13;
        int closeup_avg_pixels     = 11;
        int current_channel        = 0;
        int subimage_index         = 0;
        int miplevel_index         = 0;
        int color_mode             = 0;
        int mouse_mode             = 0;

        float exposure = 0.0f;
        float gamma    = 1.0f;
        float offset   = 0.0f;

        std::string ocio_display           = "default";
        std::string ocio_view              = "default";
        std::string ocio_image_color_space = "auto";
    };

    struct AppFonts {
        ImFont* ui   = nullptr;
        ImFont* mono = nullptr;
    };

    void refresh_sibling_images(ViewerState& viewer);
    bool pick_sibling_image(const ViewerState& viewer, int delta,
                            std::string& out_path);
    void sort_sibling_images(ViewerState& viewer);
    int clamp_orientation(int orientation);
    void add_recent_image_path(ViewerState& viewer, const std::string& path);
    void clamp_placeholder_ui_state(PlaceholderUiState& ui_state);
    bool load_persistent_state(PlaceholderUiState& ui_state,
                               ViewerState& viewer, std::string& error_message);
    bool save_persistent_state(const PlaceholderUiState& ui_state,
                               const ViewerState& viewer,
                               std::string& error_message);
    void oriented_image_dimensions(const LoadedImage& image, int& out_width,
                                   int& out_height);

    void reset_view_navigation_state(ViewerState& viewer)
    {
        viewer.scroll                  = ImVec2(0.0f, 0.0f);
        viewer.norm_scroll             = ImVec2(0.5f, 0.5f);
        viewer.max_scroll              = ImVec2(0.0f, 0.0f);
        viewer.zoom_pivot_screen       = ImVec2(0.0f, 0.0f);
        viewer.zoom_pivot_source_uv    = ImVec2(0.5f, 0.5f);
        viewer.zoom_pivot_pending      = false;
        viewer.zoom_pivot_frames_left  = 0;
        viewer.scroll_sync_frames_left = 2;
    }

    std::filesystem::path executable_directory_path()
    {
        const std::string program_path = Sysutil::this_program_path();
        if (program_path.empty())
            return std::filesystem::path();
        return std::filesystem::path(program_path).parent_path();
    }

    ImFont* load_font_if_present(const std::filesystem::path& path,
                                 float size_pixels)
    {
        std::error_code ec;
        if (path.empty() || !std::filesystem::exists(path, ec) || ec)
            return nullptr;
        ImGuiIO& io = ImGui::GetIO();
        return io.Fonts->AddFontFromFileTTF(path.string().c_str(), size_pixels);
    }

    AppFonts setup_app_fonts(bool verbose_logging)
    {
        AppFonts fonts;
        ImGuiIO& io = ImGui::GetIO();

        const std::filesystem::path font_root = executable_directory_path()
                                                / "fonts";
        fonts.ui   = load_font_if_present(font_root / "Droid_Sans"
                                              / "DroidSans.ttf",
                                          16.0f);
        fonts.mono = load_font_if_present(font_root / "Droid_Sans_Mono"
                                              / "DroidSansMono.ttf",
                                          16.0f);
        if (!fonts.ui)
            fonts.ui = io.Fonts->AddFontDefault();
        if (!fonts.mono)
            fonts.mono = fonts.ui;
        io.FontDefault = fonts.ui;

        if (verbose_logging) {
            print("imiv: ui font={} mono font={}\n",
                  fonts.ui ? "ready" : "missing",
                  fonts.mono ? "ready" : "missing");
        }
        return fonts;
    }



    char to_lower_ascii(char c)
    {
        if (c >= 'A' && c <= 'Z')
            return static_cast<char>(c - 'A' + 'a');
        return c;
    }



    bool streq_i_ascii(const char* a, const char* b)
    {
        if (a == nullptr || b == nullptr)
            return false;
        while (*a && *b) {
            if (to_lower_ascii(*a) != to_lower_ascii(*b))
                return false;
            ++a;
            ++b;
        }
        return (*a == '\0' && *b == '\0');
    }



    bool read_env_value(const char* name, std::string& out_value)
    {
        out_value.clear();
#if defined(_WIN32)
        char* value       = nullptr;
        size_t value_size = 0;
        errno_t err       = _dupenv_s(&value, &value_size, name);
        if (err != 0 || value == nullptr || value_size == 0) {
            if (value != nullptr)
                std::free(value);
            return false;
        }
        out_value.assign(value);
        std::free(value);
#else
        const char* value = std::getenv(name);
        if (value == nullptr)
            return false;
        out_value.assign(value);
#endif
        return true;
    }



    bool env_flag_is_truthy(const char* name)
    {
        std::string value;
        if (!read_env_value(name, value))
            return false;

        const char* v = value.c_str();
        while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r')
            ++v;
        if (*v == '\0')
            return false;
        if (*v == '1')
            return true;
        if (*v == '0')
            return false;
        return streq_i_ascii(v, "true") || streq_i_ascii(v, "yes")
               || streq_i_ascii(v, "on");
    }



    int env_int_value(const char* name, int fallback)
    {
        std::string value;
        if (!read_env_value(name, value) || value.empty())
            return fallback;
        char* end = nullptr;
        long x    = std::strtol(value.c_str(), &end, 10);
        if (end == value.c_str())
            return fallback;
        if (x < 0)
            return 0;
        if (x > 1000000)
            return 1000000;
        return static_cast<int>(x);
    }

    bool env_var_is_nonempty(const char* name)
    {
        std::string value;
        return read_env_value(name, value) && !value.empty();
    }



    enum class GlfwPlatformPreference : uint8_t {
        Auto    = 0,
        X11     = 1,
        Wayland = 2
    };



    GlfwPlatformPreference glfw_platform_preference()
    {
        std::string value;
        if (!read_env_value("IMIV_GLFW_PLATFORM", value) || value.empty())
            return GlfwPlatformPreference::Auto;
        if (streq_i_ascii(value.c_str(), "x11"))
            return GlfwPlatformPreference::X11;
        if (streq_i_ascii(value.c_str(), "wayland"))
            return GlfwPlatformPreference::Wayland;
        return GlfwPlatformPreference::Auto;
    }



    const char* glfw_platform_name(int platform)
    {
#if defined(IMIV_BACKEND_VULKAN_GLFW) && defined(GLFW_VERSION_MAJOR) \
    && ((GLFW_VERSION_MAJOR > 3)                                     \
        || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4))
        switch (platform) {
#    if defined(GLFW_PLATFORM_WIN32)
        case GLFW_PLATFORM_WIN32: return "win32";
#    endif
#    if defined(GLFW_PLATFORM_COCOA)
        case GLFW_PLATFORM_COCOA: return "cocoa";
#    endif
#    if defined(GLFW_PLATFORM_WAYLAND)
        case GLFW_PLATFORM_WAYLAND: return "wayland";
#    endif
#    if defined(GLFW_PLATFORM_X11)
        case GLFW_PLATFORM_X11: return "x11";
#    endif
#    if defined(GLFW_PLATFORM_NULL)
        case GLFW_PLATFORM_NULL: return "null";
#    endif
        default: break;
        }
#else
        (void)platform;
#endif
        return "unknown";
    }



    void configure_glfw_platform_preference(bool verbose_logging)
    {
#if defined(IMIV_BACKEND_VULKAN_GLFW) && !defined(_WIN32) \
    && !defined(__APPLE__) && defined(GLFW_VERSION_MAJOR) \
    && ((GLFW_VERSION_MAJOR > 3)                          \
        || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4))
        const GlfwPlatformPreference preference = glfw_platform_preference();
        if (preference == GlfwPlatformPreference::X11) {
#    if defined(GLFW_PLATFORM) && defined(GLFW_PLATFORM_X11)
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
            if (verbose_logging)
                print("imiv: GLFW platform preference = x11\n");
#    endif
            return;
        }
        if (preference == GlfwPlatformPreference::Wayland) {
#    if defined(GLFW_PLATFORM) && defined(GLFW_PLATFORM_WAYLAND)
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
            if (verbose_logging)
                print("imiv: GLFW platform preference = wayland\n");
#    endif
            return;
        }

        const bool have_x11_display     = env_var_is_nonempty("DISPLAY");
        const bool have_wayland_display = env_var_is_nonempty(
            "WAYLAND_DISPLAY");
        if (have_x11_display && have_wayland_display) {
#    if defined(GLFW_PLATFORM) && defined(GLFW_PLATFORM_X11)
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
            if (verbose_logging) {
                print("imiv: GLFW auto-selected x11 for platform windows "
                      "(DISPLAY and WAYLAND_DISPLAY are both present)\n");
            }
#    endif
        }
#endif
    }

    bool env_read_float_value(const char* name, float& out);

#if defined(IMGUI_ENABLE_TEST_ENGINE)
    int g_layout_dump_synthetic_item_counter = 0;
    struct TestEngineMouseSpaceState {
        bool viewport_valid = false;
        bool image_valid    = false;
        ImVec2 viewport_min = ImVec2(0.0f, 0.0f);
        ImVec2 viewport_max = ImVec2(0.0f, 0.0f);
        ImVec2 image_min    = ImVec2(0.0f, 0.0f);
        ImVec2 image_max    = ImVec2(0.0f, 0.0f);
    };
    TestEngineMouseSpaceState g_test_engine_mouse_space;

    bool layout_dump_items_enabled()
    {
        return env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP_ITEMS");
    }

    void reset_layout_dump_synthetic_items()
    {
        g_layout_dump_synthetic_item_counter = 0;
    }

    void reset_test_engine_mouse_space()
    {
        g_test_engine_mouse_space = TestEngineMouseSpaceState();
    }

    void update_test_engine_mouse_space(const ImVec2& viewport_min,
                                        const ImVec2& viewport_max,
                                        const ImVec2& image_min,
                                        const ImVec2& image_max)
    {
        g_test_engine_mouse_space.viewport_valid = true;
        g_test_engine_mouse_space.viewport_min   = viewport_min;
        g_test_engine_mouse_space.viewport_max   = viewport_max;
        g_test_engine_mouse_space.image_valid    = (image_max.x > image_min.x
                                                 && image_max.y > image_min.y);
        g_test_engine_mouse_space.image_min      = image_min;
        g_test_engine_mouse_space.image_max      = image_max;
    }

    ImVec2 test_engine_rect_rel_pos(const ImVec2& rect_min,
                                    const ImVec2& rect_max, float rel_x,
                                    float rel_y)
    {
        const float clamped_x = std::clamp(rel_x, 0.0f, 1.0f);
        const float clamped_y = std::clamp(rel_y, 0.0f, 1.0f);
        return ImVec2(rect_min.x + (rect_max.x - rect_min.x) * clamped_x,
                      rect_min.y + (rect_max.y - rect_min.y) * clamped_y);
    }

    bool resolve_test_engine_mouse_pos(ImVec2& out_pos)
    {
        float rel_x = 0.0f;
        float rel_y = 0.0f;
        if (env_read_float_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_IMAGE_REL_X",
                                 rel_x)
            && env_read_float_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_IMAGE_REL_Y",
                                    rel_y)
            && g_test_engine_mouse_space.image_valid) {
            out_pos
                = test_engine_rect_rel_pos(g_test_engine_mouse_space.image_min,
                                           g_test_engine_mouse_space.image_max,
                                           rel_x, rel_y);
            return true;
        }

        if (env_read_float_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_WINDOW_REL_X",
                                 rel_x)
            && env_read_float_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_WINDOW_REL_Y",
                                    rel_y)
            && g_test_engine_mouse_space.viewport_valid) {
            out_pos = test_engine_rect_rel_pos(
                g_test_engine_mouse_space.viewport_min,
                g_test_engine_mouse_space.viewport_max, rel_x, rel_y);
            return true;
        }

        float mouse_x = 0.0f;
        float mouse_y = 0.0f;
        if (env_read_float_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_X", mouse_x)
            && env_read_float_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_Y", mouse_y)) {
            out_pos = ImVec2(mouse_x, mouse_y);
            return true;
        }
        return false;
    }

    void register_layout_dump_synthetic_item(const char* kind,
                                             const char* label)
    {
        if (!layout_dump_items_enabled())
            return;
        ImGuiContext* ui_ctx = ImGui::GetCurrentContext();
        if (ui_ctx == nullptr)
            return;

        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        if (max.x <= min.x || max.y <= min.y)
            return;

        const int ordinal   = ++g_layout_dump_synthetic_item_counter;
        char id_source[128] = {};
        std::snprintf(id_source, sizeof(id_source), "##imiv_layout_synth_%s_%d",
                      kind ? kind : "item", ordinal);
        const ImGuiID id = ImGui::GetID(id_source);
        if (id == 0)
            return;

        char debug_label[128] = {};
        if (label && label[0] != '\0') {
            std::snprintf(debug_label, sizeof(debug_label), "%s: %s",
                          kind ? kind : "item", label);
        } else {
            std::snprintf(debug_label, sizeof(debug_label), "%s",
                          kind ? kind : "item");
        }

        const ImRect bb(min, max);
        ImGuiTestEngineHook_ItemAdd(ui_ctx, id, bb, nullptr);
        ImGuiTestEngineHook_ItemInfo(ui_ctx, id, debug_label,
                                     ImGuiItemStatusFlags_None);
    }
#else
    void reset_layout_dump_synthetic_items() {}
    void reset_test_engine_mouse_space() {}
    void update_test_engine_mouse_space(const ImVec2&, const ImVec2&,
                                        const ImVec2&, const ImVec2&)
    {
    }
    bool resolve_test_engine_mouse_pos(ImVec2&) { return false; }
    void register_layout_dump_synthetic_item(const char*, const char*) {}
#endif



    template<class T> T read_unaligned_value(const unsigned char* ptr)
    {
        T value = {};
        std::memcpy(&value, ptr, sizeof(T));
        return value;
    }



    std::string channel_label_for_index(int c)
    {
        static const char* names[] = { "R", "G", "B", "A" };
        if (c >= 0 && c < 4)
            return names[c];
        return Strutil::fmt::format("C{}", c);
    }



    std::string pixel_preview_channel_label(const LoadedImage& image, int c)
    {
        if (c >= 0 && c < static_cast<int>(image.channel_names.size())
            && !image.channel_names[c].empty()) {
            return image.channel_names[c];
        }
        return channel_label_for_index(c);
    }



    double probe_value_to_oiio_float(UploadDataType type, double value)
    {
        switch (type) {
        case UploadDataType::UInt8: return value / 255.0;
        case UploadDataType::UInt16: return value / 65535.0;
        case UploadDataType::UInt32: return value / 4294967295.0;
        case UploadDataType::Half:
        case UploadDataType::Float:
        case UploadDataType::Double: return value;
        default: break;
        }
        return value;
    }



    enum class ProbeStatsSemantics : uint8_t { RawStored = 0, OIIOFloat = 1 };



    bool sample_loaded_pixel(const LoadedImage& image, int x, int y,
                             std::vector<double>& out_channels)
    {
        out_channels.clear();
        if (image.width <= 0 || image.height <= 0 || image.nchannels <= 0
            || image.channel_bytes == 0) {
            return false;
        }
        if (x < 0 || y < 0 || x >= image.width || y >= image.height)
            return false;

        const size_t channels  = static_cast<size_t>(image.nchannels);
        const size_t px_stride = channels * image.channel_bytes;
        const size_t row_start = static_cast<size_t>(y) * image.row_pitch_bytes;
        const size_t px_start  = static_cast<size_t>(x) * px_stride;
        const size_t offset    = row_start + px_start;
        if (offset + px_stride > image.pixels.size())
            return false;

        out_channels.resize(channels);
        const unsigned char* src = image.pixels.data() + offset;
        for (size_t c = 0; c < channels; ++c) {
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
                v = static_cast<double>(static_cast<float>(
                    read_unaligned_value<half>(channel_ptr)));
                break;
            case UploadDataType::Float:
                v = static_cast<double>(
                    read_unaligned_value<float>(channel_ptr));
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



    bool sample_loaded_pixel_with_semantics(const LoadedImage& image, int x,
                                            int y,
                                            ProbeStatsSemantics semantics,
                                            std::vector<double>& out_channels)
    {
        if (!sample_loaded_pixel(image, x, y, out_channels))
            return false;
        if (semantics == ProbeStatsSemantics::OIIOFloat) {
            for (double& value : out_channels)
                value = probe_value_to_oiio_float(image.type, value);
        }
        return true;
    }



    bool apply_forced_probe_from_env(ViewerState& viewer)
    {
        const int forced_x = env_int_value("IMIV_IMGUI_TEST_ENGINE_PROBE_X",
                                           -1);
        const int forced_y = env_int_value("IMIV_IMGUI_TEST_ENGINE_PROBE_Y",
                                           -1);
        if (forced_x < 0 || forced_y < 0 || viewer.image.path.empty())
            return false;

        const int px = std::clamp(forced_x, 0,
                                  std::max(0, viewer.image.width - 1));
        const int py = std::clamp(forced_y, 0,
                                  std::max(0, viewer.image.height - 1));
        std::vector<double> sampled;
        if (!sample_loaded_pixel(viewer.image, px, py, sampled))
            return false;

        viewer.probe_valid    = true;
        viewer.probe_x        = px;
        viewer.probe_y        = py;
        viewer.probe_channels = std::move(sampled);
        return true;
    }



    bool compute_area_stats(
        const LoadedImage& image, int center_x, int center_y, int window_size,
        std::vector<double>& out_min, std::vector<double>& out_max,
        std::vector<double>& out_avg, int& out_samples,
        ProbeStatsSemantics semantics = ProbeStatsSemantics::RawStored)
    {
        out_min.clear();
        out_max.clear();
        out_avg.clear();
        out_samples = 0;
        if (image.width <= 0 || image.height <= 0 || image.nchannels <= 0)
            return false;
        if (window_size <= 0)
            return false;
        if ((window_size & 1) == 0)
            ++window_size;

        const int half_window = window_size / 2;
        const int x0          = std::max(0, center_x - half_window);
        const int x1 = std::min(image.width - 1, center_x + half_window);
        const int y0 = std::max(0, center_y - half_window);
        const int y1 = std::min(image.height - 1, center_y + half_window);
        if (x1 < x0 || y1 < y0)
            return false;

        const size_t channels = static_cast<size_t>(image.nchannels);
        out_min.assign(channels, std::numeric_limits<double>::infinity());
        out_max.assign(channels, -std::numeric_limits<double>::infinity());
        out_avg.assign(channels, 0.0);

        std::vector<double> sample;
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                if (!sample_loaded_pixel_with_semantics(image, x, y, semantics,
                                                        sample))
                    continue;
                if (sample.size() != channels)
                    continue;
                for (size_t c = 0; c < channels; ++c) {
                    out_min[c] = std::min(out_min[c], sample[c]);
                    out_max[c] = std::max(out_max[c], sample[c]);
                    out_avg[c] += sample[c];
                }
                ++out_samples;
            }
        }

        if (out_samples <= 0) {
            out_min.clear();
            out_max.clear();
            out_avg.clear();
            return false;
        }
        for (double& v : out_avg)
            v /= static_cast<double>(out_samples);
        return true;
    }



    bool is_ascii_space(char c)
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }



    std::string trim_ascii(const std::string& value)
    {
        size_t begin = 0;
        while (begin < value.size() && is_ascii_space(value[begin]))
            ++begin;
        size_t end = value.size();
        while (end > begin && is_ascii_space(value[end - 1]))
            --end;
        return value.substr(begin, end - begin);
    }



    std::string lower_ascii_copy(const std::string& value)
    {
        std::string out = value;
        for (char& c : out)
            c = to_lower_ascii(c);
        return out;
    }



    ImGuiKey parse_test_engine_key_token(const std::string& token)
    {
        if (token.size() == 1) {
            const char c = token[0];
            if (c >= 'a' && c <= 'z')
                return static_cast<ImGuiKey>(ImGuiKey_A + (c - 'a'));
            if (c >= '0' && c <= '9')
                return static_cast<ImGuiKey>(ImGuiKey_0 + (c - '0'));
        }

        if (token == "comma")
            return ImGuiKey_Comma;
        if (token == "period" || token == "dot")
            return ImGuiKey_Period;
        if (token == "equal" || token == "equals")
            return ImGuiKey_Equal;
        if (token == "minus" || token == "dash")
            return ImGuiKey_Minus;
        if (token == "leftbracket" || token == "[")
            return ImGuiKey_LeftBracket;
        if (token == "rightbracket" || token == "]")
            return ImGuiKey_RightBracket;
        if (token == "pageup")
            return ImGuiKey_PageUp;
        if (token == "pagedown")
            return ImGuiKey_PageDown;
        if (token == "escape" || token == "esc")
            return ImGuiKey_Escape;
        if (token == "delete" || token == "del")
            return ImGuiKey_Delete;
        if (token == "kp0" || token == "keypad0")
            return ImGuiKey_Keypad0;
        if (token == "kpdecimal" || token == "keypaddecimal")
            return ImGuiKey_KeypadDecimal;
        if (token == "kpadd" || token == "keypadadd")
            return ImGuiKey_KeypadAdd;
        if (token == "kpsubtract" || token == "keypadsubtract")
            return ImGuiKey_KeypadSubtract;

        return ImGuiKey_None;
    }



    bool parse_test_engine_key_chord(const std::string& value,
                                     ImGuiKeyChord& out_chord)
    {
        const std::string trimmed = trim_ascii(value);
        if (trimmed.empty())
            return false;

        ImGuiKeyChord chord = 0;
        bool have_key       = false;
        size_t begin        = 0;
        while (begin <= trimmed.size()) {
            const size_t plus = trimmed.find('+', begin);
            const size_t end  = (plus == std::string::npos) ? trimmed.size()
                                                            : plus;
            const std::string token = lower_ascii_copy(
                trim_ascii(trimmed.substr(begin, end - begin)));
            if (token.empty())
                return false;

            if (token == "ctrl" || token == "control") {
                chord |= ImGuiMod_Ctrl;
            } else if (token == "shift") {
                chord |= ImGuiMod_Shift;
            } else if (token == "alt") {
                chord |= ImGuiMod_Alt;
            } else if (token == "super" || token == "cmd" || token == "command"
                       || token == "win" || token == "meta") {
                chord |= ImGuiMod_Super;
            } else {
                const ImGuiKey key = parse_test_engine_key_token(token);
                if (key == ImGuiKey_None || have_key)
                    return false;
                chord |= key;
                have_key = true;
            }

            if (plus == std::string::npos)
                break;
            begin = plus + 1;
        }

        if (!have_key)
            return false;
        out_chord = chord;
        return true;
    }



    bool parse_bool_value(const std::string& value, bool& out)
    {
        const std::string trimmed = trim_ascii(value);
        if (trimmed.empty())
            return false;
        const char* cstr = trimmed.c_str();
        if (streq_i_ascii(cstr, "1") || streq_i_ascii(cstr, "true")
            || streq_i_ascii(cstr, "yes") || streq_i_ascii(cstr, "on")) {
            out = true;
            return true;
        }
        if (streq_i_ascii(cstr, "0") || streq_i_ascii(cstr, "false")
            || streq_i_ascii(cstr, "no") || streq_i_ascii(cstr, "off")) {
            out = false;
            return true;
        }
        return false;
    }



    bool parse_int_value(const std::string& value, int& out)
    {
        const std::string trimmed = trim_ascii(value);
        if (trimmed.empty())
            return false;
        char* end = nullptr;
        long x    = std::strtol(trimmed.c_str(), &end, 10);
        if (end == trimmed.c_str() || *end != '\0')
            return false;
        if (x < std::numeric_limits<int>::min()
            || x > std::numeric_limits<int>::max())
            return false;
        out = static_cast<int>(x);
        return true;
    }



    bool parse_float_value(const std::string& value, float& out)
    {
        const std::string trimmed = trim_ascii(value);
        if (trimmed.empty())
            return false;
        char* end = nullptr;
        float x   = std::strtof(trimmed.c_str(), &end);
        if (end == trimmed.c_str() || *end != '\0')
            return false;
        out = x;
        return true;
    }

    bool env_read_int_value(const char* name, int& out)
    {
        std::string value;
        return read_env_value(name, value) && parse_int_value(value, out);
    }

    bool env_read_float_value(const char* name, float& out)
    {
        std::string value;
        return read_env_value(name, value) && parse_float_value(value, out);
    }

    bool env_read_key_chord_value(const char* name, ImGuiKeyChord& out)
    {
        std::string value;
        return read_env_value(name, value)
               && parse_test_engine_key_chord(value, out);
    }



    std::filesystem::path prefs_file_path()
    {
        return std::filesystem::path(k_imiv_prefs_filename);
    }



    void clamp_placeholder_ui_state(PlaceholderUiState& ui_state)
    {
        auto clamp_odd = [](int value, int min_value, int max_value) {
            int clamped = std::clamp(value, min_value, max_value);
            if ((clamped & 1) == 0) {
                if (clamped < max_value)
                    ++clamped;
                else
                    --clamped;
            }
            return std::clamp(clamped, min_value, max_value);
        };

        if (ui_state.max_memory_ic_mb < 64)
            ui_state.max_memory_ic_mb = 64;
        if (ui_state.slide_duration_seconds < 1)
            ui_state.slide_duration_seconds = 1;
        ui_state.closeup_pixels     = clamp_odd(ui_state.closeup_pixels, 9, 25);
        ui_state.closeup_avg_pixels = clamp_odd(ui_state.closeup_avg_pixels, 3,
                                                25);
        if (ui_state.closeup_avg_pixels > ui_state.closeup_pixels)
            ui_state.closeup_avg_pixels = ui_state.closeup_pixels;
        if (ui_state.current_channel < 0)
            ui_state.current_channel = 0;
        if (ui_state.current_channel > 4)
            ui_state.current_channel = 4;
        if (ui_state.color_mode < 0)
            ui_state.color_mode = 0;
        if (ui_state.color_mode > 4)
            ui_state.color_mode = 4;
        if (ui_state.mouse_mode < 0)
            ui_state.mouse_mode = 0;
        if (ui_state.mouse_mode > 4)
            ui_state.mouse_mode = 4;
        if (ui_state.subimage_index < 0)
            ui_state.subimage_index = 0;
        if (ui_state.miplevel_index < 0)
            ui_state.miplevel_index = 0;
        if (ui_state.gamma < 0.1f)
            ui_state.gamma = 0.1f;
        ui_state.offset = std::clamp(ui_state.offset, -1.0f, 1.0f);
        if (ui_state.ocio_display.empty())
            ui_state.ocio_display = "default";
        if (ui_state.ocio_view.empty())
            ui_state.ocio_view = "default";
        if (ui_state.ocio_image_color_space.empty())
            ui_state.ocio_image_color_space = "auto";
    }

    void reset_per_image_preview_state(PlaceholderUiState& ui_state)
    {
        ui_state.current_channel = 0;
        ui_state.color_mode      = 0;
        ui_state.exposure        = 0.0f;
        ui_state.gamma           = 1.0f;
        ui_state.offset          = 0.0f;
    }



    bool load_persistent_state(PlaceholderUiState& ui_state,
                               ViewerState& viewer, std::string& error_message)
    {
        error_message.clear();
        const std::filesystem::path path = prefs_file_path();
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
        while (std::getline(input, line)) {
            const std::string trimmed = trim_ascii(line);
            if (trimmed.empty() || trimmed[0] == '#')
                continue;

            const size_t eq = trimmed.find('=');
            if (eq == std::string::npos)
                continue;

            const std::string key   = trim_ascii(trimmed.substr(0, eq));
            const std::string value = trimmed.substr(eq + 1);
            bool bool_value         = false;
            int int_value           = 0;

            if (key == "pixelview_follows_mouse") {
                if (parse_bool_value(value, bool_value))
                    ui_state.pixelview_follows_mouse = bool_value;
            } else if (key == "pixelview_left_corner") {
                if (parse_bool_value(value, bool_value))
                    ui_state.pixelview_left_corner = bool_value;
            } else if (key == "linear_interpolation") {
                if (parse_bool_value(value, bool_value))
                    ui_state.linear_interpolation = bool_value;
            } else if (key == "dark_palette") {
                if (parse_bool_value(value, bool_value))
                    ui_state.dark_palette = bool_value;
            } else if (key == "auto_mipmap") {
                if (parse_bool_value(value, bool_value))
                    ui_state.auto_mipmap = bool_value;
            } else if (key == "fit_image_to_window") {
                if (parse_bool_value(value, bool_value))
                    ui_state.fit_image_to_window = bool_value;
            } else if (key == "show_mouse_mode_selector") {
                if (parse_bool_value(value, bool_value))
                    ui_state.show_mouse_mode_selector = bool_value;
            } else if (key == "full_screen_mode") {
                if (parse_bool_value(value, bool_value))
                    ui_state.full_screen_mode = bool_value;
            } else if (key == "slide_show_running") {
                if (parse_bool_value(value, bool_value))
                    ui_state.slide_show_running = bool_value;
            } else if (key == "slide_loop") {
                if (parse_bool_value(value, bool_value))
                    ui_state.slide_loop = bool_value;
            } else if (key == "use_ocio") {
                if (parse_bool_value(value, bool_value))
                    ui_state.use_ocio = bool_value;
            } else if (key == "max_memory_ic_mb") {
                if (parse_int_value(value, int_value))
                    ui_state.max_memory_ic_mb = int_value;
            } else if (key == "slide_duration_seconds") {
                if (parse_int_value(value, int_value))
                    ui_state.slide_duration_seconds = int_value;
            } else if (key == "closeup_pixels") {
                if (parse_int_value(value, int_value))
                    ui_state.closeup_pixels = int_value;
            } else if (key == "closeup_avg_pixels") {
                if (parse_int_value(value, int_value))
                    ui_state.closeup_avg_pixels = int_value;
            } else if (key == "subimage_index") {
                if (parse_int_value(value, int_value))
                    ui_state.subimage_index = int_value;
            } else if (key == "miplevel_index") {
                if (parse_int_value(value, int_value))
                    ui_state.miplevel_index = int_value;
            } else if (key == "mouse_mode") {
                if (parse_int_value(value, int_value))
                    ui_state.mouse_mode = int_value;
            } else if (key == "ocio_display") {
                ui_state.ocio_display = trim_ascii(value);
            } else if (key == "ocio_view") {
                ui_state.ocio_view = trim_ascii(value);
            } else if (key == "ocio_image_color_space") {
                ui_state.ocio_image_color_space = trim_ascii(value);
            } else if (key == "sort_mode") {
                if (parse_int_value(value, int_value)) {
                    int_value        = std::clamp(int_value, 0, 3);
                    viewer.sort_mode = static_cast<ImageSortMode>(int_value);
                }
            } else if (key == "sort_reverse") {
                if (parse_bool_value(value, bool_value))
                    viewer.sort_reverse = bool_value;
            } else if (key == "recent_image") {
                add_recent_image_path(viewer, trim_ascii(value));
            }
        }

        clamp_placeholder_ui_state(ui_state);
        if (!input.eof()) {
            error_message = Strutil::fmt::format("failed while reading '{}'",
                                                 path.string());
            return false;
        }
        return true;
    }



    bool save_persistent_state(const PlaceholderUiState& ui_state,
                               const ViewerState& viewer,
                               std::string& error_message)
    {
        error_message.clear();
        const std::filesystem::path path      = prefs_file_path();
        const std::filesystem::path temp_path = path.string() + ".tmp";

        std::ofstream output(temp_path, std::ios::trunc);
        if (!output) {
            error_message = Strutil::fmt::format("failed to open '{}'",
                                                 temp_path.string());
            return false;
        }

        output << "# imiv preferences\n";
        output << "pixelview_follows_mouse="
               << (ui_state.pixelview_follows_mouse ? 1 : 0) << "\n";
        output << "pixelview_left_corner="
               << (ui_state.pixelview_left_corner ? 1 : 0) << "\n";
        output << "linear_interpolation="
               << (ui_state.linear_interpolation ? 1 : 0) << "\n";
        output << "dark_palette=" << (ui_state.dark_palette ? 1 : 0) << "\n";
        output << "auto_mipmap=" << (ui_state.auto_mipmap ? 1 : 0) << "\n";
        output << "fit_image_to_window="
               << (ui_state.fit_image_to_window ? 1 : 0) << "\n";
        output << "show_mouse_mode_selector="
               << (ui_state.show_mouse_mode_selector ? 1 : 0) << "\n";
        output << "full_screen_mode=" << (ui_state.full_screen_mode ? 1 : 0)
               << "\n";
        output << "slide_show_running=" << (ui_state.slide_show_running ? 1 : 0)
               << "\n";
        output << "slide_loop=" << (ui_state.slide_loop ? 1 : 0) << "\n";
        output << "use_ocio=" << (ui_state.use_ocio ? 1 : 0) << "\n";
        output << "max_memory_ic_mb=" << ui_state.max_memory_ic_mb << "\n";
        output << "slide_duration_seconds=" << ui_state.slide_duration_seconds
               << "\n";
        output << "closeup_pixels=" << ui_state.closeup_pixels << "\n";
        output << "closeup_avg_pixels=" << ui_state.closeup_avg_pixels << "\n";
        output << "subimage_index=" << ui_state.subimage_index << "\n";
        output << "miplevel_index=" << ui_state.miplevel_index << "\n";
        output << "mouse_mode=" << ui_state.mouse_mode << "\n";
        output << "ocio_display=" << ui_state.ocio_display << "\n";
        output << "ocio_view=" << ui_state.ocio_view << "\n";
        output << "ocio_image_color_space=" << ui_state.ocio_image_color_space
               << "\n";
        output << "sort_mode=" << static_cast<int>(viewer.sort_mode) << "\n";
        output << "sort_reverse=" << (viewer.sort_reverse ? 1 : 0) << "\n";
        for (const std::string& recent : viewer.recent_images)
            output << "recent_image=" << recent << "\n";
        output.flush();
        if (!output) {
            error_message = Strutil::fmt::format("failed while writing '{}'",
                                                 temp_path.string());
            output.close();
            std::error_code rm_ec;
            std::filesystem::remove(temp_path, rm_ec);
            return false;
        }
        output.close();

        std::error_code ec;
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
            std::error_code rm_ec;
            std::filesystem::remove(temp_path, rm_ec);
            return false;
        }
        return true;
    }

#if defined(IMGUI_ENABLE_TEST_ENGINE)
    struct TestEngineConfig {
        bool want_test_engine = false;
        bool trace            = false;
        bool auto_screenshot  = false;
        bool layout_dump      = false;
        bool state_dump       = false;
        bool junit_xml        = false;
        bool automation_mode  = false;
        bool exit_on_finish   = false;
        bool has_work         = false;
        bool show_windows     = false;
        std::string junit_xml_out;
        std::string state_dump_out;
    };

    struct TestEngineRuntime {
        ImGuiTestEngine* engine = nullptr;
        bool request_exit       = false;
        bool show_windows       = false;
    };
#endif


    void glfw_error_callback(int error, const char* description)
    {
        print(stderr, "imiv: GLFW error {}: {}\n", error, description);
    }

    void append_longinfo_row(LoadedImage& image, const char* label,
                             const std::string& value)
    {
        if (label == nullptr || label[0] == '\0')
            return;
        image.longinfo_rows.emplace_back(label, value);
    }

    void build_longinfo_rows(LoadedImage& image, const ImageBuf& source,
                             const ImageSpec& spec)
    {
        image.longinfo_rows.clear();
        if (spec.depth <= 1) {
            append_longinfo_row(image, "Dimensions",
                                Strutil::fmt::format("{} x {} pixels",
                                                     spec.width, spec.height));
        } else {
            append_longinfo_row(image, "Dimensions",
                                Strutil::fmt::format("{} x {} x {} pixels",
                                                     spec.width, spec.height,
                                                     spec.depth));
        }
        append_longinfo_row(image, "Channels",
                            Strutil::fmt::format("{}", spec.nchannels));

        std::string channel_list;
        for (int i = 0; i < spec.nchannels; ++i) {
            if (i > 0)
                channel_list += ", ";
            channel_list += spec.channelnames[i];
        }
        append_longinfo_row(image, "Channel list", channel_list);
        append_longinfo_row(image, "File format",
                            std::string(source.file_format_name()));
        append_longinfo_row(image, "Data format",
                            std::string(spec.format.c_str()));
        append_longinfo_row(image, "Data size",
                            Strutil::fmt::format("{:.2f} MB",
                                                 static_cast<double>(
                                                     spec.image_bytes())
                                                     / (1024.0 * 1024.0)));
        append_longinfo_row(image, "Image origin",
                            Strutil::fmt::format("{}, {}, {}", spec.x, spec.y,
                                                 spec.z));
        append_longinfo_row(image, "Full/display size",
                            Strutil::fmt::format("{} x {} x {}",
                                                 spec.full_width,
                                                 spec.full_height,
                                                 spec.full_depth));
        append_longinfo_row(image, "Full/display origin",
                            Strutil::fmt::format("{}, {}, {}", spec.full_x,
                                                 spec.full_y, spec.full_z));
        if (spec.tile_width) {
            append_longinfo_row(image, "Scanline/tile",
                                Strutil::fmt::format("tiled {} x {} x {}",
                                                     spec.tile_width,
                                                     spec.tile_height,
                                                     spec.tile_depth));
        } else {
            append_longinfo_row(image, "Scanline/tile", "scanline");
        }
        if (spec.alpha_channel >= 0) {
            append_longinfo_row(image, "Alpha channel",
                                Strutil::fmt::format("{}", spec.alpha_channel));
        }
        if (spec.z_channel >= 0) {
            append_longinfo_row(image, "Depth (z) channel",
                                Strutil::fmt::format("{}", spec.z_channel));
        }

        ParamValueList attribs = spec.extra_attribs;
        attribs.sort(false);
        for (auto&& p : attribs) {
            append_longinfo_row(image, p.name().c_str(),
                                spec.metadata_val(p, true));
        }
    }

    bool load_image_for_compute(const std::string& path, int requested_subimage,
                                int requested_miplevel, LoadedImage& image,
                                std::string& error_message)
    {
        ImageBuf source(path);
        if (!source.read(0, 0, true, TypeUnknown)) {
            error_message = source.geterror();
            if (error_message.empty())
                error_message = "failed to read image";
            return false;
        }

        int nsubimages = source.nsubimages();
        if (nsubimages <= 0)
            nsubimages = 1;
        const int resolved_subimage = std::clamp(requested_subimage, 0,
                                                 nsubimages - 1);
        if (resolved_subimage != 0) {
            source.reset(path, resolved_subimage, 0);
            if (!source.read(resolved_subimage, 0, true, TypeUnknown)) {
                error_message = source.geterror();
                if (error_message.empty())
                    error_message = "failed to read subimage";
                return false;
            }
        }

        int nmiplevels = source.nmiplevels();
        if (nmiplevels <= 0)
            nmiplevels = 1;
        const int resolved_miplevel = std::clamp(requested_miplevel, 0,
                                                 nmiplevels - 1);
        if (resolved_miplevel != source.miplevel()) {
            source.reset(path, resolved_subimage, resolved_miplevel);
            if (!source.read(resolved_subimage, resolved_miplevel, true,
                             TypeUnknown)) {
                error_message = source.geterror();
                if (error_message.empty())
                    error_message = "failed to read miplevel";
                return false;
            }
        }

        const ImageSpec& spec = source.spec();
        if (spec.width <= 0 || spec.height <= 0 || spec.nchannels <= 0) {
            error_message = "image has invalid dimensions or channel count";
            return false;
        }

        UploadDataType upload_type = UploadDataType::Unknown;
        TypeDesc read_format       = TypeUnknown;
        if (!map_spec_type_to_upload(spec.format, upload_type, read_format)) {
            upload_type = UploadDataType::Float;
            read_format = TypeFloat;
        }

        const size_t width         = static_cast<size_t>(spec.width);
        const size_t height        = static_cast<size_t>(spec.height);
        const size_t channel_count = static_cast<size_t>(spec.nchannels);
        const size_t channel_bytes = upload_data_type_size(upload_type);
        if (channel_bytes == 0) {
            error_message = "unsupported pixel data type";
            return false;
        }
        const size_t row_pitch  = width * channel_count * channel_bytes;
        const size_t total_size = row_pitch * height;

        std::vector<unsigned char> pixels(total_size);
        if (!source.get_pixels(source.roi(), read_format, pixels.data())) {
            error_message = source.geterror();
            if (error_message.empty())
                error_message = "failed to fetch pixel data";
            return false;
        }

        image.path        = path;
        image.width       = spec.width;
        image.height      = spec.height;
        image.orientation = clamp_orientation(
            spec.get_int_attribute("Orientation", 1));
        image.nchannels       = spec.nchannels;
        image.subimage        = resolved_subimage;
        image.miplevel        = resolved_miplevel;
        image.nsubimages      = nsubimages;
        image.nmiplevels      = nmiplevels;
        image.type            = upload_type;
        image.channel_bytes   = channel_bytes;
        image.row_pitch_bytes = row_pitch;
        image.pixels          = std::move(pixels);
        image.channel_names.clear();
        image.channel_names.reserve(static_cast<size_t>(spec.nchannels));
        for (int c = 0; c < spec.nchannels; ++c)
            image.channel_names.emplace_back(spec.channelnames[c]);
        build_longinfo_rows(image, source, spec);

        if (spec.format != read_format) {
            print(
                stderr,
                "imiv: source format '{}' converted to '{}' for compute upload path\n",
                spec.format.c_str(), read_format.c_str());
        }
        return true;
    }



#if defined(IMIV_BACKEND_VULKAN_GLFW)

    void check_vk_result(VkResult err)
    {
        if (err == VK_SUCCESS)
            return;
        print(stderr, "imiv: Vulkan error {}\n", static_cast<int>(err));
    }



    bool
    is_extension_available(const ImVector<VkExtensionProperties>& properties,
                           const char* extension_name)
    {
        for (const VkExtensionProperties& p : properties) {
            if (std::strcmp(p.extensionName, extension_name) == 0)
                return true;
        }
        return false;
    }



    void append_unique_extension(ImVector<const char*>& extensions,
                                 const char* extension_name)
    {
        for (const char* existing_name : extensions) {
            if (std::strcmp(existing_name, extension_name) == 0)
                return;
        }
        extensions.push_back(extension_name);
    }



    bool is_layer_available(const char* layer_name)
    {
        uint32_t count = 0;
        VkResult err   = vkEnumerateInstanceLayerProperties(&count, nullptr);
        if (err != VK_SUCCESS)
            return false;

        std::vector<VkLayerProperties> properties(count);
        err = vkEnumerateInstanceLayerProperties(&count, properties.data());
        if (err != VK_SUCCESS)
            return false;

        for (const VkLayerProperties& p : properties) {
            if (std::strcmp(p.layerName, layer_name) == 0)
                return true;
        }
        return false;
    }



    uint32_t vulkan_header_api_version()
    {
#    ifdef VK_HEADER_VERSION_COMPLETE
        return VK_HEADER_VERSION_COMPLETE;
#    else
        return VK_API_VERSION_1_0;
#    endif
    }



    uint32_t query_loader_api_version()
    {
        uint32_t version = VK_API_VERSION_1_0;
        PFN_vkEnumerateInstanceVersion enumerate_instance_version
            = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
                vkGetInstanceProcAddr(VK_NULL_HANDLE,
                                      "vkEnumerateInstanceVersion"));
        if (enumerate_instance_version != nullptr) {
            uint32_t loader_version = VK_API_VERSION_1_0;
            if (enumerate_instance_version(&loader_version) == VK_SUCCESS)
                version = loader_version;
        }
        return version;
    }



    uint32_t choose_instance_api_version()
    {
        return std::min(vulkan_header_api_version(),
                        query_loader_api_version());
    }



    const char* physical_device_type_name(VkPhysicalDeviceType type)
    {
        switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual";
        case VK_PHYSICAL_DEVICE_TYPE_CPU: return "cpu";
        default: break;
        }
        return "other";
    }



    const char* severity_name(VkDebugUtilsMessageSeverityFlagBitsEXT severity)
    {
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            return "error";
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            return "warning";
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
            return "info";
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
            return "verbose";
        return "unknown";
    }



    void print_message_type(VkDebugUtilsMessageTypeFlagsEXT message_type)
    {
        bool first = true;

        if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
            print(stderr, "general");
            first = false;
        }
        if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
            if (!first)
                print(stderr, "|");
            print(stderr, "validation");
            first = false;
        }
        if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
            if (!first)
                print(stderr, "|");
            print(stderr, "performance");
            first = false;
        }
#    ifdef VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT
        if (message_type
            & VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT) {
            if (!first)
                print(stderr, "|");
            print(stderr, "device_address_binding");
            first = false;
        }
#    endif

        if (first)
            print(stderr, "unknown");
    }



    const char* object_type_name(VkObjectType object_type)
    {
        switch (object_type) {
        case VK_OBJECT_TYPE_INSTANCE: return "instance";
        case VK_OBJECT_TYPE_PHYSICAL_DEVICE: return "physical_device";
        case VK_OBJECT_TYPE_DEVICE: return "device";
        case VK_OBJECT_TYPE_QUEUE: return "queue";
        case VK_OBJECT_TYPE_SEMAPHORE: return "semaphore";
        case VK_OBJECT_TYPE_COMMAND_BUFFER: return "command_buffer";
        case VK_OBJECT_TYPE_FENCE: return "fence";
        case VK_OBJECT_TYPE_DEVICE_MEMORY: return "device_memory";
        case VK_OBJECT_TYPE_BUFFER: return "buffer";
        case VK_OBJECT_TYPE_IMAGE: return "image";
        case VK_OBJECT_TYPE_EVENT: return "event";
        case VK_OBJECT_TYPE_QUERY_POOL: return "query_pool";
        case VK_OBJECT_TYPE_BUFFER_VIEW: return "buffer_view";
        case VK_OBJECT_TYPE_IMAGE_VIEW: return "image_view";
        case VK_OBJECT_TYPE_SHADER_MODULE: return "shader_module";
        case VK_OBJECT_TYPE_PIPELINE_CACHE: return "pipeline_cache";
        case VK_OBJECT_TYPE_PIPELINE_LAYOUT: return "pipeline_layout";
        case VK_OBJECT_TYPE_RENDER_PASS: return "render_pass";
        case VK_OBJECT_TYPE_PIPELINE: return "pipeline";
        case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
            return "descriptor_set_layout";
        case VK_OBJECT_TYPE_SAMPLER: return "sampler";
        case VK_OBJECT_TYPE_DESCRIPTOR_POOL: return "descriptor_pool";
        case VK_OBJECT_TYPE_DESCRIPTOR_SET: return "descriptor_set";
        case VK_OBJECT_TYPE_FRAMEBUFFER: return "framebuffer";
        case VK_OBJECT_TYPE_COMMAND_POOL: return "command_pool";
        case VK_OBJECT_TYPE_SURFACE_KHR: return "surface";
        case VK_OBJECT_TYPE_SWAPCHAIN_KHR: return "swapchain";
        case VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT:
            return "debug_utils_messenger";
        default: break;
        }
        return "unknown";
    }



    VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data)
    {
        (void)user_data;

        const char* message_id_name = "";
        const char* message         = "";
        int32_t message_id_number   = 0;
        if (callback_data != nullptr) {
            if (callback_data->pMessageIdName != nullptr)
                message_id_name = callback_data->pMessageIdName;
            if (callback_data->pMessage != nullptr)
                message = callback_data->pMessage;
            message_id_number = callback_data->messageIdNumber;
        }

        print(stderr, "imiv: vk[{}][", severity_name(severity));
        print_message_type(message_type);
        print(stderr, "] id={} {}: {}\n", message_id_number, message_id_name,
              message);

        const bool print_objects
            = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
              || (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT);
        if (print_objects && callback_data != nullptr
            && callback_data->objectCount > 0) {
            for (uint32_t i = 0; i < callback_data->objectCount; ++i) {
                const VkDebugUtilsObjectNameInfoEXT& o
                    = callback_data->pObjects[i];
                const char* object_name = (o.pObjectName != nullptr
                                           && o.pObjectName[0] != '\0')
                                              ? o.pObjectName
                                              : "<unnamed>";
                print(stderr, "imiv: vk object[{}] type={} handle={} name={}\n",
                      i, object_type_name(o.objectType), o.objectHandle,
                      object_name);
            }
        }
        return VK_FALSE;
    }



    const char* texture_status_name(ImTextureStatus status)
    {
        switch (status) {
        case ImTextureStatus_OK: return "ok";
        case ImTextureStatus_Destroyed: return "destroyed";
        case ImTextureStatus_WantCreate: return "want_create";
        case ImTextureStatus_WantUpdates: return "want_updates";
        case ImTextureStatus_WantDestroy: return "want_destroy";
        default: break;
        }
        return "unknown";
    }



    std::string queue_flags_string(VkQueueFlags flags)
    {
        std::string out;
        if (flags & VK_QUEUE_GRAPHICS_BIT) {
            if (!out.empty())
                out += "|";
            out += "graphics";
        }
        if (flags & VK_QUEUE_COMPUTE_BIT) {
            if (!out.empty())
                out += "|";
            out += "compute";
        }
        if (flags & VK_QUEUE_TRANSFER_BIT) {
            if (!out.empty())
                out += "|";
            out += "transfer";
        }
        if (flags & VK_QUEUE_SPARSE_BINDING_BIT) {
            if (!out.empty())
                out += "|";
            out += "sparse";
        }
#    ifdef VK_QUEUE_PROTECTED_BIT
        if (flags & VK_QUEUE_PROTECTED_BIT) {
            if (!out.empty())
                out += "|";
            out += "protected";
        }
#    endif
        if (out.empty())
            out = "none";
        return out;
    }



    bool cache_queue_family_properties(VulkanState& vk_state,
                                       std::string& error_message)
    {
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(vk_state.physical_device,
                                                 &queue_family_count, nullptr);
        if (queue_family_count == 0) {
            error_message
                = "vkGetPhysicalDeviceQueueFamilyProperties found no queue families";
            return false;
        }
        if (vk_state.queue_family >= queue_family_count) {
            error_message = "selected queue family index is out of range";
            return false;
        }

        std::vector<VkQueueFamilyProperties> properties(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(vk_state.physical_device,
                                                 &queue_family_count,
                                                 properties.data());
        vk_state.queue_family_properties = properties[vk_state.queue_family];

        const VkExtent3D granularity
            = vk_state.queue_family_properties.minImageTransferGranularity;
        vk_state.queue_requires_full_image_copies = (granularity.width == 0
                                                     || granularity.height == 0
                                                     || granularity.depth == 0);

        if (vk_state.verbose_logging) {
            print("imiv: Vulkan queue family {} flags={} granularity=({}, {}, "
                  "{}) count={}\n",
                  vk_state.queue_family,
                  queue_flags_string(
                      vk_state.queue_family_properties.queueFlags),
                  granularity.width, granularity.height, granularity.depth,
                  vk_state.queue_family_properties.queueCount);
        }
        if (vk_state.queue_requires_full_image_copies) {
            print(stderr,
                  "imiv: queue family {} requires full-image transfer copies; "
                  "enabling conservative ImGui texture upload workaround\n",
                  vk_state.queue_family);
        }
        return true;
    }



    template<typename HandleT> uint64_t vk_handle_to_u64(HandleT handle)
    {
        if constexpr (std::is_pointer<HandleT>::value) {
            return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(handle));
        } else {
            return static_cast<uint64_t>(handle);
        }
    }



    template<typename HandleT>
    void set_vk_object_name(VulkanState& vk_state, VkObjectType object_type,
                            HandleT handle, const char* name)
    {
        if (vk_state.set_debug_object_name_fn == nullptr
            || handle == VK_NULL_HANDLE || name == nullptr || name[0] == '\0')
            return;

        VkDebugUtilsObjectNameInfoEXT info = {};
        info.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        info.objectType   = object_type;
        info.objectHandle = vk_handle_to_u64(handle);
        info.pObjectName  = name;
        vk_state.set_debug_object_name_fn(vk_state.device, &info);
    }



    void name_window_frame_objects(VulkanState& vk_state)
    {
        if (vk_state.set_debug_object_name_fn == nullptr)
            return;

        ImGui_ImplVulkanH_Window* wd = &vk_state.window_data;
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_SWAPCHAIN_KHR,
                           wd->Swapchain, "imiv.main.swapchain");
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_RENDER_PASS, wd->RenderPass,
                           "imiv.main.render_pass");

        for (int i = 0; i < wd->Frames.Size; ++i) {
            char buffer_name[64] = {};
            std::snprintf(buffer_name, sizeof(buffer_name),
                          "imiv.main.frame[%d].command_buffer", i);
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_COMMAND_BUFFER,
                               wd->Frames[i].CommandBuffer, buffer_name);

            char image_name[64] = {};
            std::snprintf(image_name, sizeof(image_name),
                          "imiv.main.frame[%d].backbuffer", i);
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_IMAGE,
                               wd->Frames[i].Backbuffer, image_name);
        }
    }



    unsigned short clamp_u16(int value)
    {
        if (value <= 0)
            return 0;
        if (value
            > static_cast<int>(std::numeric_limits<unsigned short>::max())) {
            return std::numeric_limits<unsigned short>::max();
        }
        return static_cast<unsigned short>(value);
    }



    void apply_imgui_texture_update_workarounds(VulkanState& vk_state,
                                                ImDrawData* draw_data)
    {
#    if defined(IMGUI_HAS_TEXTURES)
        if (draw_data == nullptr || draw_data->Textures == nullptr)
            return;

        for (ImTextureData* tex : *draw_data->Textures) {
            if (tex == nullptr || tex->Status == ImTextureStatus_OK)
                continue;

            if (vk_state.log_imgui_texture_updates) {
                print(
                    stderr,
                    "imiv: imgui texture id={} status={} size={}x{} update=({},{} {}x{}) pending={}\n",
                    tex->UniqueID, texture_status_name(tex->Status), tex->Width,
                    tex->Height, tex->UpdateRect.x, tex->UpdateRect.y,
                    tex->UpdateRect.w, tex->UpdateRect.h, tex->Updates.Size);
            }

            if (!vk_state.queue_requires_full_image_copies
                || tex->Status != ImTextureStatus_WantUpdates) {
                continue;
            }

            ImTextureRect full_rect = {};
            full_rect.x             = 0;
            full_rect.y             = 0;
            full_rect.w             = clamp_u16(tex->Width);
            full_rect.h             = clamp_u16(tex->Height);
            tex->UpdateRect         = full_rect;
            tex->Updates.resize(1);
            tex->Updates[0] = full_rect;

            if (!vk_state.warned_about_full_imgui_uploads) {
                print(stderr,
                      "imiv: forcing full ImGui texture uploads on this queue "
                      "family due to strict transfer granularity\n");
                vk_state.warned_about_full_imgui_uploads = true;
            }
        }
#    else
        (void)vk_state;
        (void)draw_data;
#    endif
    }



    void populate_debug_messenger_ci(VkDebugUtilsMessengerCreateInfoEXT& ci,
                                     bool verbose_output)
    {
        ci       = {};
        ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        if (verbose_output) {
            ci.messageSeverity
                |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                   | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
        }
        ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        ci.pfnUserCallback = vulkan_debug_callback;
    }



    bool setup_debug_messenger(VulkanState& vk_state,
                               std::string& error_message)
    {
        PFN_vkCreateDebugUtilsMessengerEXT create_fn
            = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(vk_state.instance,
                                      "vkCreateDebugUtilsMessengerEXT"));
        if (create_fn == nullptr) {
            error_message = "vkCreateDebugUtilsMessengerEXT not available";
            return false;
        }

        VkDebugUtilsMessengerCreateInfoEXT ci = {};
        populate_debug_messenger_ci(ci, vk_state.verbose_validation_output);
        VkResult err = create_fn(vk_state.instance, &ci, vk_state.allocator,
                                 &vk_state.debug_messenger);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateDebugUtilsMessengerEXT failed";
            return false;
        }
        return true;
    }



    bool find_memory_type(VkPhysicalDevice physical_device, uint32_t type_bits,
                          VkMemoryPropertyFlags required,
                          uint32_t& memory_type_index)
    {
        VkPhysicalDeviceMemoryProperties memory_properties = {};
        vkGetPhysicalDeviceMemoryProperties(physical_device,
                                            &memory_properties);
        for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
            const bool type_matches = (type_bits & (1u << i)) != 0;
            const bool has_flags
                = (memory_properties.memoryTypes[i].propertyFlags & required)
                  == required;
            if (type_matches && has_flags) {
                memory_type_index = i;
                return true;
            }
        }
        return false;
    }



    bool has_format_features(VkPhysicalDevice physical_device, VkFormat format,
                             VkFormatFeatureFlags required_features)
    {
        VkFormatProperties props = {};
        vkGetPhysicalDeviceFormatProperties(physical_device, format, &props);
        return (props.optimalTilingFeatures & required_features)
               == required_features;
    }



    bool device_supports_required_extensions(VkPhysicalDevice physical_device,
                                             bool& has_portability_subset,
                                             std::string& error_message)
    {
        has_portability_subset = false;

        uint32_t device_extension_count = 0;
        VkResult err                    = vkEnumerateDeviceExtensionProperties(
            physical_device, nullptr, &device_extension_count, nullptr);
        if (err != VK_SUCCESS) {
            error_message = "vkEnumerateDeviceExtensionProperties failed";
            return false;
        }

        ImVector<VkExtensionProperties> device_properties;
        device_properties.resize(device_extension_count);
        err = vkEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                                   &device_extension_count,
                                                   device_properties.Data);
        if (err != VK_SUCCESS) {
            error_message = "vkEnumerateDeviceExtensionProperties failed";
            return false;
        }

        if (!is_extension_available(device_properties,
                                    VK_KHR_SWAPCHAIN_EXTENSION_NAME))
            return false;

#    ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
        has_portability_subset
            = is_extension_available(device_properties,
                                     VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#    endif
        return true;
    }



    int score_device_type(VkPhysicalDeviceType type)
    {
        switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 1000;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 500;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 250;
        case VK_PHYSICAL_DEVICE_TYPE_CPU: return 50;
        default: break;
        }
        return 10;
    }



    int score_queue_family(const VkQueueFamilyProperties& properties)
    {
        int score = 0;
        if ((properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            score += 100;
        if ((properties.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0)
            score += 100;
        if ((properties.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0)
            score += 25;
        score += static_cast<int>(properties.queueCount);
        return score;
    }



    bool select_physical_device_and_queue(VulkanState& vk_state,
                                          std::string& error_message)
    {
        const VkFormatFeatureFlags required_compute_output_features
            = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT
              | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

        uint32_t device_count = 0;
        VkResult err          = vkEnumeratePhysicalDevices(vk_state.instance,
                                                           &device_count, nullptr);
        if (err != VK_SUCCESS) {
            error_message = "vkEnumeratePhysicalDevices failed";
            return false;
        }
        if (device_count == 0) {
            error_message = "no Vulkan physical device found";
            return false;
        }

        ImVector<VkPhysicalDevice> devices;
        devices.resize(device_count);
        err = vkEnumeratePhysicalDevices(vk_state.instance, &device_count,
                                         devices.Data);
        if (err != VK_SUCCESS) {
            error_message = "vkEnumeratePhysicalDevices failed";
            return false;
        }

        int best_score               = std::numeric_limits<int>::min();
        VkPhysicalDevice best_device = VK_NULL_HANDLE;
        uint32_t best_queue_family   = static_cast<uint32_t>(-1);
        VkPhysicalDeviceProperties best_properties = {};

        for (VkPhysicalDevice device : devices) {
            bool has_portability_subset = false;
            std::string extension_error;
            if (!device_supports_required_extensions(device,
                                                     has_portability_subset,
                                                     extension_error)) {
                if (!extension_error.empty()) {
                    error_message = extension_error;
                    return false;
                }
                continue;
            }

            VkPhysicalDeviceProperties properties = {};
            vkGetPhysicalDeviceProperties(device, &properties);

            VkPhysicalDeviceFeatures features = {};
            vkGetPhysicalDeviceFeatures(device, &features);

            if (!has_format_features(device, VK_FORMAT_R16G16B16A16_SFLOAT,
                                     required_compute_output_features)
                && !has_format_features(device, VK_FORMAT_R32G32B32A32_SFLOAT,
                                        required_compute_output_features)) {
                continue;
            }

            uint32_t queue_family_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device,
                                                     &queue_family_count,
                                                     nullptr);
            if (queue_family_count == 0)
                continue;

            std::vector<VkQueueFamilyProperties> queue_families(
                queue_family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(device,
                                                     &queue_family_count,
                                                     queue_families.data());

            for (uint32_t family_index = 0; family_index < queue_family_count;
                 ++family_index) {
                const VkQueueFamilyProperties& family_properties
                    = queue_families[family_index];
                if ((family_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0
                    || (family_properties.queueFlags & VK_QUEUE_COMPUTE_BIT)
                           == 0
                    || family_properties.queueCount == 0) {
                    continue;
                }

                VkBool32 supports_present = VK_FALSE;
                err = vkGetPhysicalDeviceSurfaceSupportKHR(device, family_index,
                                                           vk_state.surface,
                                                           &supports_present);
                if (err != VK_SUCCESS) {
                    error_message
                        = "vkGetPhysicalDeviceSurfaceSupportKHR failed";
                    return false;
                }
                if (supports_present != VK_TRUE)
                    continue;

                int score = score_device_type(properties.deviceType);
                score += score_queue_family(family_properties);
                if (features.shaderFloat64 == VK_TRUE)
                    score += 50;
                if (has_portability_subset)
                    score += 5;

                if (score > best_score) {
                    best_score        = score;
                    best_device       = device;
                    best_queue_family = family_index;
                    best_properties   = properties;
                }
            }
        }

        if (best_device == VK_NULL_HANDLE
            || best_queue_family == static_cast<uint32_t>(-1)) {
            error_message
                = "no Vulkan device/queue family supports graphics+compute+present for the window surface";
            return false;
        }

        vk_state.physical_device = best_device;
        vk_state.queue_family    = best_queue_family;
        if (!cache_queue_family_properties(vk_state, error_message))
            return false;

        if (vk_state.verbose_logging) {
            print("imiv: selected Vulkan device='{}' type={} api={}.{}.{} "
                  "queue_family={}\n",
                  best_properties.deviceName,
                  physical_device_type_name(best_properties.deviceType),
                  VK_API_VERSION_MAJOR(best_properties.apiVersion),
                  VK_API_VERSION_MINOR(best_properties.apiVersion),
                  VK_API_VERSION_PATCH(best_properties.apiVersion),
                  vk_state.queue_family);
        }
        return true;
    }



    bool read_binary_file(const std::string& path, std::vector<uint32_t>& words,
                          std::string& error_message)
    {
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in) {
            error_message = Strutil::fmt::format("cannot open '{}'", path);
            return false;
        }

        const std::streamsize size = in.tellg();
        if (size <= 0 || (size % 4) != 0) {
            error_message
                = Strutil::fmt::format("invalid SPIR-V file size for '{}'",
                                       path);
            return false;
        }
        in.seekg(0, std::ios::beg);

        words.resize(static_cast<size_t>(size) / 4);
        if (!in.read(reinterpret_cast<char*>(words.data()), size)) {
            error_message = Strutil::fmt::format("failed to read '{}'", path);
            words.clear();
            return false;
        }
        return true;
    }



    bool create_compute_pipeline_from_file(VulkanState& vk_state,
                                           const std::string& shader_path,
                                           const char* debug_name,
                                           VkPipeline& out_pipeline,
                                           std::string& error_message)
    {
        out_pipeline = VK_NULL_HANDLE;
        std::vector<uint32_t> shader_words;
        if (!read_binary_file(shader_path, shader_words, error_message))
            return false;

        VkShaderModule shader_module       = VK_NULL_HANDLE;
        VkShaderModuleCreateInfo shader_ci = {};
        shader_ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_ci.codeSize = shader_words.size() * sizeof(uint32_t);
        shader_ci.pCode    = shader_words.data();
        VkResult err       = vkCreateShaderModule(vk_state.device, &shader_ci,
                                                  vk_state.allocator, &shader_module);
        if (err != VK_SUCCESS) {
            error_message
                = Strutil::fmt::format("vkCreateShaderModule failed for '{}'",
                                       shader_path);
            return false;
        }

        VkPipelineShaderStageCreateInfo stage_ci = {};
        stage_ci.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_ci.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
        stage_ci.module = shader_module;
        stage_ci.pName  = "main";
        VkComputePipelineCreateInfo pipeline_ci = {};
        pipeline_ci.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_ci.stage  = stage_ci;
        pipeline_ci.layout = vk_state.compute_pipeline_layout;
        err = vkCreateComputePipelines(vk_state.device, vk_state.pipeline_cache,
                                       1, &pipeline_ci, vk_state.allocator,
                                       &out_pipeline);
        vkDestroyShaderModule(vk_state.device, shader_module,
                              vk_state.allocator);
        if (err != VK_SUCCESS) {
            error_message = Strutil::fmt::format(
                "vkCreateComputePipelines failed for '{}'", shader_path);
            out_pipeline = VK_NULL_HANDLE;
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_PIPELINE, out_pipeline,
                           debug_name);
        return true;
    }



    bool create_shader_module_from_file(VkDevice device,
                                        VkAllocationCallbacks* allocator,
                                        const std::string& shader_path,
                                        VkShaderModule& shader_module,
                                        std::string& error_message)
    {
        shader_module = VK_NULL_HANDLE;
        std::vector<uint32_t> shader_words;
        if (!read_binary_file(shader_path, shader_words, error_message))
            return false;

        VkShaderModuleCreateInfo shader_ci = {};
        shader_ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_ci.codeSize = shader_words.size() * sizeof(uint32_t);
        shader_ci.pCode    = shader_words.data();
        VkResult err       = vkCreateShaderModule(device, &shader_ci, allocator,
                                                  &shader_module);
        if (err != VK_SUCCESS) {
            error_message
                = Strutil::fmt::format("vkCreateShaderModule failed for '{}'",
                                       shader_path);
            shader_module = VK_NULL_HANDLE;
            return false;
        }
        return true;
    }



    void destroy_preview_resources(VulkanState& vk_state)
    {
        if (vk_state.preview_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(vk_state.device, vk_state.preview_pipeline,
                              vk_state.allocator);
            vk_state.preview_pipeline = VK_NULL_HANDLE;
        }
        if (vk_state.preview_pipeline_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(vk_state.device,
                                    vk_state.preview_pipeline_layout,
                                    vk_state.allocator);
            vk_state.preview_pipeline_layout = VK_NULL_HANDLE;
        }
        if (vk_state.preview_descriptor_set_layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(vk_state.device,
                                         vk_state.preview_descriptor_set_layout,
                                         vk_state.allocator);
            vk_state.preview_descriptor_set_layout = VK_NULL_HANDLE;
        }
        if (vk_state.preview_descriptor_pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(vk_state.device,
                                    vk_state.preview_descriptor_pool,
                                    vk_state.allocator);
            vk_state.preview_descriptor_pool = VK_NULL_HANDLE;
        }
        if (vk_state.preview_render_pass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(vk_state.device, vk_state.preview_render_pass,
                                vk_state.allocator);
            vk_state.preview_render_pass = VK_NULL_HANDLE;
        }
    }



    bool init_preview_resources(VulkanState& vk_state,
                                std::string& error_message)
    {
#    if !(defined(IMIV_HAS_COMPUTE_UPLOAD_SHADERS) \
          && IMIV_HAS_COMPUTE_UPLOAD_SHADERS)
        error_message = "preview shaders were not generated at build time";
        return false;
#    else
        destroy_preview_resources(vk_state);

        if (!has_format_features(vk_state.physical_device,
                                 vk_state.compute_output_format,
                                 VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)) {
            error_message
                = "selected preview output format does not support color attachment";
            return false;
        }

        VkAttachmentDescription attachment = {};
        attachment.format                  = vk_state.compute_output_format;
        attachment.samples                 = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp           = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp          = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_ref = {};
        color_ref.attachment            = 0;
        color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &color_ref;

        VkRenderPassCreateInfo render_pass_ci = {};
        render_pass_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_ci.attachmentCount = 1;
        render_pass_ci.pAttachments    = &attachment;
        render_pass_ci.subpassCount    = 1;
        render_pass_ci.pSubpasses      = &subpass;
        VkResult err = vkCreateRenderPass(vk_state.device, &render_pass_ci,
                                          vk_state.allocator,
                                          &vk_state.preview_render_pass);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateRenderPass failed for preview pipeline";
            destroy_preview_resources(vk_state);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_RENDER_PASS,
                           vk_state.preview_render_pass,
                           "imiv.preview.render_pass");

        VkDescriptorPoolSize preview_pool_size = {};
        preview_pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        preview_pool_size.descriptorCount          = 64;
        VkDescriptorPoolCreateInfo preview_pool_ci = {};
        preview_pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        preview_pool_ci.flags
            = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        preview_pool_ci.maxSets       = 64;
        preview_pool_ci.poolSizeCount = 1;
        preview_pool_ci.pPoolSizes    = &preview_pool_size;
        err = vkCreateDescriptorPool(vk_state.device, &preview_pool_ci,
                                     vk_state.allocator,
                                     &vk_state.preview_descriptor_pool);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateDescriptorPool failed for preview";
            destroy_preview_resources(vk_state);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                           vk_state.preview_descriptor_pool,
                           "imiv.preview.descriptor_pool");

        VkDescriptorSetLayoutBinding preview_binding = {};
        preview_binding.binding                      = 0;
        preview_binding.descriptorType
            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        preview_binding.descriptorCount = 1;
        preview_binding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo preview_set_layout_ci = {};
        preview_set_layout_ci.sType
            = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        preview_set_layout_ci.bindingCount = 1;
        preview_set_layout_ci.pBindings    = &preview_binding;
        err                                = vkCreateDescriptorSetLayout(
            vk_state.device, &preview_set_layout_ci, vk_state.allocator,
            &vk_state.preview_descriptor_set_layout);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateDescriptorSetLayout failed for preview";
            destroy_preview_resources(vk_state);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                           vk_state.preview_descriptor_set_layout,
                           "imiv.preview.set_layout");

        VkPushConstantRange preview_push = {};
        preview_push.stageFlags          = VK_SHADER_STAGE_FRAGMENT_BIT;
        preview_push.offset              = 0;
        preview_push.size                = sizeof(PreviewPushConstants);

        VkPipelineLayoutCreateInfo preview_layout_ci = {};
        preview_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        preview_layout_ci.setLayoutCount = 1;
        preview_layout_ci.pSetLayouts = &vk_state.preview_descriptor_set_layout;
        preview_layout_ci.pushConstantRangeCount = 1;
        preview_layout_ci.pPushConstantRanges    = &preview_push;
        err = vkCreatePipelineLayout(vk_state.device, &preview_layout_ci,
                                     vk_state.allocator,
                                     &vk_state.preview_pipeline_layout);
        if (err != VK_SUCCESS) {
            error_message = "vkCreatePipelineLayout failed for preview";
            destroy_preview_resources(vk_state);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                           vk_state.preview_pipeline_layout,
                           "imiv.preview.pipeline_layout");

        const std::string shader_vert = std::string(IMIV_SHADER_DIR)
                                        + "/imiv_preview.vert.spv";
        const std::string shader_frag = std::string(IMIV_SHADER_DIR)
                                        + "/imiv_preview.frag.spv";
        VkShaderModule vert_module = VK_NULL_HANDLE;
        VkShaderModule frag_module = VK_NULL_HANDLE;
        if (!create_shader_module_from_file(vk_state.device, vk_state.allocator,
                                            shader_vert, vert_module,
                                            error_message)) {
            destroy_preview_resources(vk_state);
            return false;
        }
        if (!create_shader_module_from_file(vk_state.device, vk_state.allocator,
                                            shader_frag, frag_module,
                                            error_message)) {
            vkDestroyShaderModule(vk_state.device, vert_module,
                                  vk_state.allocator);
            destroy_preview_resources(vk_state);
            return false;
        }

        VkPipelineShaderStageCreateInfo stages[2] = {};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vert_module;
        stages[0].pName  = "main";
        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = frag_module;
        stages[1].pName  = "main";

        VkPipelineVertexInputStateCreateInfo vertex_input = {};
        vertex_input.sType
            = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
        input_assembly.sType
            = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport_state = {};
        viewport_state.sType
            = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount                  = 1;
        viewport_state.scissorCount                   = 1;
        VkPipelineRasterizationStateCreateInfo raster = {};
        raster.sType
            = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode    = VK_CULL_MODE_NONE;
        raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth   = 1.0f;
        VkPipelineMultisampleStateCreateInfo multisample = {};
        multisample.sType
            = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState color_blend_attachment = {};
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
                                                | VK_COLOR_COMPONENT_G_BIT
                                                | VK_COLOR_COMPONENT_B_BIT
                                                | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo color_blend = {};
        color_blend.sType
            = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend.attachmentCount     = 1;
        color_blend.pAttachments        = &color_blend_attachment;
        VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT,
                                            VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic_state = {};
        dynamic_state.sType
            = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = static_cast<uint32_t>(
            IM_ARRAYSIZE(dynamic_states));
        dynamic_state.pDynamicStates = dynamic_states;

        VkGraphicsPipelineCreateInfo pipeline_ci = {};
        pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_ci.stageCount          = 2;
        pipeline_ci.pStages             = stages;
        pipeline_ci.pVertexInputState   = &vertex_input;
        pipeline_ci.pInputAssemblyState = &input_assembly;
        pipeline_ci.pViewportState      = &viewport_state;
        pipeline_ci.pRasterizationState = &raster;
        pipeline_ci.pMultisampleState   = &multisample;
        pipeline_ci.pColorBlendState    = &color_blend;
        pipeline_ci.pDynamicState       = &dynamic_state;
        pipeline_ci.layout              = vk_state.preview_pipeline_layout;
        pipeline_ci.renderPass          = vk_state.preview_render_pass;
        pipeline_ci.subpass             = 0;

        err = vkCreateGraphicsPipelines(vk_state.device,
                                        vk_state.pipeline_cache, 1,
                                        &pipeline_ci, vk_state.allocator,
                                        &vk_state.preview_pipeline);
        vkDestroyShaderModule(vk_state.device, vert_module, vk_state.allocator);
        vkDestroyShaderModule(vk_state.device, frag_module, vk_state.allocator);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateGraphicsPipelines failed for preview";
            destroy_preview_resources(vk_state);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_PIPELINE,
                           vk_state.preview_pipeline, "imiv.preview.pipeline");

        return true;
#    endif
    }



    void destroy_compute_upload_resources(VulkanState& vk_state)
    {
        if (vk_state.compute_pipeline_fp64 != VK_NULL_HANDLE) {
            vkDestroyPipeline(vk_state.device, vk_state.compute_pipeline_fp64,
                              vk_state.allocator);
            vk_state.compute_pipeline_fp64 = VK_NULL_HANDLE;
        }
        if (vk_state.compute_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(vk_state.device, vk_state.compute_pipeline,
                              vk_state.allocator);
            vk_state.compute_pipeline = VK_NULL_HANDLE;
        }
        if (vk_state.compute_pipeline_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(vk_state.device,
                                    vk_state.compute_pipeline_layout,
                                    vk_state.allocator);
            vk_state.compute_pipeline_layout = VK_NULL_HANDLE;
        }
        if (vk_state.compute_descriptor_set_layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(vk_state.device,
                                         vk_state.compute_descriptor_set_layout,
                                         vk_state.allocator);
            vk_state.compute_descriptor_set_layout = VK_NULL_HANDLE;
        }
        if (vk_state.compute_descriptor_pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(vk_state.device,
                                    vk_state.compute_descriptor_pool,
                                    vk_state.allocator);
            vk_state.compute_descriptor_pool = VK_NULL_HANDLE;
        }
        vk_state.compute_output_format = VK_FORMAT_UNDEFINED;
        vk_state.compute_upload_ready  = false;
    }



    bool init_compute_upload_resources(VulkanState& vk_state,
                                       std::string& error_message)
    {
#    if !(defined(IMIV_HAS_COMPUTE_UPLOAD_SHADERS) \
          && IMIV_HAS_COMPUTE_UPLOAD_SHADERS)
        error_message
            = "compute upload shaders were not generated at build time";
        return false;
#    else
        destroy_compute_upload_resources(vk_state);

        if ((vk_state.queue_family_properties.queueFlags & VK_QUEUE_COMPUTE_BIT)
            == 0) {
            error_message
                = "selected Vulkan queue family does not support compute";
            return false;
        }

        VkPhysicalDeviceFeatures features = {};
        vkGetPhysicalDeviceFeatures(vk_state.physical_device, &features);
        vk_state.compute_supports_float64 = (features.shaderFloat64 == VK_TRUE);

        const VkFormatFeatureFlags required
            = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT
              | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

        std::string shader_path;
        std::string shader_path_fp64;
        if (has_format_features(vk_state.physical_device,
                                VK_FORMAT_R16G16B16A16_SFLOAT, required)) {
            vk_state.compute_output_format = VK_FORMAT_R16G16B16A16_SFLOAT;
            shader_path                    = std::string(IMIV_SHADER_DIR)
                          + "/imiv_upload_to_rgba16f.comp.spv";
            shader_path_fp64 = std::string(IMIV_SHADER_DIR)
                               + "/imiv_upload_to_rgba16f_fp64.comp.spv";
        } else if (has_format_features(vk_state.physical_device,
                                       VK_FORMAT_R32G32B32A32_SFLOAT,
                                       required)) {
            vk_state.compute_output_format = VK_FORMAT_R32G32B32A32_SFLOAT;
            shader_path                    = std::string(IMIV_SHADER_DIR)
                          + "/imiv_upload_to_rgba32f.comp.spv";
            shader_path_fp64 = std::string(IMIV_SHADER_DIR)
                               + "/imiv_upload_to_rgba32f_fp64.comp.spv";
        } else {
            error_message
                = "no compute output format support for rgba16f/rgba32f storage image";
            return false;
        }

        VkDescriptorPoolSize pool_sizes[2] = {};
        pool_sizes[0].type                 = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        pool_sizes[0].descriptorCount      = 64;
        pool_sizes[1].type                 = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        pool_sizes[1].descriptorCount      = 64;
        VkDescriptorPoolCreateInfo pool_ci = {};
        pool_ci.sType   = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_ci.flags   = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_ci.maxSets = 64;
        pool_ci.poolSizeCount = 2;
        pool_ci.pPoolSizes    = pool_sizes;
        VkResult err = vkCreateDescriptorPool(vk_state.device, &pool_ci,
                                              vk_state.allocator,
                                              &vk_state.compute_descriptor_pool);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateDescriptorPool failed for compute upload";
            destroy_compute_upload_resources(vk_state);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                           vk_state.compute_descriptor_pool,
                           "imiv.compute_upload.descriptor_pool");

        VkDescriptorSetLayoutBinding bindings[2] = {};
        bindings[0].binding                      = 0;
        bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[1].binding         = 1;
        bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
        VkDescriptorSetLayoutCreateInfo set_layout_ci = {};
        set_layout_ci.sType
            = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        set_layout_ci.bindingCount = 2;
        set_layout_ci.pBindings    = bindings;
        err                        = vkCreateDescriptorSetLayout(
            vk_state.device, &set_layout_ci, vk_state.allocator,
            &vk_state.compute_descriptor_set_layout);
        if (err != VK_SUCCESS) {
            error_message
                = "vkCreateDescriptorSetLayout failed for compute upload";
            destroy_compute_upload_resources(vk_state);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                           vk_state.compute_descriptor_set_layout,
                           "imiv.compute_upload.set_layout");

        VkPushConstantRange push_range = {};
        push_range.stageFlags          = VK_SHADER_STAGE_COMPUTE_BIT;
        push_range.offset              = 0;
        push_range.size                = sizeof(UploadComputePushConstants);
        VkPipelineLayoutCreateInfo pipeline_layout_ci = {};
        pipeline_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_ci.setLayoutCount = 1;
        pipeline_layout_ci.pSetLayouts = &vk_state.compute_descriptor_set_layout;
        pipeline_layout_ci.pushConstantRangeCount = 1;
        pipeline_layout_ci.pPushConstantRanges    = &push_range;
        err = vkCreatePipelineLayout(vk_state.device, &pipeline_layout_ci,
                                     vk_state.allocator,
                                     &vk_state.compute_pipeline_layout);
        if (err != VK_SUCCESS) {
            error_message = "vkCreatePipelineLayout failed for compute upload";
            destroy_compute_upload_resources(vk_state);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                           vk_state.compute_pipeline_layout,
                           "imiv.compute_upload.pipeline_layout");

        if (!create_compute_pipeline_from_file(vk_state, shader_path,
                                               "imiv.compute_upload.pipeline",
                                               vk_state.compute_pipeline,
                                               error_message)) {
            destroy_compute_upload_resources(vk_state);
            return false;
        }

        if (vk_state.compute_supports_float64) {
            std::string fp64_error;
            if (!create_compute_pipeline_from_file(
                    vk_state, shader_path_fp64,
                    "imiv.compute_upload.pipeline_fp64",
                    vk_state.compute_pipeline_fp64, fp64_error)) {
                print(stderr,
                      "imiv: fp64 compute pipeline unavailable, will fallback "
                      "double->float on CPU: {}\n",
                      fp64_error);
            }
        }

        vk_state.compute_upload_ready = true;
        if (vk_state.verbose_logging) {
            print(
                "imiv: compute upload ready output_format={} shader={} float64_support={} fp64_pipeline={}\n",
                static_cast<int>(vk_state.compute_output_format), shader_path,
                vk_state.compute_supports_float64 ? "yes" : "no",
                vk_state.compute_pipeline_fp64 != VK_NULL_HANDLE ? "yes"
                                                                 : "no");
        }
        return true;
#    endif
    }



    bool setup_vulkan_instance(VulkanState& vk_state,
                               ImVector<const char*>& instance_extensions,
                               std::string& error_message)
    {
        VkResult err;

        VkInstanceCreateInfo instance_ci = {};
        instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ImVector<const char*> instance_layers;

#    if defined(IMIV_VULKAN_VALIDATION) && IMIV_VULKAN_VALIDATION
        if (is_layer_available("VK_LAYER_KHRONOS_validation")) {
            vk_state.validation_layer_enabled = true;
            instance_layers.push_back("VK_LAYER_KHRONOS_validation");
        } else {
            print(
                stderr,
                "imiv: Vulkan validation requested but VK_LAYER_KHRONOS_validation was not found\n");
        }
#    endif

        uint32_t extension_count = 0;
        err = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                                     nullptr);
        if (err != VK_SUCCESS) {
            error_message = "vkEnumerateInstanceExtensionProperties failed";
            return false;
        }

        ImVector<VkExtensionProperties> instance_properties;
        instance_properties.resize(extension_count);
        err = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                                     instance_properties.Data);
        if (err != VK_SUCCESS) {
            error_message = "vkEnumerateInstanceExtensionProperties failed";
            return false;
        }

        if (is_extension_available(
                instance_properties,
                VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
            append_unique_extension(
                instance_extensions,
                VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#    ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
        if (is_extension_available(
                instance_properties,
                VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
            append_unique_extension(
                instance_extensions,
                VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            instance_ci.flags
                |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
#    endif
        if (vk_state.validation_layer_enabled) {
            if (is_extension_available(instance_properties,
                                       VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
                append_unique_extension(instance_extensions,
                                        VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                vk_state.debug_utils_enabled = true;
            } else {
                print(
                    stderr,
                    "imiv: VK_EXT_debug_utils not found; validation output will be limited\n");
            }
        }

        VkApplicationInfo app_info        = {};
        app_info.sType                    = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName         = "imiv";
        app_info.applicationVersion       = VK_MAKE_API_VERSION(0, 0, 1, 0);
        app_info.pEngineName              = "OpenImageIO";
        app_info.engineVersion            = VK_MAKE_API_VERSION(0, 0, 1, 0);
        vk_state.api_version              = choose_instance_api_version();
        app_info.apiVersion               = vk_state.api_version;
        instance_ci.pApplicationInfo      = &app_info;
        instance_ci.enabledExtensionCount = static_cast<uint32_t>(
            instance_extensions.Size);
        instance_ci.ppEnabledExtensionNames = instance_extensions.Data;
        instance_ci.enabledLayerCount       = static_cast<uint32_t>(
            instance_layers.Size);
        instance_ci.ppEnabledLayerNames = instance_layers.Data;

        VkDebugUtilsMessengerCreateInfoEXT debug_ci = {};
        if (vk_state.validation_layer_enabled && vk_state.debug_utils_enabled) {
            populate_debug_messenger_ci(debug_ci,
                                        vk_state.verbose_validation_output);
            instance_ci.pNext = &debug_ci;
        }

        err = vkCreateInstance(&instance_ci, vk_state.allocator,
                               &vk_state.instance);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateInstance failed";
            return false;
        }
        if (vk_state.validation_layer_enabled) {
            if (vk_state.debug_utils_enabled) {
                if (!setup_debug_messenger(vk_state, error_message))
                    return false;
                if (vk_state.verbose_validation_output) {
                    print("imiv: Vulkan validation enabled with verbose debug "
                          "utils output\n");
                } else if (vk_state.verbose_logging) {
                    print(
                        "imiv: Vulkan validation enabled with warnings/errors "
                        "only\n");
                }
            } else {
                if (vk_state.verbose_logging) {
                    print("imiv: Vulkan validation enabled without debug utils "
                          "messenger\n");
                }
            }
        }
        return true;
    }

    bool setup_vulkan_device(VulkanState& vk_state, std::string& error_message)
    {
        VkResult err;

        if (!select_physical_device_and_queue(vk_state, error_message))
            return false;

        ImVector<const char*> device_extensions;
        device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        uint32_t device_extension_count = 0;
        err = vkEnumerateDeviceExtensionProperties(vk_state.physical_device,
                                                   nullptr,
                                                   &device_extension_count,
                                                   nullptr);
        if (err != VK_SUCCESS) {
            error_message = "vkEnumerateDeviceExtensionProperties failed";
            return false;
        }
        ImVector<VkExtensionProperties> device_properties;
        device_properties.resize(device_extension_count);
        err = vkEnumerateDeviceExtensionProperties(vk_state.physical_device,
                                                   nullptr,
                                                   &device_extension_count,
                                                   device_properties.Data);
        if (err != VK_SUCCESS) {
            error_message = "vkEnumerateDeviceExtensionProperties failed";
            return false;
        }
#    ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
        if (is_extension_available(device_properties,
                                   VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
            device_extensions.push_back(
                VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#    endif

        const float queue_priority[]     = { 1.0f };
        VkDeviceQueueCreateInfo queue_ci = {};
        queue_ci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_ci.queueFamilyIndex = vk_state.queue_family;
        queue_ci.queueCount       = 1;
        queue_ci.pQueuePriorities = queue_priority;

        VkPhysicalDeviceFeatures supported_features = {};
        vkGetPhysicalDeviceFeatures(vk_state.physical_device,
                                    &supported_features);
        VkPhysicalDeviceFeatures enabled_features = {};
        if (supported_features.shaderFloat64 == VK_TRUE) {
            enabled_features.shaderFloat64    = VK_TRUE;
            vk_state.compute_supports_float64 = true;
        } else {
            vk_state.compute_supports_float64 = false;
        }

        VkDeviceCreateInfo device_ci    = {};
        device_ci.sType                 = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_ci.queueCreateInfoCount  = 1;
        device_ci.pQueueCreateInfos     = &queue_ci;
        device_ci.enabledExtensionCount = static_cast<uint32_t>(
            device_extensions.Size);
        device_ci.ppEnabledExtensionNames = device_extensions.Data;
        device_ci.pEnabledFeatures        = &enabled_features;
        err = vkCreateDevice(vk_state.physical_device, &device_ci,
                             vk_state.allocator, &vk_state.device);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateDevice failed";
            return false;
        }

        if (vk_state.debug_utils_enabled) {
            vk_state.set_debug_object_name_fn
                = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
                    vkGetDeviceProcAddr(vk_state.device,
                                        "vkSetDebugUtilsObjectNameEXT"));
            if (vk_state.set_debug_object_name_fn == nullptr) {
                print(
                    stderr,
                    "imiv: vkSetDebugUtilsObjectNameEXT not available on device\n");
            }
        }

        vkGetDeviceQueue(vk_state.device, vk_state.queue_family, 0,
                         &vk_state.queue);
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_QUEUE, vk_state.queue,
                           "imiv.main.queue");

        VkDescriptorPoolSize pool_sizes[]
            = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256 } };
        VkDescriptorPoolCreateInfo pool_ci = {};
        pool_ci.sType   = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_ci.flags   = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_ci.maxSets = 256;
        pool_ci.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(pool_sizes));
        pool_ci.pPoolSizes    = pool_sizes;
        err = vkCreateDescriptorPool(vk_state.device, &pool_ci,
                                     vk_state.allocator,
                                     &vk_state.descriptor_pool);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateDescriptorPool failed";
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                           vk_state.descriptor_pool,
                           "imiv.main.descriptor_pool");

        if (!init_compute_upload_resources(vk_state, error_message))
            return false;
        if (!init_preview_resources(vk_state, error_message))
            return false;

        return true;
    }



    bool setup_vulkan_window(VulkanState& vk_state, int width, int height,
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
        const VkColorSpaceKHR request_color_space
            = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        vk_state.window_data.Surface = vk_state.surface;
        vk_state.window_data.SurfaceFormat
            = ImGui_ImplVulkanH_SelectSurfaceFormat(
                vk_state.physical_device, vk_state.window_data.Surface,
                request_surface_formats,
                static_cast<int>(IM_ARRAYSIZE(request_surface_formats)),
                request_color_space);

        const VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
        vk_state.window_data.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
            vk_state.physical_device, vk_state.window_data.Surface,
            present_modes, static_cast<int>(IM_ARRAYSIZE(present_modes)));

        ImGui_ImplVulkanH_CreateOrResizeWindow(
            vk_state.instance, vk_state.physical_device, vk_state.device,
            &vk_state.window_data, vk_state.queue_family, vk_state.allocator,
            width, height, vk_state.min_image_count,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        name_window_frame_objects(vk_state);
        return true;
    }



    void destroy_vulkan_surface(VulkanState& vk_state)
    {
        if (vk_state.surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(vk_state.instance, vk_state.surface,
                                vk_state.allocator);
            vk_state.surface = VK_NULL_HANDLE;
        }
    }



    void cleanup_vulkan_window(VulkanState& vk_state)
    {
        if (vk_state.window_data.Swapchain != VK_NULL_HANDLE) {
            ImGui_ImplVulkanH_DestroyWindow(vk_state.instance, vk_state.device,
                                            &vk_state.window_data,
                                            vk_state.allocator);
            vk_state.window_data = ImGui_ImplVulkanH_Window();
        }
        destroy_vulkan_surface(vk_state);
    }



    void cleanup_vulkan(VulkanState& vk_state)
    {
        if (vk_state.debug_messenger != VK_NULL_HANDLE) {
            PFN_vkDestroyDebugUtilsMessengerEXT destroy_fn
                = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    vkGetInstanceProcAddr(vk_state.instance,
                                          "vkDestroyDebugUtilsMessengerEXT"));
            if (destroy_fn != nullptr) {
                destroy_fn(vk_state.instance, vk_state.debug_messenger,
                           vk_state.allocator);
            }
            vk_state.debug_messenger = VK_NULL_HANDLE;
        }
        if (vk_state.device != VK_NULL_HANDLE)
            destroy_compute_upload_resources(vk_state);
        if (vk_state.device != VK_NULL_HANDLE)
            destroy_preview_resources(vk_state);
        if (vk_state.descriptor_pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(vk_state.device, vk_state.descriptor_pool,
                                    vk_state.allocator);
            vk_state.descriptor_pool = VK_NULL_HANDLE;
        }
        if (vk_state.pipeline_cache != VK_NULL_HANDLE) {
            vkDestroyPipelineCache(vk_state.device, vk_state.pipeline_cache,
                                   vk_state.allocator);
            vk_state.pipeline_cache = VK_NULL_HANDLE;
        }
        if (vk_state.device != VK_NULL_HANDLE) {
            vkDestroyDevice(vk_state.device, vk_state.allocator);
            vk_state.device = VK_NULL_HANDLE;
        }
        if (vk_state.instance != VK_NULL_HANDLE) {
            vkDestroyInstance(vk_state.instance, vk_state.allocator);
            vk_state.instance = VK_NULL_HANDLE;
        }
    }



    void frame_render(VulkanState& vk_state, ImDrawData* draw_data)
    {
        ImGui_ImplVulkanH_Window* wd = &vk_state.window_data;
        VkSemaphore image_acquired
            = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
        VkSemaphore render_complete
            = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;

        VkResult err = vkAcquireNextImageKHR(vk_state.device, wd->Swapchain,
                                             UINT64_MAX, image_acquired,
                                             VK_NULL_HANDLE, &wd->FrameIndex);
        if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
            vk_state.swapchain_rebuild = true;
        if (err == VK_ERROR_OUT_OF_DATE_KHR)
            return;
        if (err != VK_SUBOPTIMAL_KHR)
            check_vk_result(err);

        ImGui_ImplVulkanH_Frame* frame = &wd->Frames[wd->FrameIndex];
        err = vkWaitForFences(vk_state.device, 1, &frame->Fence, VK_TRUE,
                              UINT64_MAX);
        check_vk_result(err);
        err = vkResetFences(vk_state.device, 1, &frame->Fence);
        check_vk_result(err);

        err = vkResetCommandPool(vk_state.device, frame->CommandPool, 0);
        check_vk_result(err);

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(frame->CommandBuffer, &begin_info);
        check_vk_result(err);

        VkRenderPassBeginInfo rp_begin = {};
        rp_begin.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_begin.renderPass  = wd->RenderPass;
        rp_begin.framebuffer = frame->Framebuffer;
        rp_begin.renderArea.extent.width  = wd->Width;
        rp_begin.renderArea.extent.height = wd->Height;
        rp_begin.clearValueCount          = 1;
        rp_begin.pClearValues             = &wd->ClearValue;
        vkCmdBeginRenderPass(frame->CommandBuffer, &rp_begin,
                             VK_SUBPASS_CONTENTS_INLINE);

        apply_imgui_texture_update_workarounds(vk_state, draw_data);
        ImGui_ImplVulkan_RenderDrawData(draw_data, frame->CommandBuffer);

        vkCmdEndRenderPass(frame->CommandBuffer);
        err = vkEndCommandBuffer(frame->CommandBuffer);
        check_vk_result(err);

        VkPipelineStageFlags wait_stage
            = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit_info         = {};
        submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount   = 1;
        submit_info.pWaitSemaphores      = &image_acquired;
        submit_info.pWaitDstStageMask    = &wait_stage;
        submit_info.commandBufferCount   = 1;
        submit_info.pCommandBuffers      = &frame->CommandBuffer;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores    = &render_complete;
        err = vkQueueSubmit(vk_state.queue, 1, &submit_info, frame->Fence);
        check_vk_result(err);
    }



    void frame_present(VulkanState& vk_state)
    {
        if (vk_state.swapchain_rebuild)
            return;

        ImGui_ImplVulkanH_Window* wd = &vk_state.window_data;
        VkSemaphore render_complete
            = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
        VkPresentInfoKHR present_info   = {};
        present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores    = &render_complete;
        present_info.swapchainCount     = 1;
        present_info.pSwapchains        = &wd->Swapchain;
        present_info.pImageIndices      = &wd->FrameIndex;
        VkResult err = vkQueuePresentKHR(vk_state.queue, &present_info);
        if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
            vk_state.swapchain_rebuild = true;
        if (err == VK_ERROR_OUT_OF_DATE_KHR)
            return;
        if (err != VK_SUBOPTIMAL_KHR)
            check_vk_result(err);

        wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;
    }



    void destroy_texture(VulkanState& vk_state, VulkanTexture& texture)
    {
        if (texture.pixelview_set != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_RemoveTexture(texture.pixelview_set);
            texture.pixelview_set = VK_NULL_HANDLE;
        }
        if (texture.nearest_mag_set != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_RemoveTexture(texture.nearest_mag_set);
            texture.nearest_mag_set = VK_NULL_HANDLE;
        }
        if (texture.set != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_RemoveTexture(texture.set);
            texture.set = VK_NULL_HANDLE;
        }
        if (texture.preview_source_set != VK_NULL_HANDLE
            && vk_state.preview_descriptor_pool != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(vk_state.device,
                                 vk_state.preview_descriptor_pool, 1,
                                 &texture.preview_source_set);
            texture.preview_source_set = VK_NULL_HANDLE;
        }
        if (texture.preview_framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(vk_state.device, texture.preview_framebuffer,
                                 vk_state.allocator);
            texture.preview_framebuffer = VK_NULL_HANDLE;
        }
        if (texture.sampler != VK_NULL_HANDLE) {
            vkDestroySampler(vk_state.device, texture.sampler,
                             vk_state.allocator);
            texture.sampler = VK_NULL_HANDLE;
        }
        if (texture.nearest_mag_sampler != VK_NULL_HANDLE) {
            vkDestroySampler(vk_state.device, texture.nearest_mag_sampler,
                             vk_state.allocator);
            texture.nearest_mag_sampler = VK_NULL_HANDLE;
        }
        if (texture.pixelview_sampler != VK_NULL_HANDLE) {
            vkDestroySampler(vk_state.device, texture.pixelview_sampler,
                             vk_state.allocator);
            texture.pixelview_sampler = VK_NULL_HANDLE;
        }
        if (texture.view != VK_NULL_HANDLE) {
            vkDestroyImageView(vk_state.device, texture.view,
                               vk_state.allocator);
            texture.view = VK_NULL_HANDLE;
        }
        if (texture.image != VK_NULL_HANDLE) {
            vkDestroyImage(vk_state.device, texture.image, vk_state.allocator);
            texture.image = VK_NULL_HANDLE;
        }
        if (texture.memory != VK_NULL_HANDLE) {
            vkFreeMemory(vk_state.device, texture.memory, vk_state.allocator);
            texture.memory = VK_NULL_HANDLE;
        }
        if (texture.source_view != VK_NULL_HANDLE) {
            vkDestroyImageView(vk_state.device, texture.source_view,
                               vk_state.allocator);
            texture.source_view = VK_NULL_HANDLE;
        }
        if (texture.source_image != VK_NULL_HANDLE) {
            vkDestroyImage(vk_state.device, texture.source_image,
                           vk_state.allocator);
            texture.source_image = VK_NULL_HANDLE;
        }
        if (texture.source_memory != VK_NULL_HANDLE) {
            vkFreeMemory(vk_state.device, texture.source_memory,
                         vk_state.allocator);
            texture.source_memory = VK_NULL_HANDLE;
        }
        texture.width                = 0;
        texture.height               = 0;
        texture.preview_initialized  = false;
        texture.preview_dirty        = false;
        texture.preview_params_valid = false;
    }



    bool create_texture(VulkanState& vk_state, const LoadedImage& image,
                        VulkanTexture& texture, std::string& error_message)
    {
        destroy_texture(vk_state, texture);

        if (!vk_state.compute_upload_ready) {
            error_message = "compute upload path is not initialized";
            return false;
        }
        if (image.width <= 0 || image.height <= 0 || image.pixels.empty()) {
            error_message = "invalid image data for texture upload";
            return false;
        }

        UploadDataType upload_type = image.type;
        size_t channel_bytes       = image.channel_bytes;
        size_t row_pitch_bytes     = image.row_pitch_bytes;
        const int channel_count    = std::max(0, image.nchannels);
        if (channel_count <= 0) {
            error_message = "invalid source channel count";
            return false;
        }

        std::vector<unsigned char> converted_pixels;
        const unsigned char* upload_ptr = image.pixels.data();
        size_t upload_bytes             = image.pixels.size();
        bool use_fp64_pipeline          = false;

        if (upload_type == UploadDataType::Double) {
            if (vk_state.compute_pipeline_fp64 != VK_NULL_HANDLE) {
                use_fp64_pipeline = true;
                if (vk_state.verbose_logging) {
                    print("imiv: using fp64 compute upload path for '{}'\n",
                          image.path);
                }
            } else {
                const size_t value_count = image.pixels.size() / sizeof(double);
                converted_pixels.resize(value_count * sizeof(float));
                const double* src = reinterpret_cast<const double*>(
                    image.pixels.data());
                float* dst = reinterpret_cast<float*>(converted_pixels.data());
                for (size_t i = 0; i < value_count; ++i)
                    dst[i] = static_cast<float>(src[i]);
                upload_type     = UploadDataType::Float;
                channel_bytes   = sizeof(float);
                row_pitch_bytes = static_cast<size_t>(image.width)
                                  * static_cast<size_t>(channel_count)
                                  * channel_bytes;
                upload_ptr   = converted_pixels.data();
                upload_bytes = converted_pixels.size();
                print(stderr,
                      "imiv: fp64 compute pipeline unavailable; converting "
                      "double input to float on CPU\n");
            }
        }

        const size_t pixel_stride_bytes = channel_bytes
                                          * static_cast<size_t>(channel_count);
        if (pixel_stride_bytes == 0 || row_pitch_bytes == 0
            || row_pitch_bytes
                   < static_cast<size_t>(image.width) * pixel_stride_bytes) {
            error_message = "invalid source stride for compute upload";
            return false;
        }

        const VkDeviceSize upload_size_aligned = static_cast<VkDeviceSize>(
            (upload_bytes + 3u) & ~size_t(3));

        VkBuffer staging_buffer           = VK_NULL_HANDLE;
        VkDeviceMemory staging_memory     = VK_NULL_HANDLE;
        VkBuffer source_buffer            = VK_NULL_HANDLE;
        VkDeviceMemory source_memory      = VK_NULL_HANDLE;
        VkDescriptorSet compute_set       = VK_NULL_HANDLE;
        VkCommandPool upload_command_pool = VK_NULL_HANDLE;
        VkCommandBuffer upload_command    = VK_NULL_HANDLE;
        bool ok                           = false;

        do {
            VkResult err = VK_SUCCESS;

            VkImageCreateInfo source_ci = {};
            source_ci.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            source_ci.imageType         = VK_IMAGE_TYPE_2D;
            source_ci.format            = vk_state.compute_output_format;
            source_ci.extent.width      = static_cast<uint32_t>(image.width);
            source_ci.extent.height     = static_cast<uint32_t>(image.height);
            source_ci.extent.depth      = 1;
            source_ci.mipLevels         = 1;
            source_ci.arrayLayers       = 1;
            source_ci.samples           = VK_SAMPLE_COUNT_1_BIT;
            source_ci.tiling            = VK_IMAGE_TILING_OPTIMAL;
            source_ci.usage             = VK_IMAGE_USAGE_STORAGE_BIT
                              | VK_IMAGE_USAGE_SAMPLED_BIT;
            source_ci.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
            source_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            err = vkCreateImage(vk_state.device, &source_ci, vk_state.allocator,
                                &texture.source_image);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateImage failed for source image";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_IMAGE,
                               texture.source_image,
                               "imiv.viewer.source_image");

            VkMemoryRequirements source_reqs = {};
            vkGetImageMemoryRequirements(vk_state.device, texture.source_image,
                                         &source_reqs);

            uint32_t image_memory_type = 0;
            if (!find_memory_type(vk_state.physical_device,
                                  source_reqs.memoryTypeBits,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  image_memory_type)) {
                error_message = "no device-local memory type for source image";
                break;
            }

            VkMemoryAllocateInfo source_alloc = {};
            source_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            source_alloc.allocationSize  = source_reqs.size;
            source_alloc.memoryTypeIndex = image_memory_type;
            err = vkAllocateMemory(vk_state.device, &source_alloc,
                                   vk_state.allocator, &texture.source_memory);
            if (err != VK_SUCCESS) {
                error_message = "vkAllocateMemory failed for source image";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_DEVICE_MEMORY,
                               texture.source_memory,
                               "imiv.viewer.source_image.memory");
            err = vkBindImageMemory(vk_state.device, texture.source_image,
                                    texture.source_memory, 0);
            if (err != VK_SUCCESS) {
                error_message = "vkBindImageMemory failed for source image";
                break;
            }

            VkImageViewCreateInfo source_view_ci = {};
            source_view_ci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            source_view_ci.image    = texture.source_image;
            source_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            source_view_ci.format   = vk_state.compute_output_format;
            source_view_ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            source_view_ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            source_view_ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            source_view_ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            source_view_ci.subresourceRange.aspectMask
                = VK_IMAGE_ASPECT_COLOR_BIT;
            source_view_ci.subresourceRange.baseMipLevel   = 0;
            source_view_ci.subresourceRange.levelCount     = 1;
            source_view_ci.subresourceRange.baseArrayLayer = 0;
            source_view_ci.subresourceRange.layerCount     = 1;
            err = vkCreateImageView(vk_state.device, &source_view_ci,
                                    vk_state.allocator, &texture.source_view);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateImageView failed for source image";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_IMAGE_VIEW,
                               texture.source_view, "imiv.viewer.source_view");

            VkImageCreateInfo preview_ci = source_ci;
            preview_ci.usage             = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                               | VK_IMAGE_USAGE_SAMPLED_BIT;
            err = vkCreateImage(vk_state.device, &preview_ci,
                                vk_state.allocator, &texture.image);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateImage failed for preview image";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_IMAGE, texture.image,
                               "imiv.viewer.preview_image");

            VkMemoryRequirements preview_reqs = {};
            vkGetImageMemoryRequirements(vk_state.device, texture.image,
                                         &preview_reqs);
            uint32_t preview_memory_type = 0;
            if (!find_memory_type(vk_state.physical_device,
                                  preview_reqs.memoryTypeBits,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  preview_memory_type)) {
                error_message = "no device-local memory type for preview image";
                break;
            }
            VkMemoryAllocateInfo preview_alloc = {};
            preview_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            preview_alloc.allocationSize  = preview_reqs.size;
            preview_alloc.memoryTypeIndex = preview_memory_type;
            err = vkAllocateMemory(vk_state.device, &preview_alloc,
                                   vk_state.allocator, &texture.memory);
            if (err != VK_SUCCESS) {
                error_message = "vkAllocateMemory failed for preview image";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_DEVICE_MEMORY,
                               texture.memory,
                               "imiv.viewer.preview_image.memory");
            err = vkBindImageMemory(vk_state.device, texture.image,
                                    texture.memory, 0);
            if (err != VK_SUCCESS) {
                error_message = "vkBindImageMemory failed for preview image";
                break;
            }

            VkImageViewCreateInfo preview_view_ci = source_view_ci;
            preview_view_ci.image                 = texture.image;
            err = vkCreateImageView(vk_state.device, &preview_view_ci,
                                    vk_state.allocator, &texture.view);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateImageView failed for preview image";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_IMAGE_VIEW,
                               texture.view, "imiv.viewer.preview_view");

            VkFramebufferCreateInfo fb_ci = {};
            fb_ci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fb_ci.renderPass      = vk_state.preview_render_pass;
            fb_ci.attachmentCount = 1;
            fb_ci.pAttachments    = &texture.view;
            fb_ci.width           = static_cast<uint32_t>(image.width);
            fb_ci.height          = static_cast<uint32_t>(image.height);
            fb_ci.layers          = 1;
            err                   = vkCreateFramebuffer(vk_state.device, &fb_ci,
                                                        vk_state.allocator,
                                                        &texture.preview_framebuffer);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateFramebuffer failed for preview image";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_FRAMEBUFFER,
                               texture.preview_framebuffer,
                               "imiv.viewer.preview_framebuffer");

            VkBufferCreateInfo buffer_ci = {};
            buffer_ci.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_ci.size               = upload_size_aligned;
            buffer_ci.usage              = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                              | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            err                   = vkCreateBuffer(vk_state.device, &buffer_ci,
                                                   vk_state.allocator, &source_buffer);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateBuffer failed for source buffer";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_BUFFER, source_buffer,
                               "imiv.viewer.upload.source_buffer");

            VkMemoryRequirements staging_reqs = {};
            vkGetBufferMemoryRequirements(vk_state.device, source_buffer,
                                          &staging_reqs);

            uint32_t staging_memory_type = 0;
            if (!find_memory_type(vk_state.physical_device,
                                  staging_reqs.memoryTypeBits,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  staging_memory_type)) {
                error_message = "no device-local memory type for source buffer";
                break;
            }

            VkMemoryAllocateInfo staging_alloc = {};
            staging_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            staging_alloc.allocationSize  = staging_reqs.size;
            staging_alloc.memoryTypeIndex = staging_memory_type;
            err = vkAllocateMemory(vk_state.device, &staging_alloc,
                                   vk_state.allocator, &source_memory);
            if (err != VK_SUCCESS) {
                error_message = "vkAllocateMemory failed for source buffer";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_DEVICE_MEMORY,
                               source_memory,
                               "imiv.viewer.upload.source_memory");
            err = vkBindBufferMemory(vk_state.device, source_buffer,
                                     source_memory, 0);
            if (err != VK_SUCCESS) {
                error_message = "vkBindBufferMemory failed for source buffer";
                break;
            }

            VkBufferCreateInfo staging_ci = {};
            staging_ci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            staging_ci.size        = upload_size_aligned;
            staging_ci.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            staging_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            err = vkCreateBuffer(vk_state.device, &staging_ci,
                                 vk_state.allocator, &staging_buffer);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateBuffer failed for staging buffer";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_BUFFER, staging_buffer,
                               "imiv.viewer.upload.staging_buffer");

            VkMemoryRequirements staging_buffer_reqs = {};
            vkGetBufferMemoryRequirements(vk_state.device, staging_buffer,
                                          &staging_buffer_reqs);
            uint32_t host_visible_memory_type = 0;
            if (!find_memory_type(vk_state.physical_device,
                                  staging_buffer_reqs.memoryTypeBits,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  host_visible_memory_type)) {
                error_message = "no host-visible memory type for staging buffer";
                break;
            }
            VkMemoryAllocateInfo host_alloc = {};
            host_alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            host_alloc.allocationSize  = staging_buffer_reqs.size;
            host_alloc.memoryTypeIndex = host_visible_memory_type;
            err = vkAllocateMemory(vk_state.device, &host_alloc,
                                   vk_state.allocator, &staging_memory);
            if (err != VK_SUCCESS) {
                error_message = "vkAllocateMemory failed for staging buffer";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_DEVICE_MEMORY,
                               staging_memory,
                               "imiv.viewer.upload.staging_memory");
            err = vkBindBufferMemory(vk_state.device, staging_buffer,
                                     staging_memory, 0);
            if (err != VK_SUCCESS) {
                error_message = "vkBindBufferMemory failed for staging buffer";
                break;
            }

            void* mapped = nullptr;
            err          = vkMapMemory(vk_state.device, staging_memory, 0,
                                       upload_size_aligned, 0, &mapped);
            if (err != VK_SUCCESS || mapped == nullptr) {
                error_message = "vkMapMemory failed for staging buffer";
                break;
            }
            std::memset(mapped, 0, static_cast<size_t>(upload_size_aligned));
            std::memcpy(mapped, upload_ptr, upload_bytes);
            vkUnmapMemory(vk_state.device, staging_memory);

            VkDescriptorSetAllocateInfo set_alloc = {};
            set_alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            set_alloc.descriptorPool     = vk_state.compute_descriptor_pool;
            set_alloc.descriptorSetCount = 1;
            set_alloc.pSetLayouts = &vk_state.compute_descriptor_set_layout;
            err = vkAllocateDescriptorSets(vk_state.device, &set_alloc,
                                           &compute_set);
            if (err != VK_SUCCESS) {
                error_message
                    = "vkAllocateDescriptorSets failed for upload compute";
                break;
            }

            VkCommandPoolCreateInfo pool_ci = {};
            pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            pool_ci.queueFamilyIndex = vk_state.queue_family;
            err = vkCreateCommandPool(vk_state.device, &pool_ci,
                                      vk_state.allocator, &upload_command_pool);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateCommandPool failed";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_COMMAND_POOL,
                               upload_command_pool,
                               "imiv.viewer.upload.command_pool");

            VkCommandBufferAllocateInfo command_alloc = {};
            command_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            command_alloc.commandPool        = upload_command_pool;
            command_alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            command_alloc.commandBufferCount = 1;
            err = vkAllocateCommandBuffers(vk_state.device, &command_alloc,
                                           &upload_command);
            if (err != VK_SUCCESS) {
                error_message = "vkAllocateCommandBuffers failed";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_COMMAND_BUFFER,
                               upload_command,
                               "imiv.viewer.upload.command_buffer");

            VkDescriptorBufferInfo source_buffer_info = {};
            source_buffer_info.buffer                 = source_buffer;
            source_buffer_info.offset                 = 0;
            source_buffer_info.range                  = upload_size_aligned;

            VkDescriptorImageInfo output_image_info = {};
            output_image_info.imageView             = texture.source_view;
            output_image_info.imageLayout           = VK_IMAGE_LAYOUT_GENERAL;
            output_image_info.sampler               = VK_NULL_HANDLE;

            VkWriteDescriptorSet writes[2] = {};
            writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet          = compute_set;
            writes[0].dstBinding      = 0;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[0].pBufferInfo     = &source_buffer_info;
            writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet          = compute_set;
            writes[1].dstBinding      = 1;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[1].pImageInfo      = &output_image_info;
            vkUpdateDescriptorSets(vk_state.device, 2, writes, 0, nullptr);

            VkSamplerCreateInfo sampler_ci = {};
            sampler_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            VkFormatProperties output_props = {};
            vkGetPhysicalDeviceFormatProperties(vk_state.physical_device,
                                                vk_state.compute_output_format,
                                                &output_props);
            const bool has_linear_filter
                = (output_props.optimalTilingFeatures
                   & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
                  != 0;
            sampler_ci.magFilter     = has_linear_filter ? VK_FILTER_LINEAR
                                                         : VK_FILTER_NEAREST;
            sampler_ci.minFilter     = has_linear_filter ? VK_FILTER_LINEAR
                                                         : VK_FILTER_NEAREST;
            sampler_ci.mipmapMode    = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            sampler_ci.addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_ci.addressModeV  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_ci.addressModeW  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_ci.minLod        = -1000.0f;
            sampler_ci.maxLod        = 1000.0f;
            sampler_ci.maxAnisotropy = 1.0f;
            err = vkCreateSampler(vk_state.device, &sampler_ci,
                                  vk_state.allocator, &texture.sampler);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateSampler failed";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_SAMPLER,
                               texture.sampler, "imiv.viewer.sampler");

            VkSamplerCreateInfo pixelview_sampler_ci   = sampler_ci;
            VkSamplerCreateInfo nearest_mag_sampler_ci = sampler_ci;
            nearest_mag_sampler_ci.magFilter           = VK_FILTER_NEAREST;
            pixelview_sampler_ci.magFilter             = VK_FILTER_NEAREST;
            pixelview_sampler_ci.minFilter             = VK_FILTER_NEAREST;
            pixelview_sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            err = vkCreateSampler(vk_state.device, &nearest_mag_sampler_ci,
                                  vk_state.allocator,
                                  &texture.nearest_mag_sampler);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateSampler failed for nearest-mag view";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_SAMPLER,
                               texture.nearest_mag_sampler,
                               "imiv.viewer.nearest_mag_sampler");
            err = vkCreateSampler(vk_state.device, &pixelview_sampler_ci,
                                  vk_state.allocator,
                                  &texture.pixelview_sampler);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateSampler failed for pixel closeup";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_SAMPLER,
                               texture.pixelview_sampler,
                               "imiv.viewer.pixelview_sampler");

            VkDescriptorSetAllocateInfo preview_set_alloc = {};
            preview_set_alloc.sType
                = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            preview_set_alloc.descriptorPool = vk_state.preview_descriptor_pool;
            preview_set_alloc.descriptorSetCount = 1;
            preview_set_alloc.pSetLayouts
                = &vk_state.preview_descriptor_set_layout;
            err = vkAllocateDescriptorSets(vk_state.device, &preview_set_alloc,
                                           &texture.preview_source_set);
            if (err != VK_SUCCESS) {
                error_message
                    = "vkAllocateDescriptorSets failed for preview source set";
                break;
            }
            VkDescriptorImageInfo preview_source_image = {};
            preview_source_image.sampler               = texture.sampler;
            preview_source_image.imageView             = texture.source_view;
            preview_source_image.imageLayout
                = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkWriteDescriptorSet preview_write = {};
            preview_write.sType      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            preview_write.dstSet     = texture.preview_source_set;
            preview_write.dstBinding = 0;
            preview_write.descriptorCount = 1;
            preview_write.descriptorType
                = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            preview_write.pImageInfo = &preview_source_image;
            vkUpdateDescriptorSets(vk_state.device, 1, &preview_write, 0,
                                   nullptr);

            VkCommandBufferBeginInfo command_begin = {};
            command_begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            command_begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            err = vkBeginCommandBuffer(upload_command, &command_begin);
            if (err != VK_SUCCESS) {
                error_message = "vkBeginCommandBuffer failed";
                break;
            }

            VkBufferCopy copy_region = {};
            copy_region.srcOffset    = 0;
            copy_region.dstOffset    = 0;
            copy_region.size         = upload_size_aligned;
            vkCmdCopyBuffer(upload_command, staging_buffer, source_buffer, 1,
                            &copy_region);

            VkBufferMemoryBarrier source_to_compute = {};
            source_to_compute.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            source_to_compute.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            source_to_compute.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            source_to_compute.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            source_to_compute.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            source_to_compute.buffer              = source_buffer;
            source_to_compute.offset              = 0;
            source_to_compute.size                = upload_size_aligned;

            VkImageMemoryBarrier image_to_general = {};
            image_to_general.sType     = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            image_to_general.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            image_to_general.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            image_to_general.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            image_to_general.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            image_to_general.image               = texture.source_image;
            image_to_general.subresourceRange.aspectMask
                = VK_IMAGE_ASPECT_COLOR_BIT;
            image_to_general.subresourceRange.baseMipLevel   = 0;
            image_to_general.subresourceRange.levelCount     = 1;
            image_to_general.subresourceRange.baseArrayLayer = 0;
            image_to_general.subresourceRange.layerCount     = 1;
            image_to_general.srcAccessMask                   = 0;
            image_to_general.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

            vkCmdPipelineBarrier(upload_command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                                 nullptr, 1, &source_to_compute, 1,
                                 &image_to_general);

            vkCmdBindPipeline(upload_command, VK_PIPELINE_BIND_POINT_COMPUTE,
                              use_fp64_pipeline ? vk_state.compute_pipeline_fp64
                                                : vk_state.compute_pipeline);
            vkCmdBindDescriptorSets(upload_command,
                                    VK_PIPELINE_BIND_POINT_COMPUTE,
                                    vk_state.compute_pipeline_layout, 0, 1,
                                    &compute_set, 0, nullptr);

            UploadComputePushConstants push = {};
            push.width           = static_cast<uint32_t>(image.width);
            push.height          = static_cast<uint32_t>(image.height);
            push.row_pitch_bytes = static_cast<uint32_t>(row_pitch_bytes);
            push.pixel_stride    = static_cast<uint32_t>(pixel_stride_bytes);
            push.channel_count   = static_cast<uint32_t>(channel_count);
            push.data_type       = static_cast<uint32_t>(upload_type);
            vkCmdPushConstants(upload_command, vk_state.compute_pipeline_layout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push),
                               &push);

            const uint32_t group_x = (push.width + 15u) / 16u;
            const uint32_t group_y = (push.height + 15u) / 16u;
            vkCmdDispatch(upload_command, group_x, group_y, 1);

            VkImageMemoryBarrier to_shader = {};
            to_shader.sType     = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            to_shader.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            to_shader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            to_shader.srcQueueFamilyIndex           = VK_QUEUE_FAMILY_IGNORED;
            to_shader.dstQueueFamilyIndex           = VK_QUEUE_FAMILY_IGNORED;
            to_shader.image                         = texture.source_image;
            to_shader.subresourceRange.aspectMask   = VK_IMAGE_ASPECT_COLOR_BIT;
            to_shader.subresourceRange.baseMipLevel = 0;
            to_shader.subresourceRange.levelCount   = 1;
            to_shader.subresourceRange.baseArrayLayer = 0;
            to_shader.subresourceRange.layerCount     = 1;
            to_shader.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            to_shader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(upload_command,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                                 nullptr, 0, nullptr, 1, &to_shader);

            err = vkEndCommandBuffer(upload_command);
            if (err != VK_SUCCESS) {
                error_message = "vkEndCommandBuffer failed";
                break;
            }

            VkSubmitInfo submit_info       = {};
            submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers    = &upload_command;
            err = vkQueueSubmit(vk_state.queue, 1, &submit_info,
                                VK_NULL_HANDLE);
            if (err != VK_SUCCESS) {
                error_message = "vkQueueSubmit failed";
                break;
            }
            err = vkQueueWaitIdle(vk_state.queue);
            if (err != VK_SUCCESS) {
                error_message = "vkQueueWaitIdle failed";
                break;
            }

            texture.set = ImGui_ImplVulkan_AddTexture(
                texture.sampler, texture.view,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            if (texture.set == VK_NULL_HANDLE) {
                error_message = "ImGui_ImplVulkan_AddTexture failed";
                break;
            }
            texture.nearest_mag_set = ImGui_ImplVulkan_AddTexture(
                texture.nearest_mag_sampler, texture.view,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            if (texture.nearest_mag_set == VK_NULL_HANDLE) {
                error_message
                    = "ImGui_ImplVulkan_AddTexture failed for nearest-mag "
                      "view";
                break;
            }
            texture.pixelview_set = ImGui_ImplVulkan_AddTexture(
                texture.pixelview_sampler, texture.view,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            if (texture.pixelview_set == VK_NULL_HANDLE) {
                error_message
                    = "ImGui_ImplVulkan_AddTexture failed for pixel closeup";
                break;
            }

            texture.width                = image.width;
            texture.height               = image.height;
            texture.preview_initialized  = false;
            texture.preview_dirty        = true;
            texture.preview_params_valid = false;
            ok                           = true;

            if (compute_set != VK_NULL_HANDLE) {
                vkFreeDescriptorSets(vk_state.device,
                                     vk_state.compute_descriptor_pool, 1,
                                     &compute_set);
                compute_set = VK_NULL_HANDLE;
            }
        } while (false);

        if (compute_set != VK_NULL_HANDLE)
            vkFreeDescriptorSets(vk_state.device,
                                 vk_state.compute_descriptor_pool, 1,
                                 &compute_set);
        if (upload_command_pool != VK_NULL_HANDLE)
            vkDestroyCommandPool(vk_state.device, upload_command_pool,
                                 vk_state.allocator);
        if (source_buffer != VK_NULL_HANDLE)
            vkDestroyBuffer(vk_state.device, source_buffer, vk_state.allocator);
        if (source_memory != VK_NULL_HANDLE)
            vkFreeMemory(vk_state.device, source_memory, vk_state.allocator);
        if (staging_buffer != VK_NULL_HANDLE)
            vkDestroyBuffer(vk_state.device, staging_buffer,
                            vk_state.allocator);
        if (staging_memory != VK_NULL_HANDLE)
            vkFreeMemory(vk_state.device, staging_memory, vk_state.allocator);
        if (!ok)
            destroy_texture(vk_state, texture);
        return ok;
    }



    bool preview_controls_equal(const PreviewControls& a,
                                const PreviewControls& b)
    {
        return std::abs(a.exposure - b.exposure) < 1.0e-6f
               && std::abs(a.gamma - b.gamma) < 1.0e-6f
               && std::abs(a.offset - b.offset) < 1.0e-6f
               && a.color_mode == b.color_mode && a.channel == b.channel
               && a.use_ocio == b.use_ocio && a.orientation == b.orientation;
    }



    bool update_preview_texture(VulkanState& vk_state, VulkanTexture& texture,
                                const PreviewControls& controls,
                                std::string& error_message)
    {
        if (texture.image == VK_NULL_HANDLE
            || texture.source_image == VK_NULL_HANDLE
            || texture.preview_framebuffer == VK_NULL_HANDLE
            || texture.preview_source_set == VK_NULL_HANDLE)
            return false;

        if (texture.preview_params_valid
            && preview_controls_equal(texture.last_preview_controls, controls)
            && !texture.preview_dirty) {
            return true;
        }

        VkCommandPool command_pool     = VK_NULL_HANDLE;
        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        bool ok                        = false;

        do {
            VkCommandPoolCreateInfo pool_ci = {};
            pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            pool_ci.queueFamilyIndex = vk_state.queue_family;
            VkResult err = vkCreateCommandPool(vk_state.device, &pool_ci,
                                               vk_state.allocator,
                                               &command_pool);
            if (err != VK_SUCCESS) {
                error_message = "vkCreateCommandPool failed for preview update";
                break;
            }

            VkCommandBufferAllocateInfo command_alloc = {};
            command_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            command_alloc.commandPool        = command_pool;
            command_alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            command_alloc.commandBufferCount = 1;
            err = vkAllocateCommandBuffers(vk_state.device, &command_alloc,
                                           &command_buffer);
            if (err != VK_SUCCESS) {
                error_message
                    = "vkAllocateCommandBuffers failed for preview update";
                break;
            }

            VkCommandBufferBeginInfo begin = {};
            begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            err         = vkBeginCommandBuffer(command_buffer, &begin);
            if (err != VK_SUCCESS) {
                error_message = "vkBeginCommandBuffer failed for preview update";
                break;
            }

            VkImageMemoryBarrier to_color_attachment = {};
            to_color_attachment.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            to_color_attachment.oldLayout
                = texture.preview_initialized
                      ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                      : VK_IMAGE_LAYOUT_UNDEFINED;
            to_color_attachment.newLayout
                = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            to_color_attachment.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_color_attachment.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_color_attachment.image               = texture.image;
            to_color_attachment.subresourceRange.aspectMask
                = VK_IMAGE_ASPECT_COLOR_BIT;
            to_color_attachment.subresourceRange.baseMipLevel   = 0;
            to_color_attachment.subresourceRange.levelCount     = 1;
            to_color_attachment.subresourceRange.baseArrayLayer = 0;
            to_color_attachment.subresourceRange.layerCount     = 1;
            to_color_attachment.srcAccessMask = texture.preview_initialized
                                                    ? VK_ACCESS_SHADER_READ_BIT
                                                    : 0;
            to_color_attachment.dstAccessMask
                = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            vkCmdPipelineBarrier(command_buffer,
                                 texture.preview_initialized
                                     ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                                     : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 0, 0, nullptr, 0, nullptr, 1,
                                 &to_color_attachment);

            VkClearValue clear     = {};
            clear.color.float32[0] = 0.0f;
            clear.color.float32[1] = 0.0f;
            clear.color.float32[2] = 0.0f;
            clear.color.float32[3] = 1.0f;

            VkRenderPassBeginInfo rp_begin = {};
            rp_begin.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rp_begin.renderPass  = vk_state.preview_render_pass;
            rp_begin.framebuffer = texture.preview_framebuffer;
            rp_begin.renderArea.offset       = { 0, 0 };
            rp_begin.renderArea.extent.width = static_cast<uint32_t>(
                texture.width);
            rp_begin.renderArea.extent.height = static_cast<uint32_t>(
                texture.height);
            rp_begin.clearValueCount = 1;
            rp_begin.pClearValues    = &clear;

            vkCmdBeginRenderPass(command_buffer, &rp_begin,
                                 VK_SUBPASS_CONTENTS_INLINE);
            VkViewport vp         = {};
            vp.x                  = 0.0f;
            vp.y                  = 0.0f;
            vp.width              = static_cast<float>(texture.width);
            vp.height             = static_cast<float>(texture.height);
            vp.minDepth           = 0.0f;
            vp.maxDepth           = 1.0f;
            VkRect2D scissor      = {};
            scissor.extent.width  = static_cast<uint32_t>(texture.width);
            scissor.extent.height = static_cast<uint32_t>(texture.height);
            vkCmdSetViewport(command_buffer, 0, 1, &vp);
            vkCmdSetScissor(command_buffer, 0, 1, &scissor);
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              vk_state.preview_pipeline);
            vkCmdBindDescriptorSets(command_buffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    vk_state.preview_pipeline_layout, 0, 1,
                                    &texture.preview_source_set, 0, nullptr);

            PreviewPushConstants push = {};
            push.exposure             = controls.exposure;
            push.gamma                = std::max(0.01f, controls.gamma);
            push.offset               = controls.offset;
            push.color_mode           = controls.color_mode;
            push.channel              = controls.channel;
            push.use_ocio             = controls.use_ocio;
            push.orientation          = controls.orientation;
            vkCmdPushConstants(command_buffer, vk_state.preview_pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push),
                               &push);
            vkCmdDraw(command_buffer, 3, 1, 0, 0);
            vkCmdEndRenderPass(command_buffer);

            VkImageMemoryBarrier to_shader_read = {};
            to_shader_read.sType     = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            to_shader_read.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            to_shader_read.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            to_shader_read.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_shader_read.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            to_shader_read.image               = texture.image;
            to_shader_read.subresourceRange.aspectMask
                = VK_IMAGE_ASPECT_COLOR_BIT;
            to_shader_read.subresourceRange.baseMipLevel   = 0;
            to_shader_read.subresourceRange.levelCount     = 1;
            to_shader_read.subresourceRange.baseArrayLayer = 0;
            to_shader_read.subresourceRange.layerCount     = 1;
            to_shader_read.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            to_shader_read.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(command_buffer,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                                 nullptr, 0, nullptr, 1, &to_shader_read);

            err = vkEndCommandBuffer(command_buffer);
            if (err != VK_SUCCESS) {
                error_message = "vkEndCommandBuffer failed for preview update";
                break;
            }

            VkSubmitInfo submit       = {};
            submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit.commandBufferCount = 1;
            submit.pCommandBuffers    = &command_buffer;
            err = vkQueueSubmit(vk_state.queue, 1, &submit, VK_NULL_HANDLE);
            if (err != VK_SUCCESS) {
                error_message = "vkQueueSubmit failed for preview update";
                break;
            }
            err = vkQueueWaitIdle(vk_state.queue);
            if (err != VK_SUCCESS) {
                error_message = "vkQueueWaitIdle failed for preview update";
                break;
            }

            texture.preview_initialized   = true;
            texture.preview_dirty         = false;
            texture.preview_params_valid  = true;
            texture.last_preview_controls = controls;
            ok                            = true;
        } while (false);

        if (command_pool != VK_NULL_HANDLE)
            vkDestroyCommandPool(vk_state.device, command_pool,
                                 vk_state.allocator);
        return ok;
    }
    bool should_reset_preview_on_load(const ViewerState& viewer,
                                      const std::string& path)
    {
        if (path.empty())
            return false;
        if (viewer.image.path.empty())
            return true;
        const std::filesystem::path current_path(viewer.image.path);
        const std::filesystem::path next_path(path);
        return current_path.lexically_normal() != next_path.lexically_normal();
    }

    bool load_viewer_image(VulkanState& vk_state, ViewerState& viewer,
                           PlaceholderUiState* ui_state,
                           const std::string& path, int requested_subimage = 0,
                           int requested_miplevel = 0)
    {
        viewer.last_error.clear();
        LoadedImage loaded;
        std::string error;
        if (!load_image_for_compute(path, requested_subimage,
                                    requested_miplevel, loaded, error)) {
            viewer.last_error = Strutil::fmt::format("open failed: {}", error);
            print(stderr, "imiv: {}\n", viewer.last_error);
            return false;
        }
        VulkanTexture texture;
        if (!create_texture(vk_state, loaded, texture, error)) {
            viewer.last_error = Strutil::fmt::format("upload failed: {}",
                                                     error);
            print(stderr, "imiv: {}\n", viewer.last_error);
            return false;
        }
        destroy_texture(vk_state, viewer.texture);
        if (!viewer.image.path.empty())
            viewer.toggle_image_path = viewer.image.path;
        if (ui_state != nullptr && should_reset_preview_on_load(viewer, path))
            reset_per_image_preview_state(*ui_state);
        viewer.image       = std::move(loaded);
        viewer.texture     = texture;
        viewer.zoom        = 1.0f;
        viewer.fit_request = true;
        reset_view_navigation_state(viewer);
        viewer.probe_valid = false;
        viewer.probe_channels.clear();
        if (viewer.image.width > 0 && viewer.image.height > 0) {
            const int center_x = viewer.image.width / 2;
            const int center_y = viewer.image.height / 2;
            std::vector<double> sample;
            if (sample_loaded_pixel(viewer.image, center_x, center_y, sample)) {
                viewer.probe_valid    = true;
                viewer.probe_x        = center_x;
                viewer.probe_y        = center_y;
                viewer.probe_channels = std::move(sample);
            }
        }
        add_recent_image_path(viewer, viewer.image.path);
        refresh_sibling_images(viewer);
        viewer.status_message = Strutil::fmt::format(
            "Loaded {} ({}x{}, {} channels, {}, subimage {}/{}, mip {}/{})",
            viewer.image.path, viewer.image.width, viewer.image.height,
            viewer.image.nchannels, upload_data_type_name(viewer.image.type),
            viewer.image.subimage + 1, viewer.image.nsubimages,
            viewer.image.miplevel + 1, viewer.image.nmiplevels);
        if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE")
            || env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT")
            || env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP")
            || env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_JUNIT_XML")) {
            print("imiv: {}\n", viewer.status_message);
        }
        return true;
    }

#    if defined(IMGUI_ENABLE_TEST_ENGINE)
    TestEngineRuntime* g_imiv_test_runtime   = nullptr;
    ViewerState* g_imiv_test_viewer          = nullptr;
    PlaceholderUiState* g_imiv_test_ui_state = nullptr;



    bool capture_swapchain_region_rgba8(VulkanState& vk_state, int x, int y,
                                        int w, int h, unsigned int* pixels)
    {
        if (pixels == nullptr || w <= 0 || h <= 0)
            return false;

        ImGui_ImplVulkanH_Window* wd = &vk_state.window_data;
        if (wd->FrameIndex >= static_cast<uint32_t>(wd->Frames.Size))
            return false;
        if (x < 0 || y < 0 || x + w > wd->Width || y + h > wd->Height)
            return false;

        VkImage image = wd->Frames[wd->FrameIndex].Backbuffer;
        if (image == VK_NULL_HANDLE)
            return false;

        const int full_width  = wd->Width;
        const int full_height = wd->Height;
        if (full_width <= 0 || full_height <= 0)
            return false;

        VkResult err = vkQueueWaitIdle(vk_state.queue);
        if (err != VK_SUCCESS)
            return false;

        VkBuffer staging_buffer       = VK_NULL_HANDLE;
        VkDeviceMemory staging_memory = VK_NULL_HANDLE;
        VkCommandPool command_pool    = VK_NULL_HANDLE;
        VkCommandBuffer command_buf   = VK_NULL_HANDLE;
        const VkDeviceSize full_buffer_size
            = static_cast<VkDeviceSize>(full_width)
              * static_cast<VkDeviceSize>(full_height) * 4;
        bool ok = false;

        do {
            VkBufferCreateInfo buffer_ci = {};
            buffer_ci.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_ci.size               = full_buffer_size;
            buffer_ci.usage              = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            buffer_ci.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;
            err = vkCreateBuffer(vk_state.device, &buffer_ci,
                                 vk_state.allocator, &staging_buffer);
            if (err != VK_SUCCESS)
                break;
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_BUFFER, staging_buffer,
                               "imiv.capture.readback.staging_buffer");

            VkMemoryRequirements memory_reqs = {};
            vkGetBufferMemoryRequirements(vk_state.device, staging_buffer,
                                          &memory_reqs);

            uint32_t memory_type = 0;
            if (!find_memory_type(vk_state.physical_device,
                                  memory_reqs.memoryTypeBits,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  memory_type))
                break;

            VkMemoryAllocateInfo alloc_info = {};
            alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            alloc_info.allocationSize  = memory_reqs.size;
            alloc_info.memoryTypeIndex = memory_type;
            err = vkAllocateMemory(vk_state.device, &alloc_info,
                                   vk_state.allocator, &staging_memory);
            if (err != VK_SUCCESS)
                break;
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_DEVICE_MEMORY,
                               staging_memory,
                               "imiv.capture.readback.staging_memory");

            err = vkBindBufferMemory(vk_state.device, staging_buffer,
                                     staging_memory, 0);
            if (err != VK_SUCCESS)
                break;

            VkCommandPoolCreateInfo pool_ci = {};
            pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            pool_ci.queueFamilyIndex = vk_state.queue_family;
            err = vkCreateCommandPool(vk_state.device, &pool_ci,
                                      vk_state.allocator, &command_pool);
            if (err != VK_SUCCESS)
                break;
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_COMMAND_POOL,
                               command_pool, "imiv.capture.command_pool");

            VkCommandBufferAllocateInfo command_alloc = {};
            command_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            command_alloc.commandPool        = command_pool;
            command_alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            command_alloc.commandBufferCount = 1;
            err = vkAllocateCommandBuffers(vk_state.device, &command_alloc,
                                           &command_buf);
            if (err != VK_SUCCESS)
                break;
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_COMMAND_BUFFER,
                               command_buf, "imiv.capture.command_buffer");

            VkCommandBufferBeginInfo begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            err              = vkBeginCommandBuffer(command_buf, &begin_info);
            if (err != VK_SUCCESS)
                break;

            VkImageMemoryBarrier to_transfer = {};
            to_transfer.sType     = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            to_transfer.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            to_transfer.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
            to_transfer.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
            to_transfer.image                       = image;
            to_transfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            to_transfer.subresourceRange.baseMipLevel   = 0;
            to_transfer.subresourceRange.levelCount     = 1;
            to_transfer.subresourceRange.baseArrayLayer = 0;
            to_transfer.subresourceRange.layerCount     = 1;
            to_transfer.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(command_buf,
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                                 0, nullptr, 1, &to_transfer);

            VkBufferImageCopy copy_region           = {};
            copy_region.bufferOffset                = 0;
            copy_region.bufferRowLength             = 0;
            copy_region.bufferImageHeight           = 0;
            copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy_region.imageSubresource.mipLevel   = 0;
            copy_region.imageSubresource.baseArrayLayer = 0;
            copy_region.imageSubresource.layerCount     = 1;
            copy_region.imageOffset.x                   = 0;
            copy_region.imageOffset.y                   = 0;
            copy_region.imageOffset.z                   = 0;
            copy_region.imageExtent.width  = static_cast<uint32_t>(full_width);
            copy_region.imageExtent.height = static_cast<uint32_t>(full_height);
            copy_region.imageExtent.depth  = 1;
            vkCmdCopyImageToBuffer(command_buf, image,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   staging_buffer, 1, &copy_region);

            VkImageMemoryBarrier to_present = {};
            to_present.sType     = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            to_present.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
            to_present.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
            to_present.image                       = image;
            to_present.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            to_present.subresourceRange.baseMipLevel   = 0;
            to_present.subresourceRange.levelCount     = 1;
            to_present.subresourceRange.baseArrayLayer = 0;
            to_present.subresourceRange.layerCount     = 1;
            to_present.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            to_present.dstAccessMask = 0;
            vkCmdPipelineBarrier(command_buf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
                                 nullptr, 0, nullptr, 1, &to_present);

            err = vkEndCommandBuffer(command_buf);
            if (err != VK_SUCCESS)
                break;

            VkSubmitInfo submit_info       = {};
            submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers    = &command_buf;
            err = vkQueueSubmit(vk_state.queue, 1, &submit_info,
                                VK_NULL_HANDLE);
            if (err != VK_SUCCESS)
                break;
            err = vkQueueWaitIdle(vk_state.queue);
            if (err != VK_SUCCESS)
                break;

            void* mapped = nullptr;
            err          = vkMapMemory(vk_state.device, staging_memory, 0,
                                       full_buffer_size, 0, &mapped);
            if (err != VK_SUCCESS || mapped == nullptr)
                break;

            const unsigned char* src = reinterpret_cast<const unsigned char*>(
                mapped);
            unsigned char* dst = reinterpret_cast<unsigned char*>(pixels);
            const size_t src_row_bytes = static_cast<size_t>(full_width) * 4;
            const size_t dst_row_bytes = static_cast<size_t>(w) * 4;
            for (int row = 0; row < h; ++row) {
                const size_t src_offset = (static_cast<size_t>(y + row)
                                           * src_row_bytes)
                                          + static_cast<size_t>(x) * 4;
                std::memcpy(dst + static_cast<size_t>(row) * dst_row_bytes,
                            src + src_offset, dst_row_bytes);
            }
            vkUnmapMemory(vk_state.device, staging_memory);

            VkFormat format        = wd->SurfaceFormat.format;
            const bool bgra_source = (format == VK_FORMAT_B8G8R8A8_UNORM
                                      || format == VK_FORMAT_B8G8R8A8_SRGB);
            if (bgra_source) {
                unsigned char* bytes = reinterpret_cast<unsigned char*>(pixels);
                const size_t pixel_count = static_cast<size_t>(w)
                                           * static_cast<size_t>(h);
                for (size_t i = 0; i < pixel_count; ++i) {
                    unsigned char* px = bytes + i * 4;
                    std::swap(px[0], px[2]);
                }
            }
            ok = true;
        } while (false);

        if (command_pool != VK_NULL_HANDLE)
            vkDestroyCommandPool(vk_state.device, command_pool,
                                 vk_state.allocator);
        if (staging_buffer != VK_NULL_HANDLE)
            vkDestroyBuffer(vk_state.device, staging_buffer,
                            vk_state.allocator);
        if (staging_memory != VK_NULL_HANDLE)
            vkFreeMemory(vk_state.device, staging_memory, vk_state.allocator);
        return ok;
    }



    bool imiv_vulkan_screen_capture(ImGuiID viewport_id, int x, int y, int w,
                                    int h, unsigned int* pixels,
                                    void* user_data)
    {
        (void)viewport_id;
        VulkanState* vk_state = reinterpret_cast<VulkanState*>(user_data);
        if (vk_state == nullptr)
            return false;
        return capture_swapchain_region_rgba8(*vk_state, x, y, w, h, pixels);
    }



    void mark_test_error(ImGuiTestContext* ctx)
    {
        if (ctx && ctx->TestOutput
            && ctx->TestOutput->Status == ImGuiTestStatus_Running) {
            ctx->TestOutput->Status = ImGuiTestStatus_Error;
        }
    }



    void json_write_escaped(FILE* f, const char* s)
    {
        std::fputc('"', f);
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(
                 s ? s : "");
             *p; ++p) {
            unsigned char c = *p;
            switch (c) {
            case '\\': std::fputs("\\\\", f); break;
            case '"': std::fputs("\\\"", f); break;
            case '\b': std::fputs("\\b", f); break;
            case '\f': std::fputs("\\f", f); break;
            case '\n': std::fputs("\\n", f); break;
            case '\r': std::fputs("\\r", f); break;
            case '\t': std::fputs("\\t", f); break;
            default:
                if (c < 0x20)
                    std::fprintf(f, "\\u%04x", static_cast<unsigned int>(c));
                else
                    std::fputc(static_cast<int>(c), f);
                break;
            }
        }
        std::fputc('"', f);
    }



    void json_write_vec2(FILE* f, const ImVec2& v)
    {
        std::fprintf(f, "[%.3f,%.3f]", static_cast<double>(v.x),
                     static_cast<double>(v.y));
    }



    void json_write_rect(FILE* f, const ImRect& r)
    {
        std::fputs("{\"min\":", f);
        json_write_vec2(f, r.Min);
        std::fputs(",\"max\":", f);
        json_write_vec2(f, r.Max);
        std::fputs("}", f);
    }



    bool write_layout_dump_json(ImGuiTestContext* ctx,
                                const std::filesystem::path& out_path,
                                bool include_items, int depth)
    {
        if (depth <= 0)
            depth = 1;

        std::error_code ec;
        if (!out_path.parent_path().empty())
            std::filesystem::create_directories(out_path.parent_path(), ec);

        FILE* f = nullptr;
#        if defined(_WIN32)
        if (fopen_s(&f, out_path.string().c_str(), "wb") != 0)
            f = nullptr;
#        else
        f = std::fopen(out_path.string().c_str(), "wb");
#        endif
        if (!f) {
            ctx->LogError("layout dump: failed to open output file: %s",
                          out_path.string().c_str());
            mark_test_error(ctx);
            return false;
        }

        ImGuiIO& io = ImGui::GetIO();
        struct WindowDumpEntry {
            ImGuiTestItemInfo info;
        };
        std::vector<WindowDumpEntry> windows;
        const char* window_names[] = {
            "##MainMenuBar",  k_image_window_title, "iv Info",
            "iv Preferences", "iv Preview",
        };
        windows.reserve(IM_ARRAYSIZE(window_names));
        for (const char* window_name : window_names) {
            ImGuiTestItemInfo win = ctx->WindowInfo(window_name,
                                                    ImGuiTestOpFlags_NoError);
            if (win.ID == 0 || win.Window == nullptr)
                continue;
            bool duplicate = false;
            for (const WindowDumpEntry& existing : windows) {
                if (existing.info.Window == win.Window) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate)
                windows.push_back({ win });
        }
        if (windows.empty()) {
            std::fclose(f);
            ctx->LogError("layout dump: could not resolve any UI windows");
            mark_test_error(ctx);
            return false;
        }

        std::fputs("{\n", f);
        std::fputs("  \"frame_count\": ", f);
        std::fprintf(f, "%d,\n", ImGui::GetFrameCount());
        std::fputs("  \"display_size\": ", f);
        json_write_vec2(f, io.DisplaySize);
        std::fputs(",\n", f);
        std::fputs("  \"windows\": [\n", f);
        for (size_t wi = 0; wi < windows.size(); ++wi) {
            if (wi > 0)
                std::fputs(",\n", f);
            ImGuiTestItemInfo& win = windows[wi].info;
            std::fputs("    {\"name\": ", f);
            json_write_escaped(f, win.Window->Name);
            std::fputs(", \"id\": ", f);
            std::fprintf(f, "%u", static_cast<unsigned int>(win.Window->ID));
            std::fputs(", \"viewport_id\": ", f);
            std::fprintf(f, "%u",
                         static_cast<unsigned int>(win.Window->ViewportId));
            std::fputs(", \"pos\": ", f);
            json_write_vec2(f, win.Window->Pos);
            std::fputs(", \"size\": ", f);
            json_write_vec2(f, win.Window->Size);
            std::fputs(", \"rect\": ", f);
            json_write_rect(f, win.Window->Rect());
            std::fputs(", \"collapsed\": ", f);
            std::fputs(win.Window->Collapsed ? "true" : "false", f);
            std::fputs(", \"active\": ", f);
            std::fputs(win.Window->Active ? "true" : "false", f);
            std::fputs(", \"was_active\": ", f);
            std::fputs(win.Window->WasActive ? "true" : "false", f);
            std::fputs(", \"hidden\": ", f);
            std::fputs(win.Window->Hidden ? "true" : "false", f);

            if (include_items) {
                std::fputs(", \"items\": [\n", f);
                ImGuiTestItemList list;
                list.Reserve(16384);
                ctx->GatherItems(&list, ImGuiTestRef(win.Window->ID), depth);

                int emitted_items = 0;
                for (int i = 0; i < list.GetSize(); ++i) {
                    const ImGuiTestItemInfo* item = list.GetByIndex(i);
                    if (item == nullptr || item->Window == nullptr)
                        continue;
                    if (emitted_items++ > 0)
                        std::fputs(",\n", f);
                    std::fputs("      {\"id\": ", f);
                    std::fprintf(f, "%u", static_cast<unsigned int>(item->ID));
                    std::fputs(", \"has_id\": ", f);
                    std::fputs(item->ID != 0 ? "true" : "false", f);
                    std::fputs(", \"parent_id\": ", f);
                    std::fprintf(f, "%u",
                                 static_cast<unsigned int>(item->ParentID));
                    std::fputs(", \"depth\": ", f);
                    std::fprintf(f, "%d", static_cast<int>(item->Depth));
                    std::fputs(", \"debug\": ", f);
                    json_write_escaped(f, item->DebugLabel);
                    std::fputs(", \"rect_full\": ", f);
                    json_write_rect(f, item->RectFull);
                    std::fputs(", \"rect_clipped\": ", f);
                    json_write_rect(f, item->RectClipped);
                    std::fputs(", \"item_flags\": ", f);
                    std::fprintf(f, "%u",
                                 static_cast<unsigned int>(item->ItemFlags));
                    std::fputs(", \"status_flags\": ", f);
                    std::fprintf(f, "%u",
                                 static_cast<unsigned int>(item->StatusFlags));
                    std::fputs("}", f);
                }
                if (emitted_items > 0)
                    std::fputs("\n", f);
                std::fputs("    ]", f);
            }
            std::fputs("}", f);
        }
        std::fputs("\n", f);
        std::fputs("  ]\n}\n", f);
        std::fflush(f);
        std::fclose(f);
        ctx->LogInfo("layout dump: wrote %s", out_path.string().c_str());
        return true;
    }

    bool write_viewer_state_json(ImGuiTestContext* ctx,
                                 const std::filesystem::path& out_path)
    {
        if (g_imiv_test_viewer == nullptr || g_imiv_test_ui_state == nullptr) {
            ctx->LogError("state dump: viewer state is unavailable");
            mark_test_error(ctx);
            return false;
        }

        std::error_code ec;
        if (!out_path.parent_path().empty())
            std::filesystem::create_directories(out_path.parent_path(), ec);

        FILE* f = nullptr;
#        if defined(_WIN32)
        if (fopen_s(&f, out_path.string().c_str(), "wb") != 0)
            f = nullptr;
#        else
        f = std::fopen(out_path.string().c_str(), "wb");
#        endif
        if (!f) {
            ctx->LogError("state dump: failed to open output file: %s",
                          out_path.string().c_str());
            mark_test_error(ctx);
            return false;
        }

        const ViewerState& viewer          = *g_imiv_test_viewer;
        const PlaceholderUiState& ui_state = *g_imiv_test_ui_state;
        int display_width                  = viewer.image.width;
        int display_height                 = viewer.image.height;
        if (!viewer.image.path.empty())
            oriented_image_dimensions(viewer.image, display_width,
                                      display_height);

        std::fputs("{\n", f);
        std::fputs("  \"image_loaded\": ", f);
        std::fputs(viewer.image.path.empty() ? "false" : "true", f);
        std::fputs(",\n  \"image_path\": ", f);
        json_write_escaped(f, viewer.image.path.c_str());
        std::fputs(",\n  \"zoom\": ", f);
        std::fprintf(f, "%.6f", static_cast<double>(viewer.zoom));
        std::fputs(",\n  \"scroll\": ", f);
        json_write_vec2(f, viewer.scroll);
        std::fputs(",\n  \"norm_scroll\": ", f);
        json_write_vec2(f, viewer.norm_scroll);
        std::fputs(",\n  \"max_scroll\": ", f);
        json_write_vec2(f, viewer.max_scroll);
        std::fputs(",\n  \"fit_image_to_window\": ", f);
        std::fputs(ui_state.fit_image_to_window ? "true" : "false", f);
        std::fputs(",\n  \"image_size\": [", f);
        std::fprintf(f, "%d,%d", viewer.image.width, viewer.image.height);
        std::fputs("],\n  \"display_size\": [", f);
        std::fprintf(f, "%d,%d", display_width, display_height);
        std::fputs("],\n  \"orientation\": ", f);
        std::fprintf(f, "%d", viewer.image.orientation);
        std::fputs("\n}\n", f);
        std::fflush(f);
        std::fclose(f);
        ctx->LogInfo("state dump: wrote %s", out_path.string().c_str());
        return true;
    }



    void capture_main_viewport_screenshot(ImGuiTestContext* ctx,
                                          const char* out_file)
    {
        ctx->CaptureReset();
        if (out_file && out_file[0] != '\0') {
            std::snprintf(ctx->CaptureArgs->InOutputFile,
                          sizeof(ctx->CaptureArgs->InOutputFile), "%s",
                          out_file);
        }

        ImGuiViewport* vp                   = ImGui::GetMainViewport();
        ctx->CaptureArgs->InCaptureRect.Min = vp->Pos;
        ctx->CaptureArgs->InCaptureRect.Max = ImVec2(vp->Pos.x + vp->Size.x,
                                                     vp->Pos.y + vp->Size.y);
        ctx->CaptureScreenshot(ImGuiCaptureFlags_Instant
                               | ImGuiCaptureFlags_HideMouseCursor);
    }

    void apply_test_engine_mouse_actions(ImGuiTestContext* ctx)
    {
        ImGuiKeyChord key_chord = 0;
        if (env_read_key_chord_value("IMIV_IMGUI_TEST_ENGINE_KEY_CHORD",
                                     key_chord)) {
            ctx->KeyPress(key_chord);
            ctx->Yield(1);
        }

        ImVec2 mouse_pos(0.0f, 0.0f);
        if (resolve_test_engine_mouse_pos(mouse_pos)) {
            ctx->MouseMoveToPos(mouse_pos);
            ctx->Yield(1);
        }

        int click_button = 0;
        if (env_read_int_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_CLICK_BUTTON",
                               click_button)) {
            click_button = std::clamp(click_button, 0, 4);
            ctx->MouseClick(static_cast<ImGuiMouseButton>(click_button));
            ctx->Yield(1);
        }

        float wheel_x = 0.0f;
        float wheel_y = 0.0f;
        const bool have_wheel_x
            = env_read_float_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_WHEEL_X",
                                   wheel_x);
        const bool have_wheel_y
            = env_read_float_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_WHEEL_Y",
                                   wheel_y);
        if (have_wheel_x || have_wheel_y) {
            ctx->MouseWheel(ImVec2(have_wheel_x ? wheel_x : 0.0f,
                                   have_wheel_y ? wheel_y : 0.0f));
            ctx->Yield(1);
        }

        float drag_dx = 0.0f;
        float drag_dy = 0.0f;
        const bool have_drag_dx
            = env_read_float_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_DRAG_DX",
                                   drag_dx);
        const bool have_drag_dy
            = env_read_float_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_DRAG_DY",
                                   drag_dy);
        if (have_drag_dx || have_drag_dy) {
            int drag_button = 0;
            if (!env_read_int_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_DRAG_BUTTON",
                                    drag_button)) {
                drag_button = 0;
            }
            drag_button = std::clamp(drag_button, 0, 4);
            ctx->MouseDragWithDelta(ImVec2(have_drag_dx ? drag_dx : 0.0f,
                                           have_drag_dy ? drag_dy : 0.0f),
                                    static_cast<ImGuiMouseButton>(drag_button));
            ctx->Yield(1);
        }
    }



    void ImivTest_SmokeScreenshot(ImGuiTestContext* ctx)
    {
        int delay_frames = env_int_value(
            "IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_DELAY_FRAMES", 3);
        ctx->Yield(delay_frames);
        apply_test_engine_mouse_actions(ctx);

        int frames_to_capture
            = env_int_value("IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_FRAMES", 1);
        bool save_all = env_flag_is_truthy(
            "IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_SAVE_ALL");

        std::string out_value;
        bool has_out
            = read_env_value("IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_OUT",
                             out_value)
              && !out_value.empty();
        const char* out = has_out ? out_value.c_str() : nullptr;

        bool capture_last_only = (!save_all && frames_to_capture > 1);
        for (int i = 0; i < frames_to_capture; ++i) {
            if (capture_last_only && i + 1 != frames_to_capture) {
                ctx->Yield(1);
                continue;
            }
            if (out) {
                if (save_all && frames_to_capture > 1) {
                    const char* dot = std::strrchr(out, '.');
                    if (dot && dot != out) {
                        size_t base_len     = static_cast<size_t>(dot - out);
                        char out_file[2048] = {};
                        std::snprintf(out_file, sizeof(out_file),
                                      "%.*s_f%03d%s",
                                      static_cast<int>(base_len), out, i, dot);
                        capture_main_viewport_screenshot(ctx, out_file);
                    } else {
                        char out_file[2048] = {};
                        std::snprintf(out_file, sizeof(out_file),
                                      "%s_f%03d.png", out, i);
                        capture_main_viewport_screenshot(ctx, out_file);
                    }
                } else {
                    capture_main_viewport_screenshot(ctx, out);
                }
            } else {
                capture_main_viewport_screenshot(ctx, nullptr);
            }
            ctx->Yield(1);
        }

        if (g_imiv_test_runtime != nullptr)
            g_imiv_test_runtime->request_exit = true;
    }



    void ImivTest_DumpLayoutJson(ImGuiTestContext* ctx)
    {
        int delay_frames
            = env_int_value("IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP_DELAY_FRAMES",
                            3);
        ctx->Yield(delay_frames);
        apply_test_engine_mouse_actions(ctx);

        bool include_items = env_flag_is_truthy(
            "IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP_ITEMS");
        int depth = env_int_value("IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP_DEPTH",
                                  8);
        if (depth <= 0)
            depth = 1;

        std::string out_value;
        if (!read_env_value("IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP_OUT", out_value)
            || out_value.empty()) {
            out_value = "layout.json";
        }

        if (!write_layout_dump_json(ctx, std::filesystem::path(out_value),
                                    include_items, depth)) {
            if (g_imiv_test_runtime != nullptr)
                g_imiv_test_runtime->request_exit = true;
            return;
        }
        if (g_imiv_test_runtime != nullptr)
            g_imiv_test_runtime->request_exit = true;
    }

    void ImivTest_DumpViewerState(ImGuiTestContext* ctx)
    {
        int delay_frames
            = env_int_value("IMIV_IMGUI_TEST_ENGINE_STATE_DUMP_DELAY_FRAMES",
                            3);
        ctx->Yield(delay_frames);
        apply_test_engine_mouse_actions(ctx);
        ctx->Yield(1);

        std::string out_value;
        if (!read_env_value("IMIV_IMGUI_TEST_ENGINE_STATE_DUMP_OUT", out_value)
            || out_value.empty()) {
            out_value = "viewer_state.json";
        }

        if (!write_viewer_state_json(ctx, std::filesystem::path(out_value))) {
            if (g_imiv_test_runtime != nullptr)
                g_imiv_test_runtime->request_exit = true;
            return;
        }
        if (g_imiv_test_runtime != nullptr)
            g_imiv_test_runtime->request_exit = true;
    }



    ImGuiTest* register_imiv_smoke_tests(ImGuiTestEngine* engine)
    {
        ImGuiTest* t = IM_REGISTER_TEST(engine, "imiv", "smoke_screenshot");
        t->TestFunc  = ImivTest_SmokeScreenshot;
        return t;
    }



    ImGuiTest* register_imiv_layout_dump_tests(ImGuiTestEngine* engine)
    {
        ImGuiTest* t = IM_REGISTER_TEST(engine, "imiv", "dump_layout_json");
        t->TestFunc  = ImivTest_DumpLayoutJson;
        return t;
    }

    ImGuiTest* register_imiv_state_dump_tests(ImGuiTestEngine* engine)
    {
        ImGuiTest* t = IM_REGISTER_TEST(engine, "imiv", "dump_viewer_state");
        t->TestFunc  = ImivTest_DumpViewerState;
        return t;
    }



    TestEngineConfig gather_test_engine_config()
    {
        TestEngineConfig cfg;
        cfg.trace = env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_TRACE");
        cfg.auto_screenshot
            = env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT")
              || env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_AUTO_SCREENSHOT");

        std::string layout_out;
        const bool has_layout_out
            = read_env_value("IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP_OUT",
                             layout_out)
              && !layout_out.empty();
        cfg.layout_dump = env_flag_is_truthy(
                              "IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP")
                          || has_layout_out;

        std::string state_out;
        const bool has_state_out
            = read_env_value("IMIV_IMGUI_TEST_ENGINE_STATE_DUMP_OUT", state_out)
              && !state_out.empty();
        cfg.state_dump = env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_STATE_DUMP")
                         || has_state_out;
        cfg.state_dump_out = has_state_out ? state_out : "viewer_state.json";

        std::string junit_out;
        const bool has_junit_out
            = read_env_value("IMIV_IMGUI_TEST_ENGINE_JUNIT_OUT", junit_out)
              && !junit_out.empty();
        cfg.junit_xml = env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_JUNIT_XML")
                        || has_junit_out;
        cfg.junit_xml_out = has_junit_out ? junit_out : "imiv_tests.junit.xml";

        cfg.want_test_engine = env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE")
                               || cfg.auto_screenshot || cfg.layout_dump
                               || cfg.state_dump || cfg.junit_xml;
#        if !defined(NDEBUG)
        cfg.want_test_engine = true;
#        endif
        cfg.automation_mode = cfg.auto_screenshot || cfg.layout_dump
                              || cfg.state_dump;
        cfg.exit_on_finish = env_flag_is_truthy(
                                 "IMIV_IMGUI_TEST_ENGINE_EXIT_ON_FINISH")
                             || cfg.automation_mode;
        cfg.show_windows = false;
        return cfg;
    }
#    endif
#endif



    ImGuiID begin_main_dockspace_host()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags host_flags = ImGuiWindowFlags_NoTitleBar
                                      | ImGuiWindowFlags_NoCollapse
                                      | ImGuiWindowFlags_NoResize
                                      | ImGuiWindowFlags_NoMove
                                      | ImGuiWindowFlags_NoDocking
                                      | ImGuiWindowFlags_NoBringToFrontOnFocus
                                      | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin(k_dockspace_host_title, nullptr, host_flags);
        ImGui::PopStyleVar(3);

        const ImGuiID dockspace_id = ImGui::GetID("imiv.main.dockspace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f),
                         ImGuiDockNodeFlags_None);
        ImGui::End();
        return dockspace_id;
    }



    void setup_image_window_policy(ImGuiID dockspace_id, bool force_dock)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowDockID(dockspace_id, force_dock
                                                     ? ImGuiCond_Always
                                                     : ImGuiCond_FirstUseEver);

        ImGuiWindowClass window_class;
        window_class.ClassId = ImGui::GetID("imiv.image.window");
        window_class.DockNodeFlagsOverrideSet
            = ImGuiDockNodeFlags_NoUndocking
              | ImGuiDockNodeFlags_AutoHideTabBar;
        ImGui::SetNextWindowClass(&window_class);
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
        case 0: return "Zoom";
        case 1: return "Pan";
        case 2: return "Wipe";
        case 3: return "Select";
        case 4: return "Annotate";
        default: break;
        }
        return "Zoom";
    }

    int clamp_orientation(int orientation)
    {
        return std::clamp(orientation, 1, 8);
    }

    bool orientation_swaps_axes(int orientation)
    {
        orientation = clamp_orientation(orientation);
        return orientation == 5 || orientation == 6 || orientation == 7
               || orientation == 8;
    }

    void oriented_image_dimensions(const LoadedImage& image, int& out_width,
                                   int& out_height)
    {
        if (orientation_swaps_axes(image.orientation)) {
            out_width  = image.height;
            out_height = image.width;
        } else {
            out_width  = image.width;
            out_height = image.height;
        }
    }

    ImVec2 source_uv_to_display_uv(const ImVec2& src_uv, int orientation)
    {
        const float u = src_uv.x;
        const float v = src_uv.y;
        switch (clamp_orientation(orientation)) {
        case 1: return ImVec2(u, v);
        case 2: return ImVec2(1.0f - u, v);
        case 3: return ImVec2(1.0f - u, 1.0f - v);
        case 4: return ImVec2(u, 1.0f - v);
        case 5: return ImVec2(v, u);
        case 6: return ImVec2(1.0f - v, u);
        case 7: return ImVec2(1.0f - v, 1.0f - u);
        case 8: return ImVec2(v, 1.0f - u);
        default: break;
        }
        return ImVec2(u, v);
    }

    ImVec2 display_uv_to_source_uv(const ImVec2& display_uv, int orientation)
    {
        const float u = display_uv.x;
        const float v = display_uv.y;
        switch (clamp_orientation(orientation)) {
        case 1: return ImVec2(u, v);
        case 2: return ImVec2(1.0f - u, v);
        case 3: return ImVec2(1.0f - u, 1.0f - v);
        case 4: return ImVec2(u, 1.0f - v);
        case 5: return ImVec2(v, u);
        case 6: return ImVec2(v, 1.0f - u);
        case 7: return ImVec2(1.0f - v, 1.0f - u);
        case 8: return ImVec2(1.0f - v, u);
        default: break;
        }
        return ImVec2(u, v);
    }

    struct ImageCoordinateMap {
        bool valid               = false;
        int source_width         = 0;
        int source_height        = 0;
        int orientation          = 1;
        ImVec2 image_rect_min    = ImVec2(0.0f, 0.0f);  // screen space
        ImVec2 image_rect_max    = ImVec2(0.0f, 0.0f);  // screen space
        ImVec2 viewport_rect_min = ImVec2(0.0f, 0.0f);  // screen space
        ImVec2 viewport_rect_max = ImVec2(0.0f, 0.0f);  // screen space
        ImVec2 window_pos        = ImVec2(0.0f, 0.0f);  // screen space
    };

    ImVec2 screen_to_window_coords(const ImageCoordinateMap& map,
                                   const ImVec2& screen_pos)
    {
        return ImVec2(screen_pos.x - map.window_pos.x,
                      screen_pos.y - map.window_pos.y);
    }

    ImVec2 window_to_screen_coords(const ImageCoordinateMap& map,
                                   const ImVec2& window_pos)
    {
        return ImVec2(window_pos.x + map.window_pos.x,
                      window_pos.y + map.window_pos.y);
    }

    bool screen_to_display_uv(const ImageCoordinateMap& map,
                              const ImVec2& screen_pos, ImVec2& out_display_uv)
    {
        out_display_uv = ImVec2(0.0f, 0.0f);
        if (!map.valid)
            return false;
        const float w = map.image_rect_max.x - map.image_rect_min.x;
        const float h = map.image_rect_max.y - map.image_rect_min.y;
        if (w <= 0.0f || h <= 0.0f)
            return false;
        const float u  = (screen_pos.x - map.image_rect_min.x) / w;
        const float v  = (screen_pos.y - map.image_rect_min.y) / h;
        out_display_uv = ImVec2(u, v);
        return (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f);
    }

    bool screen_to_source_uv(const ImageCoordinateMap& map,
                             const ImVec2& screen_pos, ImVec2& out_source_uv)
    {
        ImVec2 display_uv(0.0f, 0.0f);
        if (!screen_to_display_uv(map, screen_pos, display_uv))
            return false;
        out_source_uv   = display_uv_to_source_uv(display_uv, map.orientation);
        out_source_uv.x = std::clamp(out_source_uv.x, 0.0f, 1.0f);
        out_source_uv.y = std::clamp(out_source_uv.y, 0.0f, 1.0f);
        return true;
    }

    bool source_uv_to_screen(const ImageCoordinateMap& map,
                             const ImVec2& source_uv, ImVec2& out_screen_pos)
    {
        out_screen_pos = ImVec2(0.0f, 0.0f);
        if (!map.valid)
            return false;
        const float w = map.image_rect_max.x - map.image_rect_min.x;
        const float h = map.image_rect_max.y - map.image_rect_min.y;
        if (w <= 0.0f || h <= 0.0f)
            return false;
        const ImVec2 display_uv = source_uv_to_display_uv(source_uv,
                                                          map.orientation);
        out_screen_pos = ImVec2(map.image_rect_min.x + display_uv.x * w,
                                map.image_rect_min.y + display_uv.y * h);
        return true;
    }

    bool point_in_rect(const ImVec2& pos, const ImVec2& rect_min,
                       const ImVec2& rect_max)
    {
        const float min_x = std::min(rect_min.x, rect_max.x);
        const float max_x = std::max(rect_min.x, rect_max.x);
        const float min_y = std::min(rect_min.y, rect_max.y);
        const float max_y = std::max(rect_min.y, rect_max.y);
        return pos.x >= min_x && pos.x <= max_x && pos.y >= min_y
               && pos.y <= max_y;
    }

    ImVec2 clamp_pos_to_rect(const ImVec2& pos, const ImVec2& rect_min,
                             const ImVec2& rect_max)
    {
        const float min_x = std::min(rect_min.x, rect_max.x);
        const float max_x = std::max(rect_min.x, rect_max.x);
        const float min_y = std::min(rect_min.y, rect_max.y);
        const float max_y = std::max(rect_min.y, rect_max.y);
        return ImVec2(std::clamp(pos.x, min_x, max_x),
                      std::clamp(pos.y, min_y, max_y));
    }

    ImVec2 rect_center(const ImVec2& rect_min, const ImVec2& rect_max)
    {
        return ImVec2((rect_min.x + rect_max.x) * 0.5f,
                      (rect_min.y + rect_max.y) * 0.5f);
    }

    ImVec2 rect_size(const ImVec2& rect_min, const ImVec2& rect_max)
    {
        return ImVec2(std::abs(rect_max.x - rect_min.x),
                      std::abs(rect_max.y - rect_min.y));
    }

    struct ImageViewportLayout {
        ImVec2 child_size   = ImVec2(0.0f, 0.0f);
        ImVec2 inner_size   = ImVec2(0.0f, 0.0f);
        ImVec2 content_size = ImVec2(0.0f, 0.0f);
        ImVec2 image_size   = ImVec2(0.0f, 0.0f);
        bool scroll_x       = false;
        bool scroll_y       = false;
    };

    bool viewport_axis_needs_scroll(float image_axis, float inner_axis)
    {
        return image_axis > inner_axis + 0.01f;
    }

    ImageViewportLayout compute_image_viewport_layout(const ImVec2& child_size,
                                                      const ImVec2& padding,
                                                      const ImVec2& image_size,
                                                      float scrollbar_size)
    {
        ImageViewportLayout layout;
        layout.child_size = child_size;
        layout.image_size = image_size;

        const ImVec2 base_inner(std::max(0.0f, child_size.x - padding.x * 2.0f),
                                std::max(0.0f, child_size.y - padding.y * 2.0f));
        bool scroll_x = viewport_axis_needs_scroll(image_size.x, base_inner.x);
        bool scroll_y = viewport_axis_needs_scroll(image_size.y, base_inner.y);
        for (int i = 0; i < 3; ++i) {
            const ImVec2 inner(
                std::max(0.0f,
                         base_inner.x - (scroll_y ? scrollbar_size : 0.0f)),
                std::max(0.0f,
                         base_inner.y - (scroll_x ? scrollbar_size : 0.0f)));
            const bool next_x = viewport_axis_needs_scroll(image_size.x,
                                                           inner.x);
            const bool next_y = viewport_axis_needs_scroll(image_size.y,
                                                           inner.y);
            layout.inner_size = inner;
            if (next_x == scroll_x && next_y == scroll_y)
                break;
            scroll_x = next_x;
            scroll_y = next_y;
        }
        layout.scroll_x       = scroll_x;
        layout.scroll_y       = scroll_y;
        layout.content_size.x = scroll_x ? (image_size.x + layout.inner_size.x)
                                         : layout.inner_size.x;
        layout.content_size.y = scroll_y ? (image_size.y + layout.inner_size.y)
                                         : layout.inner_size.y;
        return layout;
    }

    void sync_view_scroll_from_display_scroll(ViewerState& viewer,
                                              const ImVec2& display_scroll,
                                              const ImVec2& image_size)
    {
        viewer.max_scroll    = image_size;
        viewer.scroll.x      = std::clamp(display_scroll.x, 0.0f,
                                          std::max(0.0f, image_size.x));
        viewer.scroll.y      = std::clamp(display_scroll.y, 0.0f,
                                          std::max(0.0f, image_size.y));
        viewer.norm_scroll.x = (image_size.x > 0.0f)
                                   ? (viewer.scroll.x / image_size.x)
                                   : 0.5f;
        viewer.norm_scroll.y = (image_size.y > 0.0f)
                                   ? (viewer.scroll.y / image_size.y)
                                   : 0.5f;
    }

    void sync_view_scroll_from_source_uv(ViewerState& viewer,
                                         const ImVec2& source_uv,
                                         int orientation,
                                         const ImVec2& image_size)
    {
        const ImVec2 display_uv = source_uv_to_display_uv(
            ImVec2(std::clamp(source_uv.x, 0.0f, 1.0f),
                   std::clamp(source_uv.y, 0.0f, 1.0f)),
            orientation);
        viewer.max_scroll  = image_size;
        viewer.norm_scroll = display_uv;
        viewer.scroll      = ImVec2(display_uv.x * image_size.x,
                                    display_uv.y * image_size.y);
    }

    void queue_zoom_pivot(ViewerState& viewer, const ImVec2& anchor_screen,
                          const ImVec2& source_uv)
    {
        viewer.zoom_pivot_screen = anchor_screen;
        viewer.zoom_pivot_source_uv
            = ImVec2(std::clamp(source_uv.x, 0.0f, 1.0f),
                     std::clamp(source_uv.y, 0.0f, 1.0f));
        viewer.zoom_pivot_pending     = true;
        viewer.zoom_pivot_frames_left = 3;
    }

    struct PendingZoomRequest {
        float scale       = 1.0f;
        bool snap_to_one  = false;
        bool prefer_mouse = false;
    };

    void request_zoom_scale(PendingZoomRequest& request, float scale,
                            bool prefer_mouse)
    {
        request.scale *= scale;
        request.prefer_mouse = request.prefer_mouse || prefer_mouse;
    }

    void request_zoom_reset(PendingZoomRequest& request, bool prefer_mouse)
    {
        request.snap_to_one  = true;
        request.prefer_mouse = request.prefer_mouse || prefer_mouse;
    }

    void recenter_view(ViewerState& viewer, const ImVec2& image_size)
    {
        viewer.zoom_pivot_pending     = false;
        viewer.zoom_pivot_frames_left = 0;
        sync_view_scroll_from_display_scroll(viewer,
                                             ImVec2(image_size.x * 0.5f,
                                                    image_size.y * 0.5f),
                                             image_size);
        viewer.scroll_sync_frames_left
            = std::max(viewer.scroll_sync_frames_left, 2);
    }

    float compute_fit_zoom(const ImVec2& child_size, const ImVec2& padding,
                           int display_width, int display_height)
    {
        if (display_width <= 0 || display_height <= 0)
            return 1.0f;

        const ImVec2 fit_inner(std::max(1.0f, child_size.x - padding.x * 2.0f),
                               std::max(1.0f, child_size.y - padding.y * 2.0f));
        const float fit_x = fit_inner.x / static_cast<float>(display_width);
        const float fit_y = fit_inner.y / static_cast<float>(display_height);
        if (!(fit_x > 0.0f && fit_y > 0.0f))
            return 1.0f;

        const float fit_zoom = std::min(fit_x, fit_y);
        return std::max(0.05f, std::nextafter(fit_zoom, 0.0f));
    }

    void compute_zoom_pivot(const ImageCoordinateMap& map,
                            const ImVec2& mouse_screen,
                            bool prefer_mouse_position,
                            ImVec2& out_anchor_screen, ImVec2& out_source_uv)
    {
        out_anchor_screen = rect_center(map.viewport_rect_min,
                                        map.viewport_rect_max);
        if (prefer_mouse_position
            && point_in_rect(mouse_screen, map.viewport_rect_min,
                             map.viewport_rect_max)) {
            out_anchor_screen = mouse_screen;
        }

        ImVec2 sample_screen = out_anchor_screen;
        if (!point_in_rect(sample_screen, map.image_rect_min,
                           map.image_rect_max)) {
            sample_screen = clamp_pos_to_rect(sample_screen, map.image_rect_min,
                                              map.image_rect_max);
        }

        if (!screen_to_source_uv(map, sample_screen, out_source_uv))
            out_source_uv = ImVec2(0.5f, 0.5f);
    }

    void apply_zoom_request(const ImageCoordinateMap& map, ViewerState& viewer,
                            PlaceholderUiState& ui_state,
                            const PendingZoomRequest& request,
                            const ImVec2& mouse_screen)
    {
        if (!map.valid)
            return;
        if (!request.snap_to_one && std::abs(request.scale - 1.0f) < 1.0e-6f)
            return;

        const float new_zoom = request.snap_to_one
                                   ? 1.0f
                                   : std::clamp(viewer.zoom * request.scale,
                                                0.05f, 64.0f);
        if (std::abs(new_zoom - viewer.zoom) < 1.0e-6f)
            return;

        ImVec2 anchor_screen(0.0f, 0.0f);
        ImVec2 source_uv(0.5f, 0.5f);
        compute_zoom_pivot(map, mouse_screen, request.prefer_mouse,
                           anchor_screen, source_uv);

        viewer.zoom                  = new_zoom;
        ui_state.fit_image_to_window = false;
        viewer.fit_request           = false;
        queue_zoom_pivot(viewer, anchor_screen, source_uv);
    }

    void apply_pending_zoom_pivot(ViewerState& viewer,
                                  const ImageCoordinateMap& map,
                                  const ImVec2& image_size, bool can_scroll_x,
                                  bool can_scroll_y)
    {
        if (!(viewer.zoom_pivot_pending || viewer.zoom_pivot_frames_left > 0))
            return;
        const ImVec2 viewport_center = rect_center(map.viewport_rect_min,
                                                   map.viewport_rect_max);
        const ImVec2 display_uv
            = source_uv_to_display_uv(viewer.zoom_pivot_source_uv,
                                      map.orientation);
        const ImVec2 new_scroll((viewport_center.x - viewer.zoom_pivot_screen.x)
                                    + display_uv.x * image_size.x,
                                (viewport_center.y - viewer.zoom_pivot_screen.y)
                                    + display_uv.y * image_size.y);
        sync_view_scroll_from_display_scroll(viewer, new_scroll, image_size);
        if (can_scroll_x)
            ImGui::SetScrollX(viewer.scroll.x);
        else
            ImGui::SetScrollX(0.0f);
        if (can_scroll_y)
            ImGui::SetScrollY(viewer.scroll.y);
        else
            ImGui::SetScrollY(0.0f);
        viewer.zoom_pivot_pending = false;
        if (viewer.zoom_pivot_frames_left > 0)
            --viewer.zoom_pivot_frames_left;
    }

    bool source_uv_to_pixel(const ImageCoordinateMap& map,
                            const ImVec2& source_uv, int& out_px, int& out_py)
    {
        out_px = 0;
        out_py = 0;
        if (!map.valid || map.source_width <= 0 || map.source_height <= 0)
            return false;

        const float u = std::clamp(source_uv.x, 0.0f, 1.0f);
        const float v = std::clamp(source_uv.y, 0.0f, 1.0f);
        out_px        = std::clamp(static_cast<int>(std::floor(
                                u * static_cast<float>(map.source_width))),
                                   0, map.source_width - 1);
        out_py        = std::clamp(static_cast<int>(std::floor(
                                v * static_cast<float>(map.source_height))),
                                   0, map.source_height - 1);
        return true;
    }



    bool has_supported_image_extension(const std::filesystem::path& path)
    {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), to_lower_ascii);
        return ext == ".exr" || ext == ".tif" || ext == ".tiff" || ext == ".png"
               || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp"
               || ext == ".hdr";
    }

    bool datetime_to_time_t(string_view datetime, std::time_t& out_time)
    {
        int year  = 0;
        int month = 0;
        int day   = 0;
        int hour  = 0;
        int min   = 0;
        int sec   = 0;
        if (!Strutil::scan_datetime(datetime, year, month, day, hour, min, sec))
            return false;

        std::tm tm_value = {};
        tm_value.tm_sec  = sec;
        tm_value.tm_min  = min;
        tm_value.tm_hour = hour;
        tm_value.tm_mday = day;
        tm_value.tm_mon  = month - 1;
        tm_value.tm_year = year - 1900;
        out_time         = std::mktime(&tm_value);
        return out_time != static_cast<std::time_t>(-1);
    }

    bool file_last_write_time(const std::string& path, std::time_t& out_time)
    {
        std::error_code ec;
        const auto file_time = std::filesystem::last_write_time(path, ec);
        if (ec)
            return false;
        const auto system_time
            = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                file_time - std::filesystem::file_time_type::clock::now()
                + std::chrono::system_clock::now());
        out_time = std::chrono::system_clock::to_time_t(system_time);
        return true;
    }

    bool image_datetime(const std::string& path, std::time_t& out_time)
    {
        auto input = ImageInput::open(path);
        if (!input)
            return false;
        const ImageSpec spec = input->spec();
        input->close();

        const std::string datetime = spec.get_string_attribute("DateTime");
        if (datetime.empty())
            return false;
        return datetime_to_time_t(datetime, out_time);
    }

    void sort_sibling_images(ViewerState& viewer)
    {
        if (viewer.sibling_images.empty())
            return;

        auto filename_key = [](const std::string& path) {
            return std::filesystem::path(path).filename().string();
        };
        auto path_key = [](const std::string& path) {
            return std::filesystem::path(path).lexically_normal().string();
        };

        switch (viewer.sort_mode) {
        case ImageSortMode::ByName:
            std::sort(viewer.sibling_images.begin(),
                      viewer.sibling_images.end(),
                      [&](const std::string& a, const std::string& b) {
                          const std::string a_name = filename_key(a);
                          const std::string b_name = filename_key(b);
                          if (a_name == b_name)
                              return path_key(a) < path_key(b);
                          return a_name < b_name;
                      });
            break;
        case ImageSortMode::ByPath:
            std::sort(viewer.sibling_images.begin(),
                      viewer.sibling_images.end(),
                      [&](const std::string& a, const std::string& b) {
                          return path_key(a) < path_key(b);
                      });
            break;
        case ImageSortMode::ByImageDate:
            std::sort(viewer.sibling_images.begin(),
                      viewer.sibling_images.end(),
                      [&](const std::string& a, const std::string& b) {
                          std::time_t a_time = {};
                          std::time_t b_time = {};
                          const bool a_ok    = image_datetime(a, a_time)
                                            || file_last_write_time(a, a_time);
                          const bool b_ok = image_datetime(b, b_time)
                                            || file_last_write_time(b, b_time);
                          if (a_ok != b_ok)
                              return a_ok;
                          if (!a_ok && !b_ok)
                              return filename_key(a) < filename_key(b);
                          if (a_time == b_time)
                              return filename_key(a) < filename_key(b);
                          return a_time < b_time;
                      });
            break;
        case ImageSortMode::ByFileDate:
            std::sort(viewer.sibling_images.begin(),
                      viewer.sibling_images.end(),
                      [&](const std::string& a, const std::string& b) {
                          std::time_t a_time = {};
                          std::time_t b_time = {};
                          const bool a_ok    = file_last_write_time(a, a_time);
                          const bool b_ok    = file_last_write_time(b, b_time);
                          if (a_ok != b_ok)
                              return a_ok;
                          if (!a_ok && !b_ok)
                              return filename_key(a) < filename_key(b);
                          if (a_time == b_time)
                              return filename_key(a) < filename_key(b);
                          return a_time < b_time;
                      });
            break;
        }

        if (viewer.sort_reverse)
            std::reverse(viewer.sibling_images.begin(),
                         viewer.sibling_images.end());

        if (viewer.image.path.empty()) {
            viewer.sibling_index = -1;
            return;
        }
        auto it = std::find(viewer.sibling_images.begin(),
                            viewer.sibling_images.end(), viewer.image.path);
        viewer.sibling_index
            = (it != viewer.sibling_images.end())
                  ? static_cast<int>(
                        std::distance(viewer.sibling_images.begin(), it))
                  : -1;
    }



    void refresh_sibling_images(ViewerState& viewer)
    {
        viewer.sibling_images.clear();
        viewer.sibling_index = -1;
        if (viewer.image.path.empty())
            return;

        std::filesystem::path current(viewer.image.path);
        std::error_code ec;
        const std::filesystem::path dir = current.parent_path();
        if (dir.empty() || !std::filesystem::exists(dir, ec))
            return;

        for (const std::filesystem::directory_entry& entry :
             std::filesystem::directory_iterator(dir, ec)) {
            if (ec)
                break;
            if (!entry.is_regular_file(ec))
                continue;
            if (!has_supported_image_extension(entry.path()))
                continue;
            viewer.sibling_images.emplace_back(entry.path().string());
        }
        sort_sibling_images(viewer);
    }



    bool pick_sibling_image(const ViewerState& viewer, int delta,
                            std::string& out_path)
    {
        out_path.clear();
        if (viewer.sibling_images.empty() || viewer.sibling_index < 0)
            return false;
        const int count = static_cast<int>(viewer.sibling_images.size());
        int idx         = viewer.sibling_index + delta;
        while (idx < 0)
            idx += count;
        idx %= count;
        out_path = viewer.sibling_images[static_cast<size_t>(idx)];
        return !out_path.empty();
    }



    std::string normalize_recent_path(const std::string& path)
    {
        if (path.empty())
            return std::string();
        std::filesystem::path p(path);
        std::error_code ec;
        if (!p.is_absolute()) {
            std::filesystem::path abs = std::filesystem::absolute(p, ec);
            if (!ec)
                p = abs;
        }
        return p.lexically_normal().string();
    }



    void add_recent_image_path(ViewerState& viewer, const std::string& path)
    {
        const std::string normalized = normalize_recent_path(path);
        if (normalized.empty())
            return;

        auto it = std::remove(viewer.recent_images.begin(),
                              viewer.recent_images.end(), normalized);
        viewer.recent_images.erase(it, viewer.recent_images.end());
        viewer.recent_images.insert(viewer.recent_images.begin(), normalized);
        if (viewer.recent_images.size() > k_max_recent_images)
            viewer.recent_images.resize(k_max_recent_images);
    }



    void set_placeholder_status(ViewerState& viewer, const char* action)
    {
        viewer.status_message = Strutil::fmt::format("{} (not implemented yet)",
                                                     action);
        viewer.last_error.clear();
    }



    std::string parent_directory_for_dialog(const std::string& path)
    {
        if (path.empty())
            return std::string();
        std::filesystem::path p(path);
        if (!p.has_parent_path())
            return std::string();
        return p.parent_path().string();
    }



    std::string open_dialog_default_path(const ViewerState& viewer)
    {
        if (!viewer.image.path.empty())
            return parent_directory_for_dialog(viewer.image.path);
        if (!viewer.recent_images.empty())
            return parent_directory_for_dialog(viewer.recent_images.front());
        return std::string();
    }



    std::string save_dialog_default_name(const ViewerState& viewer)
    {
        if (viewer.image.path.empty())
            return "image.exr";
        std::filesystem::path p(viewer.image.path);
        if (p.filename().empty())
            return "image.exr";
        return p.filename().string();
    }



    bool save_loaded_image(const LoadedImage& image, const std::string& path,
                           std::string& error_message)
    {
        error_message.clear();
        if (path.empty()) {
            error_message = "save path is empty";
            return false;
        }
        if (image.width <= 0 || image.height <= 0 || image.nchannels <= 0) {
            error_message = "no valid image is loaded";
            return false;
        }

        const TypeDesc format = upload_data_type_to_typedesc(image.type);
        if (format == TypeUnknown) {
            error_message = "unsupported source pixel type for save";
            return false;
        }

        const size_t width         = static_cast<size_t>(image.width);
        const size_t height        = static_cast<size_t>(image.height);
        const size_t channels      = static_cast<size_t>(image.nchannels);
        const size_t min_row_pitch = width * channels * image.channel_bytes;
        if (image.row_pitch_bytes < min_row_pitch) {
            error_message = "image row pitch is invalid";
            return false;
        }
        const size_t required_bytes = image.row_pitch_bytes * height;
        if (image.pixels.size() < required_bytes) {
            error_message = "image pixel buffer is incomplete";
            return false;
        }

        ImageSpec spec(image.width, image.height, image.nchannels, format);
        ImageBuf output(spec);

        const std::byte* begin = reinterpret_cast<const std::byte*>(
            image.pixels.data());
        const cspan<std::byte> byte_span(begin, image.pixels.size());
        const stride_t xstride = static_cast<stride_t>(image.nchannels
                                                       * image.channel_bytes);
        const stride_t ystride = static_cast<stride_t>(image.row_pitch_bytes);
        if (!output.set_pixels(ROI::All(), format, byte_span, begin, xstride,
                               ystride, AutoStride)) {
            error_message = output.geterror();
            if (error_message.empty())
                error_message = "failed to copy pixels into save buffer";
            return false;
        }

        if (!output.write(path, format)) {
            error_message = output.geterror();
            if (error_message.empty())
                error_message = "image write failed";
            return false;
        }
        return true;
    }



    void save_as_dialog_action(ViewerState& viewer)
    {
        if (viewer.image.path.empty()) {
            viewer.last_error = "No image loaded to save";
            return;
        }

        const std::string default_path = open_dialog_default_path(viewer);
        const std::string default_name = save_dialog_default_name(viewer);
        FileDialog::DialogReply reply
            = FileDialog::save_image_file(default_path, default_name);
        if (reply.result == FileDialog::Result::Okay) {
            std::string error;
            if (save_loaded_image(viewer.image, reply.path, error)) {
                add_recent_image_path(viewer, reply.path);
                viewer.status_message = Strutil::fmt::format("Saved {}",
                                                             reply.path);
                viewer.last_error.clear();
            } else {
                viewer.last_error = Strutil::fmt::format("save failed: {}",
                                                         error);
            }
        } else if (reply.result == FileDialog::Result::Cancel) {
            viewer.status_message = "Save cancelled";
            viewer.last_error.clear();
        } else {
            viewer.last_error = reply.message.empty() ? "Save dialog failed"
                                                      : reply.message;
        }
    }



#if defined(IMIV_BACKEND_VULKAN_GLFW)
    void set_full_screen_mode(GLFWwindow* window, ViewerState& viewer,
                              bool enable, std::string& error_message)
    {
        error_message.clear();
        if (window == nullptr)
            return;
        if (enable == viewer.fullscreen_applied)
            return;

        if (enable) {
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            if (monitor == nullptr) {
                error_message = "fullscreen failed: no primary monitor";
                return;
            }
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            if (mode == nullptr) {
                error_message = "fullscreen failed: monitor mode unavailable";
                return;
            }

            glfwGetWindowPos(window, &viewer.windowed_x, &viewer.windowed_y);
            glfwGetWindowSize(window, &viewer.windowed_width,
                              &viewer.windowed_height);
            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width,
                                 mode->height, mode->refreshRate);
            viewer.fullscreen_applied = true;
            return;
        }

        const int restore_w = std::max(320, viewer.windowed_width);
        const int restore_h = std::max(240, viewer.windowed_height);
        glfwSetWindowMonitor(window, nullptr, viewer.windowed_x,
                             viewer.windowed_y, restore_w, restore_h, 0);
        viewer.fullscreen_applied = false;
    }

    void fit_window_to_image_action(GLFWwindow* window, ViewerState& viewer,
                                    PlaceholderUiState& ui_state)
    {
        if (window == nullptr || viewer.image.path.empty())
            return;
        if (viewer.fullscreen_applied || ui_state.full_screen_mode)
            return;

        int window_w = 0;
        int window_h = 0;
        int fb_w     = 0;
        int fb_h     = 0;
        glfwGetWindowSize(window, &window_w, &window_h);
        glfwGetFramebufferSize(window, &fb_w, &fb_h);

        const float scale_x = (fb_w > 0) ? (static_cast<float>(window_w) / fb_w)
                                         : 1.0f;
        const float scale_y = (fb_h > 0) ? (static_cast<float>(window_h) / fb_h)
                                         : 1.0f;

        constexpr int k_view_padding_px = 24;
        constexpr int k_ui_overhead_px  = 120;
        int display_width               = viewer.image.width;
        int display_height              = viewer.image.height;
        oriented_image_dimensions(viewer.image, display_width, display_height);
        const int target_fb_w = std::max(320,
                                         display_width + k_view_padding_px);
        const int target_fb_h = std::max(240,
                                         display_height + k_ui_overhead_px);
        const int target_w    = static_cast<int>(
            std::round(static_cast<float>(target_fb_w) * scale_x));
        const int target_h = static_cast<int>(
            std::round(static_cast<float>(target_fb_h) * scale_y));

        glfwSetWindowSize(window, target_w, target_h);
        ui_state.fit_image_to_window = false;
        viewer.zoom                  = 1.0f;
        viewer.fit_request           = false;
        viewer.status_message
            = Strutil::fmt::format("Fit window to image: {}x{}", target_w,
                                   target_h);
        viewer.last_error.clear();
    }

    void save_window_as_dialog_action(ViewerState& viewer)
    {
        save_as_dialog_action(viewer);
    }

    void save_selection_as_dialog_action(ViewerState& viewer)
    {
        save_as_dialog_action(viewer);
    }

    void set_sort_mode_action(ViewerState& viewer, ImageSortMode mode)
    {
        viewer.sort_mode = mode;
        sort_sibling_images(viewer);
        viewer.status_message = "Image list sort mode changed";
        viewer.last_error.clear();
    }

    void toggle_sort_reverse_action(ViewerState& viewer)
    {
        viewer.sort_reverse = !viewer.sort_reverse;
        sort_sibling_images(viewer);
        viewer.status_message = viewer.sort_reverse
                                    ? "Image list order reversed"
                                    : "Image list order restored";
        viewer.last_error.clear();
    }

    bool advance_slide_show_action(VulkanState& vk_state, ViewerState& viewer,
                                   PlaceholderUiState& ui_state)
    {
        if (!ui_state.slide_show_running || viewer.sibling_images.empty()
            || viewer.image.path.empty()) {
            return false;
        }

        const int count = static_cast<int>(viewer.sibling_images.size());
        if (count <= 0 || viewer.sibling_index < 0)
            return false;

        if (!ui_state.slide_loop && viewer.sibling_index >= count - 1) {
            ui_state.slide_show_running = false;
            viewer.status_message       = "Slide show reached final image";
            viewer.last_error.clear();
            return false;
        }

        std::string next_path;
        if (!pick_sibling_image(viewer, 1, next_path) || next_path.empty())
            return false;
        return load_viewer_image(vk_state, viewer, &ui_state, next_path,
                                 viewer.image.subimage, viewer.image.miplevel);
    }

    void toggle_slide_show_action(PlaceholderUiState& ui_state,
                                  ViewerState& viewer)
    {
        ui_state.slide_show_running = !ui_state.slide_show_running;
        if (ui_state.slide_show_running)
            ui_state.full_screen_mode = true;
        viewer.slide_last_advance_time = ImGui::GetTime();
        viewer.status_message          = ui_state.slide_show_running
                                             ? "Slide show started"
                                             : "Slide show stopped";
        viewer.last_error.clear();
    }

    void open_image_dialog_action(VulkanState& vk_state, ViewerState& viewer,
                                  PlaceholderUiState& ui_state,
                                  int requested_subimage,
                                  int requested_miplevel)
    {
        FileDialog::DialogReply reply = FileDialog::open_image_file(
            open_dialog_default_path(viewer));
        if (reply.result == FileDialog::Result::Okay) {
            load_viewer_image(vk_state, viewer, &ui_state, reply.path,
                              requested_subimage, requested_miplevel);
        } else if (reply.result == FileDialog::Result::Cancel) {
            viewer.status_message = "Open cancelled";
            viewer.last_error.clear();
        } else {
            viewer.last_error = reply.message;
        }
    }



    void reload_current_image_action(VulkanState& vk_state, ViewerState& viewer,
                                     PlaceholderUiState& ui_state)
    {
        if (viewer.image.path.empty()) {
            viewer.status_message = "No image loaded";
            viewer.last_error.clear();
            return;
        }
        load_viewer_image(vk_state, viewer, &ui_state, viewer.image.path,
                          viewer.image.subimage, viewer.image.miplevel);
    }



    void close_current_image_action(VulkanState& vk_state, ViewerState& viewer)
    {
        destroy_texture(vk_state, viewer.texture);
        if (!viewer.image.path.empty())
            viewer.toggle_image_path = viewer.image.path;
        viewer.image       = LoadedImage();
        viewer.zoom        = 1.0f;
        viewer.fit_request = true;
        reset_view_navigation_state(viewer);
        viewer.probe_valid = false;
        viewer.probe_channels.clear();
        viewer.sibling_images.clear();
        viewer.sibling_index = -1;
        viewer.last_error.clear();
        viewer.status_message = "Closed current image";
    }



    void next_sibling_image_action(VulkanState& vk_state, ViewerState& viewer,
                                   PlaceholderUiState& ui_state, int delta)
    {
        std::string path;
        if (!pick_sibling_image(viewer, delta, path)) {
            viewer.status_message = (delta < 0) ? "Previous image unavailable"
                                                : "Next image unavailable";
            viewer.last_error.clear();
            return;
        }
        load_viewer_image(vk_state, viewer, &ui_state, path,
                          viewer.image.subimage, viewer.image.miplevel);
    }



    void toggle_image_action(VulkanState& vk_state, ViewerState& viewer,
                             PlaceholderUiState& ui_state)
    {
        if (viewer.toggle_image_path.empty()) {
            viewer.status_message = "No toggled image available";
            viewer.last_error.clear();
            return;
        }
        if (viewer.image.path == viewer.toggle_image_path) {
            if (!pick_sibling_image(viewer, 1, viewer.toggle_image_path)) {
                viewer.status_message = "No toggled image available";
                viewer.last_error.clear();
                return;
            }
        }
        load_viewer_image(vk_state, viewer, &ui_state, viewer.toggle_image_path,
                          viewer.image.subimage, viewer.image.miplevel);
    }



    void change_subimage_action(VulkanState& vk_state, ViewerState& viewer,
                                PlaceholderUiState& ui_state, int delta)
    {
        if (viewer.image.path.empty()) {
            viewer.status_message = "No image loaded";
            viewer.last_error.clear();
            return;
        }
        const int target_subimage = viewer.image.subimage + delta;
        if (target_subimage < 0 || target_subimage >= viewer.image.nsubimages)
            return;
        load_viewer_image(vk_state, viewer, &ui_state, viewer.image.path,
                          target_subimage, viewer.image.miplevel);
    }



    void change_miplevel_action(VulkanState& vk_state, ViewerState& viewer,
                                PlaceholderUiState& ui_state, int delta)
    {
        if (viewer.image.path.empty()) {
            viewer.status_message = "No image loaded";
            viewer.last_error.clear();
            return;
        }
        const int target_mip = viewer.image.miplevel + delta;
        if (target_mip < 0 || target_mip >= viewer.image.nmiplevels)
            return;
        load_viewer_image(vk_state, viewer, &ui_state, viewer.image.path,
                          viewer.image.subimage, target_mip);
    }
#endif



    void draw_padded_message(const char* message, float x_pad = 10.0f,
                             float y_pad = 6.0f)
    {
        if (!message || message[0] == '\0')
            return;
        ImVec2 pos = ImGui::GetCursorPos();
        pos.x += x_pad;
        pos.y += y_pad;
        ImGui::SetCursorPos(pos);
        const float wrap_width
            = ImGui::GetCursorPosX()
              + std::max(64.0f, ImGui::GetContentRegionAvail().x - x_pad);
        ImGui::PushTextWrapPos(wrap_width);
        ImGui::TextUnformatted(message);
        ImGui::PopTextWrapPos();
    }



    std::string format_probe_channel_value(UploadDataType type, double value)
    {
        switch (type) {
        case UploadDataType::UInt8:
        case UploadDataType::UInt16:
        case UploadDataType::UInt32:
            return Strutil::fmt::format("{:.0f}", value);
        case UploadDataType::Half:
        case UploadDataType::Float:
            return Strutil::fmt::format("{:.7g}", value);
        case UploadDataType::Double:
            return Strutil::fmt::format("{:.12g}", value);
        default: break;
        }
        return Strutil::fmt::format("{:.7g}", value);
    }



    std::string format_probe_fixed3(double value)
    {
        return Strutil::fmt::format("{:.3f}", value);
    }



    std::string format_probe_iv_float(double value)
    {
        if (value < 10.0)
            return Strutil::fmt::format("{:.3f}", value);
        if (value < 100.0)
            return Strutil::fmt::format("{:.2f}", value);
        if (value < 1000.0)
            return Strutil::fmt::format("{:.1f}", value);
        return Strutil::fmt::format("{:.0f}", value);
    }



    std::string format_probe_integer_trunc(UploadDataType type, double value)
    {
        switch (type) {
        case UploadDataType::UInt8:
            return Strutil::fmt::format("{}",
                                        static_cast<unsigned int>(
                                            std::clamp(value, 0.0, 255.0)));
        case UploadDataType::UInt16:
            return Strutil::fmt::format("{}",
                                        static_cast<unsigned int>(
                                            std::clamp(value, 0.0, 65535.0)));
        case UploadDataType::UInt32:
            return Strutil::fmt::format("{}", static_cast<uint32_t>(
                                                  std::clamp(value, 0.0,
                                                             4294967295.0)));
        default: break;
        }
        return Strutil::fmt::format("{:.0f}", value);
    }


    bool probe_type_is_integer(UploadDataType type)
    {
        return type == UploadDataType::UInt8 || type == UploadDataType::UInt16;
    }



    double probe_channel_integer_denominator(UploadDataType type)
    {
        switch (type) {
        case UploadDataType::UInt8: return 255.0;
        case UploadDataType::UInt16: return 65535.0;
        case UploadDataType::UInt32: return 4294967295.0;
        default: break;
        }
        return 1.0;
    }



    struct OverlayPanelRect {
        bool valid = false;
        ImVec2 min = ImVec2(0.0f, 0.0f);
        ImVec2 max = ImVec2(0.0f, 0.0f);
    };

    OverlayPanelRect
    draw_overlay_text_panel(const std::vector<std::string>& lines,
                            const ImVec2& preferred_pos, const ImVec2& clip_min,
                            const ImVec2& clip_max, ImFont* font = nullptr)
    {
        OverlayPanelRect panel;
        if (lines.empty())
            return panel;

        ImFont* draw_font     = font ? font : ImGui::GetFont();
        const float font_size = draw_font ? draw_font->LegacySize
                                          : ImGui::GetFontSize();
        const float pad_x     = 10.0f;
        const float pad_y     = 8.0f;
        const float line_gap  = 2.0f;
        const float line_h    = draw_font ? draw_font->LegacySize
                                          : ImGui::GetTextLineHeight();

        float text_w = 0.0f;
        for (const std::string& line : lines) {
            const ImVec2 size
                = draw_font->CalcTextSizeA(font_size,
                                           std::numeric_limits<float>::max(),
                                           0.0f, line.c_str());
            if (size.x > text_w)
                text_w = size.x;
        }
        const float panel_w = text_w + pad_x * 2.0f;
        const float panel_h = pad_y * 2.0f
                              + static_cast<float>(lines.size()) * line_h
                              + static_cast<float>(lines.size() - 1) * line_gap;

        const float min_x = std::min(clip_min.x, clip_max.x);
        const float min_y = std::min(clip_min.y, clip_max.y);
        const float max_x = std::max(clip_min.x, clip_max.x);
        const float max_y = std::max(clip_min.y, clip_max.y);
        if ((max_x - min_x) < panel_w || (max_y - min_y) < panel_h)
            return panel;

        ImVec2 pos = preferred_pos;
        pos.x      = std::clamp(pos.x, min_x, std::max(min_x, max_x - panel_w));
        pos.y      = std::clamp(pos.y, min_y, std::max(min_y, max_y - panel_h));

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->PushClipRect(clip_min, clip_max, true);
        draw_list->AddRectFilled(pos, ImVec2(pos.x + panel_w, pos.y + panel_h),
                                 IM_COL32(20, 24, 30, 224), 4.0f);
        draw_list->AddRect(pos, ImVec2(pos.x + panel_w, pos.y + panel_h),
                           IM_COL32(175, 185, 205, 255), 4.0f, 0, 1.0f);

        ImVec2 text_pos(pos.x + pad_x, pos.y + pad_y);
        for (const std::string& line : lines) {
            draw_list->AddText(draw_font, font_size, text_pos,
                               IM_COL32(240, 242, 245, 255), line.c_str());
            text_pos.y += line_h + line_gap;
        }
        draw_list->PopClipRect();

        panel.valid = true;
        panel.min   = pos;
        panel.max   = ImVec2(pos.x + panel_w, pos.y + panel_h);
        return panel;
    }

    void set_aux_window_defaults(const ImVec2& offset, const ImVec2& size)
    {
        const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
        ImVec2 base_pos(0.0f, 0.0f);
        if (main_viewport != nullptr)
            base_pos = main_viewport->WorkPos;
        ImGui::SetNextWindowPos(ImVec2(base_pos.x + offset.x,
                                       base_pos.y + offset.y),
                                ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
    }



    void draw_info_window(const ViewerState& viewer, bool& show_window)
    {
        if (!show_window)
            return;
        set_aux_window_defaults(ImVec2(72.0f, 72.0f), ImVec2(640.0f, 420.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
        if (ImGui::Begin("iv Info", &show_window)) {
            const float close_height = ImGui::GetFrameHeightWithSpacing();
            const float body_height  = std::max(100.0f,
                                                ImGui::GetContentRegionAvail().y
                                                    - close_height - 4.0f);
            ImGui::BeginChild("##iv_info_scroll", ImVec2(0.0f, body_height),
                              true, ImGuiWindowFlags_HorizontalScrollbar);
            if (viewer.image.path.empty()) {
                draw_padded_message("No image loaded.", 8.0f, 8.0f);
                register_layout_dump_synthetic_item("text", "No image loaded.");
            } else {
                if (ImGui::BeginTable("##iv_info_table", 2,
                                      ImGuiTableFlags_SizingStretchProp
                                          | ImGuiTableFlags_BordersInnerV
                                          | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Field",
                                            ImGuiTableColumnFlags_WidthFixed,
                                            190.0f);
                    ImGui::TableSetupColumn("Value",
                                            ImGuiTableColumnFlags_WidthStretch);

                    auto draw_row = [](const char* label,
                                       const std::string& value) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(label);
                        ImGui::TableNextColumn();
                        ImGui::PushTextWrapPos(0.0f);
                        ImGui::TextUnformatted(value.c_str());
                        ImGui::PopTextWrapPos();
                    };

                    draw_row("Path", viewer.image.path);
                    for (const auto& row : viewer.image.longinfo_rows) {
                        draw_row(row.first.c_str(), row.second);
                    }
                    draw_row("Orientation",
                             Strutil::fmt::format("{}",
                                                  viewer.image.orientation));
                    draw_row("Subimage",
                             Strutil::fmt::format("{}/{}",
                                                  viewer.image.subimage + 1,
                                                  viewer.image.nsubimages));
                    draw_row("MIP level",
                             Strutil::fmt::format("{}/{}",
                                                  viewer.image.miplevel + 1,
                                                  viewer.image.nmiplevels));
                    draw_row("Row pitch (bytes)",
                             Strutil::fmt::format("{}",
                                                  viewer.image.row_pitch_bytes));
                    ImGui::EndTable();
                }
                register_layout_dump_synthetic_item("text", "iv Info content");
            }
            ImGui::EndChild();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
            if (ImGui::Button("Close"))
                show_window = false;
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

    void push_preview_active_button_style(bool active)
    {
        if (!active)
            return;
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(66, 112, 171, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              IM_COL32(80, 133, 200, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              IM_COL32(57, 95, 146, 255));
    }

    void pop_preview_active_button_style(bool active)
    {
        if (!active)
            return;
        ImGui::PopStyleColor(3);
    }

    void preview_form_next_row(const char* label)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::TableSetColumnIndex(1);
    }

    bool draw_preview_row_button_cell(const char* label, bool active)
    {
        ImGui::TableNextColumn();
        push_preview_active_button_style(active);
        const bool pressed
            = ImGui::Button(label,
                            ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));
        pop_preview_active_button_style(active);
        return pressed;
    }

    void preview_set_rgb_mode(PlaceholderUiState& ui)
    {
        ui.color_mode      = 1;
        ui.current_channel = 0;
    }

    void preview_set_luma_mode(PlaceholderUiState& ui)
    {
        ui.color_mode      = 3;
        ui.current_channel = 0;
    }

    void preview_set_single_channel_mode(PlaceholderUiState& ui, int channel)
    {
        ui.color_mode      = 2;
        ui.current_channel = channel;
    }

    void preview_set_heat_mode(PlaceholderUiState& ui)
    {
        ui.color_mode = 4;
        if (ui.current_channel <= 0)
            ui.current_channel = 1;
    }

    void preview_reset_adjustments(PlaceholderUiState& ui)
    {
        ui.exposure = 0.0f;
        ui.gamma    = 1.0f;
        ui.offset   = 0.0f;
    }



    void draw_preferences_window(PlaceholderUiState& ui, bool& show_window)
    {
        if (!show_window)
            return;
        set_aux_window_defaults(ImVec2(740.0f, 72.0f), ImVec2(520.0f, 360.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
        if (ImGui::Begin("iv Preferences", &show_window)) {
            const float close_height = ImGui::GetFrameHeightWithSpacing();
            const float body_height  = std::max(120.0f,
                                                ImGui::GetContentRegionAvail().y
                                                    - close_height - 4.0f);
            ImGui::BeginChild("##iv_prefs_body", ImVec2(0.0f, body_height),
                              false, ImGuiWindowFlags_NoScrollbar);

            ImGui::Checkbox("Pixel view follows mouse",
                            &ui.pixelview_follows_mouse);
            register_layout_dump_synthetic_item("text",
                                                "Pixel view follows mouse");

            ImGui::Spacing();
            ImGui::TextUnformatted("# closeup pixels");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(76.0f);
            ImGui::InputInt("##pref_closeup_pixels", &ui.closeup_pixels, 2, 2);

            ImGui::TextUnformatted("# closeup avg pixels");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(76.0f);
            ImGui::InputInt("##pref_closeup_avg_pixels", &ui.closeup_avg_pixels,
                            2, 2);

            ImGui::Spacing();
            ImGui::Checkbox("Linear interpolation", &ui.linear_interpolation);
            ImGui::Checkbox("Dark palette", &ui.dark_palette);
            ImGui::Checkbox("Generate mipmaps (requires restart)",
                            &ui.auto_mipmap);

            ImGui::Spacing();
            ImGui::TextUnformatted("Image Cache max memory (requires restart)");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90.0f);
            ImGui::InputInt("##pref_max_mem", &ui.max_memory_ic_mb);
            ImGui::SameLine();
            ImGui::TextUnformatted("MB");

            ImGui::TextUnformatted("Slide Show delay");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90.0f);
            ImGui::InputInt("##pref_slide_delay", &ui.slide_duration_seconds);
            ImGui::SameLine();
            ImGui::TextUnformatted("s");

            ImGui::EndChild();
            clamp_placeholder_ui_state(ui);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
            if (ImGui::Button("Close"))
                show_window = false;
            register_layout_dump_synthetic_item("text",
                                                "iv Preferences content");
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

    void draw_preview_window(PlaceholderUiState& ui, bool& show_window)
    {
        if (!show_window)
            return;
        set_aux_window_defaults(ImVec2(1030.0f, 72.0f), ImVec2(500.0f, 360.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
        if (ImGui::Begin("iv Preview", &show_window)) {
            const float close_height = ImGui::GetFrameHeightWithSpacing();
            const float body_height  = std::max(120.0f,
                                                ImGui::GetContentRegionAvail().y
                                                    - close_height - 4.0f);
            ImGui::BeginChild("##iv_preview_body", ImVec2(0.0f, body_height),
                              false, ImGuiWindowFlags_NoScrollbar);

            if (ImGui::BeginTable("##iv_preview_form", 2,
                                  ImGuiTableFlags_SizingStretchProp
                                      | ImGuiTableFlags_NoSavedSettings)) {
                ImGui::TableSetupColumn("Label",
                                        ImGuiTableColumnFlags_WidthFixed,
                                        90.0f);
                ImGui::TableSetupColumn("Control",
                                        ImGuiTableColumnFlags_WidthStretch);

                preview_form_next_row("Interpolation");
                ImGui::Checkbox("Linear##preview_interp",
                                &ui.linear_interpolation);

                preview_form_next_row("Exposure");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SliderFloat("##preview_exposure", &ui.exposure, -10.0f,
                                   10.0f, "%.2f");

                preview_form_next_row("");
                if (ImGui::BeginTable("##preview_exposure_steps", 4,
                                      ImGuiTableFlags_SizingStretchSame
                                          | ImGuiTableFlags_NoSavedSettings)) {
                    if (draw_preview_row_button_cell("-1/2", false))
                        ui.exposure -= 0.5f;
                    if (draw_preview_row_button_cell("-1/10", false))
                        ui.exposure -= 0.1f;
                    if (draw_preview_row_button_cell("+1/10", false))
                        ui.exposure += 0.1f;
                    if (draw_preview_row_button_cell("+1/2", false))
                        ui.exposure += 0.5f;
                    ImGui::EndTable();
                }

                preview_form_next_row("Gamma");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SliderFloat("##preview_gamma", &ui.gamma, 0.1f, 4.0f,
                                   "%.2f");

                preview_form_next_row("");
                if (ImGui::BeginTable("##preview_gamma_steps", 2,
                                      ImGuiTableFlags_SizingStretchSame
                                          | ImGuiTableFlags_NoSavedSettings)) {
                    if (draw_preview_row_button_cell("-0.1", false))
                        ui.gamma = std::max(0.1f, ui.gamma - 0.1f);
                    if (draw_preview_row_button_cell("+0.1", false))
                        ui.gamma += 0.1f;
                    ImGui::EndTable();
                }

                preview_form_next_row("Offset");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SliderFloat("##preview_offset", &ui.offset, -1.0f, 1.0f,
                                   "%+.3f");

                preview_form_next_row("");
                if (ImGui::Button("Reset",
                                  ImVec2(ImGui::GetContentRegionAvail().x,
                                         0.0f))) {
                    preview_reset_adjustments(ui);
                }

                preview_form_next_row("");
                if (ImGui::BeginTable("##preview_modes", 7,
                                      ImGuiTableFlags_SizingStretchSame
                                          | ImGuiTableFlags_NoSavedSettings)) {
                    const bool rgb_active = ui.current_channel == 0
                                            && (ui.color_mode == 0
                                                || ui.color_mode == 1);
                    const bool red_active = ui.current_channel == 1
                                            && ui.color_mode != 3
                                            && ui.color_mode != 4;
                    const bool green_active = ui.current_channel == 2
                                              && ui.color_mode != 3
                                              && ui.color_mode != 4;
                    const bool blue_active = ui.current_channel == 3
                                             && ui.color_mode != 3
                                             && ui.color_mode != 4;
                    const bool alpha_active = ui.current_channel == 4
                                              && ui.color_mode != 3
                                              && ui.color_mode != 4;
                    if (draw_preview_row_button_cell("RGB", rgb_active)) {
                        preview_set_rgb_mode(ui);
                    }
                    if (draw_preview_row_button_cell("Luma",
                                                     ui.color_mode == 3
                                                         && ui.current_channel
                                                                == 0)) {
                        preview_set_luma_mode(ui);
                    }
                    if (draw_preview_row_button_cell("R", red_active)) {
                        preview_set_single_channel_mode(ui, 1);
                    }
                    if (draw_preview_row_button_cell("G", green_active)) {
                        preview_set_single_channel_mode(ui, 2);
                    }
                    if (draw_preview_row_button_cell("B", blue_active)) {
                        preview_set_single_channel_mode(ui, 3);
                    }
                    if (draw_preview_row_button_cell("A", alpha_active)) {
                        preview_set_single_channel_mode(ui, 4);
                    }
                    if (draw_preview_row_button_cell("Heat",
                                                     ui.color_mode == 4)) {
                        preview_set_heat_mode(ui);
                    }
                    ImGui::EndTable();
                }

                ImGui::EndTable();
            }

            ImGui::EndChild();
            clamp_placeholder_ui_state(ui);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
            if (ImGui::Button("Close"))
                show_window = false;
            register_layout_dump_synthetic_item("text", "iv Preview content");
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }



    OverlayPanelRect draw_pixel_closeup_overlay(const ViewerState& viewer,
                                                PlaceholderUiState& ui_state,
                                                const ImageCoordinateMap& map,
                                                ImTextureRef closeup_texture,
                                                bool has_closeup_texture,
                                                const AppFonts& fonts)
    {
        OverlayPanelRect panel;
        if (!ui_state.show_pixelview_window || !map.valid)
            return panel;

        std::vector<std::string> lines;
        std::vector<ImU32> line_colors;
        lines.emplace_back("Pixel Closeup:");
        line_colors.emplace_back(IM_COL32(240, 242, 245, 255));
        if (viewer.image.path.empty()) {
            lines.emplace_back("No image loaded.");
            line_colors.emplace_back(IM_COL32(240, 242, 245, 255));
        } else if (!viewer.probe_valid) {
            lines.emplace_back("Hover over image to inspect.");
            line_colors.emplace_back(IM_COL32(240, 242, 245, 255));
        } else {
            lines.emplace_back(Strutil::fmt::format("          ({:d},{:d})",
                                                    viewer.probe_x,
                                                    viewer.probe_y));
            line_colors.emplace_back(IM_COL32(0, 255, 255, 220));

            std::vector<double> min_values;
            std::vector<double> max_values;
            std::vector<double> avg_values;
            int sample_count        = 0;
            const bool integer_type = probe_type_is_integer(viewer.image.type);
            const ProbeStatsSemantics preview_semantics
                = integer_type ? ProbeStatsSemantics::RawStored
                               : ProbeStatsSemantics::OIIOFloat;
            const bool have_stats = compute_area_stats(
                viewer.image, viewer.probe_x, viewer.probe_y,
                ui_state.closeup_avg_pixels, min_values, max_values, avg_values,
                sample_count, preview_semantics);

            const double denom = probe_channel_integer_denominator(
                viewer.image.type);
            if (integer_type) {
                lines.emplace_back("      Val    Norm   Min   Max   Avg");
            } else {
                lines.emplace_back("      Val    Min    Max    Avg");
            }
            line_colors.emplace_back(IM_COL32(255, 255, 160, 220));
            for (size_t c = 0; c < viewer.probe_channels.size(); ++c) {
                const std::string label
                    = pixel_preview_channel_label(viewer.image,
                                                  static_cast<int>(c));
                const double semantic_value
                    = integer_type
                          ? viewer.probe_channels[c]
                          : probe_value_to_oiio_float(viewer.image.type,
                                                      viewer.probe_channels[c]);
                const std::string value
                    = integer_type
                          ? format_probe_integer_trunc(viewer.image.type,
                                                       viewer.probe_channels[c])
                          : format_probe_iv_float(semantic_value);
                if (have_stats && c < min_values.size() && c < max_values.size()
                    && c < avg_values.size()) {
                    if (integer_type && denom > 0.0) {
                        lines.emplace_back(Strutil::fmt::format(
                            "{:<2}: {:>5}  {:>6.3f}  {:>3}  {:>3}  {:>3}",
                            label, value, viewer.probe_channels[c] / denom,
                            format_probe_integer_trunc(viewer.image.type,
                                                       min_values[c]),
                            format_probe_integer_trunc(viewer.image.type,
                                                       max_values[c]),
                            format_probe_integer_trunc(viewer.image.type,
                                                       avg_values[c])));
                    } else {
                        lines.emplace_back(Strutil::fmt::format(
                            "{:<2}: {:>6}  {:>6}  {:>6}  {:>6}", label, value,
                            format_probe_iv_float(min_values[c]),
                            format_probe_iv_float(max_values[c]),
                            format_probe_iv_float(avg_values[c])));
                    }
                } else {
                    if (integer_type) {
                        lines.emplace_back(Strutil::fmt::format(
                            "{:<2}: {:>5}  {:>6}  {:>3}  {:>3}  {:>3}", label,
                            value, "-----", "---", "---", "---"));
                    } else {
                        lines.emplace_back(Strutil::fmt::format(
                            "{:<2}: {:>6}  {:>6}  {:>6}  {:>6}", label, value,
                            "-----", "-----", "-----"));
                    }
                }
                ImU32 channel_color = IM_COL32(220, 220, 220, 255);
                if (!label.empty()) {
                    if (label[0] == 'R')
                        channel_color = IM_COL32(250, 94, 143, 255);
                    else if (label[0] == 'G')
                        channel_color = IM_COL32(135, 203, 124, 255);
                    else if (label[0] == 'B')
                        channel_color = IM_COL32(107, 188, 255, 255);
                }
                line_colors.emplace_back(channel_color);
            }
            (void)sample_count;
        }

        const float closeup_window_size = 260.0f;
        const float follow_mouse_offset = 15.0f;
        const float corner_padding      = 5.0f;
        const float text_pad_x          = 10.0f;
        const float text_pad_y          = 8.0f;
        const float text_line_gap       = 2.0f;
        const float text_to_window_gap  = 2.0f;
        const float text_wrap_w         = std::max(8.0f, closeup_window_size
                                                             - text_pad_x * 2.0f);
        ImFont* text_font          = fonts.mono ? fonts.mono : ImGui::GetFont();
        const float text_font_size = text_font ? text_font->LegacySize
                                               : ImGui::GetFontSize();

        float text_panel_h = text_pad_y * 2.0f;
        for (const std::string& line : lines) {
            const ImVec2 line_size
                = text_font->CalcTextSizeA(text_font_size,
                                           std::numeric_limits<float>::max(),
                                           text_wrap_w, line.c_str());
            text_panel_h += line_size.y;
            text_panel_h += text_line_gap;
        }
        if (!lines.empty())
            text_panel_h -= text_line_gap;
        const float text_panel_w = closeup_window_size;
        const float total_h      = closeup_window_size + text_to_window_gap
                              + text_panel_h;

        const float clip_min_x = std::min(map.viewport_rect_min.x,
                                          map.viewport_rect_max.x);
        const float clip_min_y = std::min(map.viewport_rect_min.y,
                                          map.viewport_rect_max.y);
        const float clip_max_x = std::max(map.viewport_rect_min.x,
                                          map.viewport_rect_max.x);
        const float clip_max_y = std::max(map.viewport_rect_min.y,
                                          map.viewport_rect_max.y);
        if ((clip_max_x - clip_min_x) < closeup_window_size
            || (clip_max_y - clip_min_y) < total_h) {
            return panel;
        }

        ImVec2 closeup_min(clip_min_x + corner_padding,
                           clip_min_y + corner_padding);
        const ImVec2 mouse_pos = ImGui::GetIO().MousePos;

        if (ui_state.pixelview_follows_mouse) {
            const bool should_show_on_left = (mouse_pos.x + closeup_window_size
                                              + follow_mouse_offset)
                                             > clip_max_x;
            const bool should_show_above = (mouse_pos.y + closeup_window_size
                                            + follow_mouse_offset
                                            + text_panel_h)
                                           > clip_max_y;

            closeup_min.x = mouse_pos.x + follow_mouse_offset;
            closeup_min.y = mouse_pos.y + follow_mouse_offset;
            if (should_show_on_left) {
                closeup_min.x = mouse_pos.x - follow_mouse_offset
                                - closeup_window_size;
            }
            if (should_show_above) {
                closeup_min.y = mouse_pos.y - follow_mouse_offset
                                - closeup_window_size - text_to_window_gap
                                - text_panel_h;
            }
        } else {
            closeup_min.x = ui_state.pixelview_left_corner
                                ? (clip_min_x + corner_padding)
                                : (clip_max_x - closeup_window_size
                                   - corner_padding);
            closeup_min.y = clip_min_y + corner_padding;

            const ImVec2 panel_max(closeup_min.x + text_panel_w,
                                   closeup_min.y + total_h);
            const bool mouse_over_panel = mouse_pos.x >= closeup_min.x
                                          && mouse_pos.x <= panel_max.x
                                          && mouse_pos.y >= closeup_min.y
                                          && mouse_pos.y <= panel_max.y;
            if (mouse_over_panel) {
                ui_state.pixelview_left_corner = !ui_state.pixelview_left_corner;
                closeup_min.x = ui_state.pixelview_left_corner
                                    ? (clip_min_x + corner_padding)
                                    : (clip_max_x - closeup_window_size
                                       - corner_padding);
            }
        }

        closeup_min.x
            = std::clamp(closeup_min.x, clip_min_x,
                         std::max(clip_min_x, clip_max_x - text_panel_w));
        closeup_min.y = std::clamp(closeup_min.y, clip_min_y,
                                   std::max(clip_min_y, clip_max_y - total_h));
        const ImVec2 closeup_max(closeup_min.x + closeup_window_size,
                                 closeup_min.y + closeup_window_size);
        const ImVec2 text_min(closeup_min.x,
                              closeup_max.y + text_to_window_gap);
        const ImVec2 text_max(text_min.x + text_panel_w,
                              text_min.y + text_panel_h);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->PushClipRect(map.viewport_rect_min, map.viewport_rect_max,
                                true);
        draw_list->AddRectFilled(closeup_min, closeup_max,
                                 IM_COL32(20, 24, 30, 224), 4.0f);
        draw_list->AddRect(closeup_min, closeup_max,
                           IM_COL32(175, 185, 205, 255), 4.0f, 0, 1.0f);

        const bool render_zoom_patch = has_closeup_texture
                                       && !viewer.image.path.empty()
                                       && viewer.probe_valid
                                       && map.source_width > 0
                                       && map.source_height > 0;
        if (render_zoom_patch) {
            int display_w = viewer.image.width;
            int display_h = viewer.image.height;
            oriented_image_dimensions(viewer.image, display_w, display_h);
            display_w = std::max(1, display_w);
            display_h = std::max(1, display_h);

            int closeup_px = std::clamp(ui_state.closeup_pixels, 1,
                                        std::min(display_w, display_h));
            if (closeup_px <= 0)
                closeup_px = 1;
            int patch_w = std::min(closeup_px, display_w);
            int patch_h = std::min(closeup_px, display_h);
            patch_w     = std::max(1, patch_w);
            patch_h     = std::max(1, patch_h);

            const ImVec2 source_uv((static_cast<float>(viewer.probe_x) + 0.5f)
                                       / static_cast<float>(map.source_width),
                                   (static_cast<float>(viewer.probe_y) + 0.5f)
                                       / static_cast<float>(map.source_height));
            const ImVec2 display_uv = source_uv_to_display_uv(source_uv,
                                                              map.orientation);
            const int center_x      = std::clamp(static_cast<int>(std::floor(
                                                display_uv.x * display_w)),
                                                 0, display_w - 1);
            const int center_y      = std::clamp(static_cast<int>(std::floor(
                                                display_uv.y * display_h)),
                                                 0, display_h - 1);

            const int xbegin = std::clamp(center_x - patch_w / 2, 0,
                                          std::max(0, display_w - patch_w));
            const int ybegin = std::clamp(center_y - patch_h / 2, 0,
                                          std::max(0, display_h - patch_h));
            const int xend   = xbegin + patch_w;
            const int yend   = ybegin + patch_h;

            const ImVec2 uv_min(static_cast<float>(xbegin) / display_w,
                                static_cast<float>(ybegin) / display_h);
            const ImVec2 uv_max(static_cast<float>(xend) / display_w,
                                static_cast<float>(yend) / display_h);
            draw_list->AddImage(closeup_texture, closeup_min, closeup_max,
                                uv_min, uv_max, IM_COL32_WHITE);

            const float cell_w = closeup_window_size / patch_w;
            const float cell_h = closeup_window_size / patch_h;
            for (int i = 1; i < patch_w; ++i) {
                const float x = closeup_min.x + i * cell_w;
                draw_list->AddLine(ImVec2(x, closeup_min.y),
                                   ImVec2(x, closeup_max.y),
                                   IM_COL32(8, 10, 12, 140), 1.0f);
            }
            for (int i = 1; i < patch_h; ++i) {
                const float y = closeup_min.y + i * cell_h;
                draw_list->AddLine(ImVec2(closeup_min.x, y),
                                   ImVec2(closeup_max.x, y),
                                   IM_COL32(8, 10, 12, 140), 1.0f);
            }

            auto draw_corner_marker =
                [draw_list](const ImVec2& p0, const ImVec2& p1, ImU32 color) {
                    const float corner_size = 4.0f;
                    draw_list->AddLine(p0, ImVec2(p0.x + corner_size, p0.y),
                                       color, 1.0f);
                    draw_list->AddLine(p0, ImVec2(p0.x, p0.y + corner_size),
                                       color, 1.0f);
                    draw_list->AddLine(ImVec2(p1.x - corner_size, p0.y),
                                       ImVec2(p1.x, p0.y), color, 1.0f);
                    draw_list->AddLine(ImVec2(p1.x, p0.y),
                                       ImVec2(p1.x, p0.y + corner_size), color,
                                       1.0f);
                    draw_list->AddLine(ImVec2(p0.x, p1.y - corner_size),
                                       ImVec2(p0.x, p1.y), color, 1.0f);
                    draw_list->AddLine(ImVec2(p0.x, p1.y),
                                       ImVec2(p0.x + corner_size, p1.y), color,
                                       1.0f);
                    draw_list->AddLine(ImVec2(p1.x - corner_size, p1.y), p1,
                                       color, 1.0f);
                    draw_list->AddLine(ImVec2(p1.x, p1.y - corner_size), p1,
                                       color, 1.0f);
                };

            const int center_ix = center_x - xbegin;
            const int center_iy = center_y - ybegin;
            const ImVec2 center_min(closeup_min.x + center_ix * cell_w,
                                    closeup_min.y + center_iy * cell_h);
            const ImVec2 center_max(center_min.x + cell_w,
                                    center_min.y + cell_h);
            draw_corner_marker(center_min, center_max,
                               IM_COL32(0, 255, 255, 180));

            int avg_px = std::clamp(ui_state.closeup_avg_pixels, 1,
                                    std::min(patch_w, patch_h));
            if ((avg_px & 1) == 0)
                avg_px = std::max(1, avg_px - 1);
            if (avg_px > 1) {
                int avg_start_x = center_ix - avg_px / 2;
                int avg_start_y = center_iy - avg_px / 2;
                int avg_end_x   = avg_start_x + avg_px;
                int avg_end_y   = avg_start_y + avg_px;
                avg_start_x     = std::clamp(avg_start_x, 0, patch_w - avg_px);
                avg_start_y     = std::clamp(avg_start_y, 0, patch_h - avg_px);
                avg_end_x       = avg_start_x + avg_px;
                avg_end_y       = avg_start_y + avg_px;
                const ImVec2 avg_min(closeup_min.x + avg_start_x * cell_w,
                                     closeup_min.y + avg_start_y * cell_h);
                const ImVec2 avg_max(closeup_min.x + avg_end_x * cell_w,
                                     closeup_min.y + avg_end_y * cell_h);
                draw_corner_marker(avg_min, avg_max,
                                   IM_COL32(255, 255, 0, 170));
            }
        }

        draw_list->AddRectFilled(text_min, text_max, IM_COL32(20, 24, 30, 224),
                                 4.0f);
        draw_list->AddRect(text_min, text_max, IM_COL32(175, 185, 205, 255),
                           4.0f, 0, 1.0f);
        ImVec2 text_pos(text_min.x + text_pad_x, text_min.y + text_pad_y);
        for (size_t i = 0; i < lines.size(); ++i) {
            const std::string& line = lines[i];
            const ImU32 color       = i < line_colors.size()
                                          ? line_colors[i]
                                          : IM_COL32(240, 242, 245, 255);
            draw_list->AddText(text_font, text_font_size, text_pos, color,
                               line.c_str(), nullptr, text_wrap_w);
            const ImVec2 line_size
                = text_font->CalcTextSizeA(text_font_size,
                                           std::numeric_limits<float>::max(),
                                           text_wrap_w, line.c_str());
            text_pos.y += line_size.y + text_line_gap;
        }
        draw_list->PopClipRect();

        panel.valid = true;
        panel.min   = closeup_min;
        panel.max   = ImVec2(closeup_min.x + text_panel_w,
                             closeup_min.y + total_h);
        register_layout_dump_synthetic_item("text", "Pixel Closeup overlay");
        return panel;
    }



    void draw_area_probe_overlay(const ViewerState& viewer,
                                 const PlaceholderUiState& ui_state,
                                 const ImageCoordinateMap& map,
                                 const OverlayPanelRect& pixel_overlay_panel,
                                 const AppFonts& fonts)
    {
        (void)pixel_overlay_panel;
        if (!ui_state.show_area_probe_window || !map.valid)
            return;

        std::vector<std::string> lines;
        lines.emplace_back("Area Probe:");
        if (viewer.image.path.empty()) {
            lines.emplace_back("No image loaded.");
        } else {
            std::vector<double> min_values;
            std::vector<double> max_values;
            std::vector<double> avg_values;
            int sample_count = 0;
            const bool have_stats
                = viewer.probe_valid
                  && compute_area_stats(viewer.image, viewer.probe_x,
                                        viewer.probe_y,
                                        ui_state.closeup_avg_pixels, min_values,
                                        max_values, avg_values, sample_count,
                                        ProbeStatsSemantics::OIIOFloat);

            const int channel_count = std::max(1, viewer.image.nchannels);
            for (int c = 0; c < channel_count; ++c) {
                const std::string channel
                    = pixel_preview_channel_label(viewer.image,
                                                  static_cast<int>(c));
                if (have_stats && static_cast<size_t>(c) < min_values.size()
                    && static_cast<size_t>(c) < max_values.size()
                    && static_cast<size_t>(c) < avg_values.size()) {
                    lines.emplace_back(Strutil::fmt::format(
                        "{:<5}: [min: {:>6.3f}  max: {:>6.3f}  avg: {:>6.3f}]",
                        channel, min_values[c], max_values[c], avg_values[c]));
                } else {
                    lines.emplace_back(Strutil::fmt::format(
                        "{:<5}: [min:  -----  max:  -----  avg:  -----]",
                        channel));
                }
            }
        }

        const float clip_min_x  = std::min(map.viewport_rect_min.x,
                                           map.viewport_rect_max.x);
        const float clip_max_y  = std::max(map.viewport_rect_min.y,
                                           map.viewport_rect_max.y);
        const float pad_y       = 8.0f;
        const float line_gap    = 2.0f;
        const ImFont* mono_font = fonts.mono ? fonts.mono : ImGui::GetFont();
        const float line_h      = mono_font ? mono_font->LegacySize
                                            : ImGui::GetTextLineHeight();
        float panel_h = pad_y * 2.0f + static_cast<float>(lines.size()) * line_h
                        + static_cast<float>(
                              std::max<size_t>(0, lines.size() - 1))
                              * line_gap;
        const float border_margin = 9.0f;
        ImVec2 preferred(clip_min_x + border_margin,
                         clip_max_y - panel_h - border_margin);
        draw_overlay_text_panel(lines, preferred, map.viewport_rect_min,
                                map.viewport_rect_max, fonts.mono);
        register_layout_dump_synthetic_item("text", "Area Probe overlay");
    }

    bool primary_monitor_workarea(int& x, int& y, int& w, int& h)
    {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (monitor == nullptr)
            return false;
#if defined(GLFW_VERSION_MAJOR)  \
    && ((GLFW_VERSION_MAJOR > 3) \
        || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 3))
        glfwGetMonitorWorkarea(monitor, &x, &y, &w, &h);
        if (w > 0 && h > 0)
            return true;
#endif
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (mode == nullptr)
            return false;
        x = 0;
        y = 0;
        w = mode->width;
        h = mode->height;
        return (w > 0 && h > 0);
    }

    bool centered_glfw_window_pos(GLFWwindow* window, int& out_pos_x,
                                  int& out_pos_y, int& out_window_w,
                                  int& out_window_h)
    {
        out_pos_x    = 0;
        out_pos_y    = 0;
        out_window_w = 0;
        out_window_h = 0;
        if (window == nullptr)
            return false;
        int monitor_x = 0;
        int monitor_y = 0;
        int monitor_w = 0;
        int monitor_h = 0;
        if (!primary_monitor_workarea(monitor_x, monitor_y, monitor_w,
                                      monitor_h)) {
            return false;
        }
        int window_w = 0;
        int window_h = 0;
        glfwGetWindowSize(window, &window_w, &window_h);
        if (window_w <= 0 || window_h <= 0)
            return false;
        int frame_left   = 0;
        int frame_top    = 0;
        int frame_right  = 0;
        int frame_bottom = 0;
        glfwGetWindowFrameSize(window, &frame_left, &frame_top, &frame_right,
                               &frame_bottom);
        const int outer_w = window_w + frame_left + frame_right;
        const int outer_h = window_h + frame_top + frame_bottom;
        out_pos_x         = monitor_x + std::max(0, (monitor_w - outer_w) / 2)
                    + frame_left;
        out_pos_y = monitor_y + std::max(0, (monitor_h - outer_h) / 2)
                    + frame_top;
        out_window_w = window_w;
        out_window_h = window_h;
        return true;
    }

    void center_glfw_window(GLFWwindow* window)
    {
        int pos_x    = 0;
        int pos_y    = 0;
        int window_w = 0;
        int window_h = 0;
        if (!centered_glfw_window_pos(window, pos_x, pos_y, window_w,
                                      window_h)) {
            return;
        }
        glfwSetWindowPos(window, pos_x, pos_y);
    }

    void force_center_glfw_window(GLFWwindow* window)
    {
        int pos_x    = 0;
        int pos_y    = 0;
        int window_w = 0;
        int window_h = 0;
        if (!centered_glfw_window_pos(window, pos_x, pos_y, window_w,
                                      window_h)) {
            return;
        }
        glfwSetWindowMonitor(window, nullptr, pos_x, pos_y, window_w, window_h,
                             GLFW_DONT_CARE);
        glfwSetWindowPos(window, pos_x, pos_y);
    }

    void current_child_visible_rect(const ImVec2& padding, bool scroll_x,
                                    bool scroll_y, ImVec2& out_min,
                                    ImVec2& out_max)
    {
        const ImVec2 window_pos  = ImGui::GetWindowPos();
        const ImVec2 window_size = ImGui::GetWindowSize();
        const ImGuiStyle& style  = ImGui::GetStyle();
        out_min = ImVec2(window_pos.x + padding.x, window_pos.y + padding.y);
        out_max = ImVec2(window_pos.x + window_size.x - padding.x,
                         window_pos.y + window_size.y - padding.y);
        if (scroll_y)
            out_max.x -= style.ScrollbarSize;
        if (scroll_x)
            out_max.y -= style.ScrollbarSize;
        out_max.x = std::max(out_min.x, out_max.x);
        out_max.y = std::max(out_min.y, out_max.y);
    }



    std::string status_image_text(const ViewerState& viewer)
    {
        if (viewer.image.path.empty())
            return "No image loaded";
        int current = 1;
        int total   = 1;
        if (!viewer.sibling_images.empty() && viewer.sibling_index >= 0) {
            total   = static_cast<int>(viewer.sibling_images.size());
            current = viewer.sibling_index + 1;
        }
        return Strutil::fmt::format("({}/{}): {} ({}x{}, {} ch, {})", current,
                                    total, viewer.image.path,
                                    viewer.image.width, viewer.image.height,
                                    viewer.image.nchannels,
                                    upload_data_type_name(viewer.image.type));
    }



    std::string status_view_text(const ViewerState& viewer,
                                 const PlaceholderUiState& ui)
    {
        if (viewer.image.path.empty())
            return "";

        std::string mode = color_mode_name(ui.color_mode);
        if (ui.color_mode == 2 || ui.color_mode == 4) {
            mode += Strutil::fmt::format(" {}", ui.current_channel);
        } else {
            mode += Strutil::fmt::format(" ({})",
                                         channel_view_name(ui.current_channel));
        }

        const float zoom  = std::max(viewer.zoom, 0.00001f);
        const float z_num = zoom >= 1.0f ? zoom : 1.0f;
        const float z_den = zoom >= 1.0f ? 1.0f : (1.0f / zoom);
        std::string text  = Strutil::fmt::format(
            "{}  {:.2f}:{:.2f}  exp {:+.1f}  gam {:.2f}  off {:+.2f}", mode,
            z_num, z_den, ui.exposure, ui.gamma, ui.offset);
        if (viewer.image.nsubimages > 1) {
            text += Strutil::fmt::format("  subimg {}/{}",
                                         viewer.image.subimage + 1,
                                         viewer.image.nsubimages);
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



    bool app_shortcut(ImGuiKeyChord key_chord)
    {
        return ImGui::Shortcut(key_chord,
                               ImGuiInputFlags_RouteGlobal
                                   | ImGuiInputFlags_RouteUnlessBgFocused);
    }



    void draw_embedded_status_bar(const ViewerState& viewer,
                                  PlaceholderUiState& ui)
    {
        const std::string img_text  = status_image_text(viewer);
        const std::string view_text = status_view_text(viewer, ui);
        const bool show_progress    = false;

        int columns = 2;
        if (show_progress)
            ++columns;
        if (ui.show_mouse_mode_selector)
            ++columns;
        ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV
                                      | ImGuiTableFlags_PadOuterX
                                      | ImGuiTableFlags_SizingStretchProp
                                      | ImGuiTableFlags_NoSavedSettings;
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 4.0f));
        if (ImGui::BeginTable("##imiv_status_bar", columns, table_flags)) {
            ImGui::TableSetupColumn("Image", ImGuiTableColumnFlags_WidthStretch,
                                    2.5f);
            ImGui::TableSetupColumn("View", ImGuiTableColumnFlags_WidthStretch,
                                    2.0f);
            if (show_progress) {
                ImGui::TableSetupColumn("Load",
                                        ImGuiTableColumnFlags_WidthFixed,
                                        140.0f);
            }
            if (ui.show_mouse_mode_selector) {
                ImGui::TableSetupColumn("Mouse",
                                        ImGuiTableColumnFlags_WidthFixed,
                                        150.0f);
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(img_text.c_str());
            register_layout_dump_synthetic_item("text", img_text.c_str());

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(view_text.c_str());
            register_layout_dump_synthetic_item("text", view_text.c_str());

            if (show_progress) {
                ImGui::TableNextColumn();
                ImGui::ProgressBar(0.0f, ImVec2(-1.0f, 0.0f), "idle");
            }

            if (ui.show_mouse_mode_selector) {
                static const char* mouse_modes[] = { "Zoom", "Pan", "Wipe",
                                                     "Select", "Annotate" };
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::Combo("##mouse_mode", &ui.mouse_mode, mouse_modes,
                             IM_ARRAYSIZE(mouse_modes));
            }

            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }



    void draw_viewer_ui(ViewerState& viewer, PlaceholderUiState& ui_state,
                        const AppFonts& fonts, bool& request_exit
#if defined(IMGUI_ENABLE_TEST_ENGINE)
                        ,
                        bool* show_test_engine_windows
#endif
#if defined(IMIV_BACKEND_VULKAN_GLFW)
                        ,
                        GLFWwindow* window, VulkanState& vk_state
#endif
    )
    {
        reset_layout_dump_synthetic_items();
        reset_test_engine_mouse_space();

        bool open_requested                = false;
        bool save_as_requested             = false;
        bool clear_recent_requested        = false;
        bool reload_requested              = false;
        bool close_requested               = false;
        bool prev_requested                = false;
        bool next_requested                = false;
        bool toggle_requested              = false;
        bool prev_subimage_requested       = false;
        bool next_subimage_requested       = false;
        bool prev_mip_requested            = false;
        bool next_mip_requested            = false;
        bool save_window_as_requested      = false;
        bool save_selection_as_requested   = false;
        bool fit_window_to_image_requested = false;
        bool recenter_requested            = false;
        bool delete_from_disk_requested    = false;
        bool full_screen_toggle_requested  = false;
        bool rotate_left_requested         = false;
        bool rotate_right_requested        = false;
        bool flip_horizontal_requested     = false;
        bool flip_vertical_requested       = false;
        PendingZoomRequest pending_zoom;
        std::string recent_open_path;
        const bool has_image         = !viewer.image.path.empty();
        const bool can_prev_subimage = has_image && viewer.image.subimage > 0;
        const bool can_next_subimage = has_image
                                       && (viewer.image.subimage + 1
                                           < viewer.image.nsubimages);
        const bool can_prev_mip = has_image && viewer.image.miplevel > 0;
        const bool can_next_mip = has_image
                                  && (viewer.image.miplevel + 1
                                      < viewer.image.nmiplevels);

#if defined(IMIV_BACKEND_VULKAN_GLFW)
        if (window != nullptr) {
            std::string fullscreen_error;
            set_full_screen_mode(window, viewer, ui_state.full_screen_mode,
                                 fullscreen_error);
            if (!fullscreen_error.empty()) {
                viewer.last_error         = fullscreen_error;
                ui_state.full_screen_mode = viewer.fullscreen_applied;
            }
        }
#endif

        const ImGuiIO& global_io = ImGui::GetIO();
        const bool no_mods       = !global_io.KeyCtrl && !global_io.KeyAlt
                             && !global_io.KeySuper;

        if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_INFO"))
            ui_state.show_info_window = true;
        if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_PREFS"))
            ui_state.show_preferences_window = true;
        if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_PREVIEW"))
            ui_state.show_preview_window = true;
        if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_PIXEL"))
            ui_state.show_pixelview_window = true;
        if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_AREA"))
            ui_state.show_area_probe_window = true;
        if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_AUX_WINDOWS")) {
            ui_state.show_info_window        = true;
            ui_state.show_preferences_window = true;
            ui_state.show_preview_window     = true;
            ui_state.show_pixelview_window   = true;
            ui_state.show_area_probe_window  = true;
        }

        if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_O))
            open_requested = true;
        if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_R) && has_image)
            reload_requested = true;
        if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_W) && has_image)
            close_requested = true;
        if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_S) && has_image)
            save_as_requested = true;
        if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_Comma))
            ui_state.show_preferences_window = true;
        if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_Q))
            request_exit = true;
        if (app_shortcut(ImGuiKey_PageUp))
            prev_requested = true;
        if (app_shortcut(ImGuiKey_PageDown))
            next_requested = true;
        if (no_mods && app_shortcut(ImGuiKey_T))
            toggle_requested = true;
        if ((app_shortcut(ImGuiMod_Ctrl | ImGuiKey_Equal)
             || app_shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Equal)
             || app_shortcut(ImGuiMod_Ctrl | ImGuiKey_KeypadAdd))
            && has_image)
            request_zoom_scale(pending_zoom, 2.0f, false);
        if ((app_shortcut(ImGuiMod_Ctrl | ImGuiKey_Minus)
             || app_shortcut(ImGuiMod_Ctrl | ImGuiKey_KeypadSubtract))
            && has_image)
            request_zoom_scale(pending_zoom, 0.5f, false);
        if ((app_shortcut(ImGuiMod_Ctrl | ImGuiKey_0)
             || app_shortcut(ImGuiMod_Ctrl | ImGuiKey_Keypad0))
            && has_image)
            request_zoom_reset(pending_zoom, false);
        if ((app_shortcut(ImGuiMod_Ctrl | ImGuiKey_Period)
             || app_shortcut(ImGuiMod_Ctrl | ImGuiKey_KeypadDecimal))
            && has_image)
            recenter_requested = true;
        if (no_mods && app_shortcut(ImGuiKey_F) && has_image)
            fit_window_to_image_requested = true;
        if (app_shortcut(ImGuiMod_Alt | ImGuiKey_F) && has_image) {
            ui_state.fit_image_to_window = !ui_state.fit_image_to_window;
            viewer.fit_request           = true;
        }
        if (app_shortcut(ImGuiMod_Ctrl | ImGuiKey_F))
            full_screen_toggle_requested = true;
        if (ui_state.full_screen_mode && app_shortcut(ImGuiKey_Escape))
            full_screen_toggle_requested = true;
        if (app_shortcut(ImGuiMod_Shift | ImGuiKey_Comma) && can_prev_subimage)
            prev_subimage_requested = true;
        if (app_shortcut(ImGuiMod_Shift | ImGuiKey_Period) && can_next_subimage)
            next_subimage_requested = true;
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
            ui_state.current_channel = std::max(0,
                                                ui_state.current_channel - 1);
        if (no_mods && app_shortcut(ImGuiKey_Period) && has_image)
            ui_state.current_channel = std::min(4,
                                                ui_state.current_channel + 1);
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
            ui_state.show_area_probe_window = !ui_state.show_area_probe_window;
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
            delete_from_disk_requested = true;
        if (app_shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_L))
            rotate_left_requested = true;
        if (app_shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_R))
            rotate_right_requested = true;

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open...", "Ctrl+O"))
                    open_requested = true;

                if (ImGui::BeginMenu("Open recent...")) {
                    if (viewer.recent_images.empty()) {
                        ImGui::MenuItem("No recent files", nullptr, false,
                                        false);
                    } else {
                        for (size_t i = 0; i < viewer.recent_images.size();
                             ++i) {
                            const std::string& recent = viewer.recent_images[i];
                            const std::string label
                                = Strutil::fmt::format("{}: {}##imiv_recent_{}",
                                                       i + 1, recent, i);
                            if (ImGui::MenuItem(label.c_str()))
                                recent_open_path = recent;
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Clear recent list", nullptr, false,
                                        !viewer.recent_images.empty()))
                        clear_recent_requested = true;
                    ImGui::EndMenu();
                }

                if (ImGui::MenuItem("Reload image", "Ctrl+R", false, has_image))
                    reload_requested = true;
                if (ImGui::MenuItem("Close image", "Ctrl+W", false, has_image))
                    close_requested = true;
                ImGui::Separator();
                if (ImGui::MenuItem("Save As...", "Ctrl+S", false, has_image))
                    save_as_requested = true;
                if (ImGui::MenuItem("Save Window As...", nullptr, false,
                                    has_image))
                    save_window_as_requested = true;
                if (ImGui::MenuItem("Save Selection As...", nullptr, false,
                                    has_image))
                    save_selection_as_requested = true;
                ImGui::Separator();
                if (ImGui::MenuItem("Move to new window", nullptr, false,
                                    has_image))
                    viewer.status_message
                        = "Move to new window is not available in imiv yet";
                if (ImGui::MenuItem("Delete from disk", "Delete", false,
                                    has_image))
                    delete_from_disk_requested = true;
                ImGui::Separator();
                if (ImGui::MenuItem("Preferences...", "Ctrl+,"))
                    ui_state.show_preferences_window = true;
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Ctrl+Q"))
                    request_exit = true;
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Previous Image", "PgUp"))
                    prev_requested = true;
                if (ImGui::MenuItem("Next Image", "PgDown"))
                    next_requested = true;
                if (ImGui::MenuItem("Toggle image", "T"))
                    toggle_requested = true;
                ImGui::MenuItem("Show display/data window borders", nullptr,
                                &ui_state.show_window_guides);
                ImGui::Separator();

                if (ImGui::MenuItem("Zoom In", "Ctrl++", false, has_image))
                    request_zoom_scale(pending_zoom, 2.0f, false);
                if (ImGui::MenuItem("Zoom Out", "Ctrl+-", false, has_image))
                    request_zoom_scale(pending_zoom, 0.5f, false);
                if (ImGui::MenuItem("Normal Size (1:1)", "Ctrl+0", false,
                                    has_image))
                    request_zoom_reset(pending_zoom, false);
                if (ImGui::MenuItem("Re-center Image", "Ctrl+.", false,
                                    has_image))
                    recenter_requested = true;
                if (ImGui::MenuItem("Fit Window to Image", "F", false,
                                    has_image))
                    fit_window_to_image_requested = true;
                if (ImGui::MenuItem("Fit Image to Window", "Alt+F",
                                    ui_state.fit_image_to_window, has_image)) {
                    ui_state.fit_image_to_window = !ui_state.fit_image_to_window;
                    viewer.fit_request = true;
                }
                if (ImGui::MenuItem("Full screen", "Ctrl+F",
                                    ui_state.full_screen_mode)) {
                    full_screen_toggle_requested = true;
                }
                ImGui::Separator();

                if (ImGui::MenuItem("Prev Subimage", "<", false,
                                    can_prev_subimage))
                    prev_subimage_requested = true;
                if (ImGui::MenuItem("Next Subimage", ">", false,
                                    can_next_subimage))
                    next_subimage_requested = true;
                if (ImGui::MenuItem("Prev MIP level", nullptr, false,
                                    can_prev_mip))
                    prev_mip_requested = true;
                if (ImGui::MenuItem("Next MIP level", nullptr, false,
                                    can_next_mip))
                    next_mip_requested = true;

                if (ImGui::BeginMenu("Channels")) {
                    if (ImGui::MenuItem("Full Color", "C",
                                        ui_state.current_channel == 0))
                        ui_state.current_channel = 0;
                    if (ImGui::MenuItem("Red", "R",
                                        ui_state.current_channel == 1))
                        ui_state.current_channel = 1;
                    if (ImGui::MenuItem("Green", "G",
                                        ui_state.current_channel == 2))
                        ui_state.current_channel = 2;
                    if (ImGui::MenuItem("Blue", "B",
                                        ui_state.current_channel == 3))
                        ui_state.current_channel = 3;
                    if (ImGui::MenuItem("Alpha", "A",
                                        ui_state.current_channel == 4))
                        ui_state.current_channel = 4;
                    ImGui::Separator();
                    if (ImGui::MenuItem("Prev Channel", ",", false, has_image)) {
                        ui_state.current_channel
                            = std::max(0, ui_state.current_channel - 1);
                    }
                    if (ImGui::MenuItem("Next Channel", ".", false, has_image)) {
                        ui_state.current_channel
                            = std::min(4, ui_state.current_channel + 1);
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Color mode")) {
                    if (ImGui::MenuItem("RGBA", nullptr,
                                        ui_state.color_mode == 0))
                        ui_state.color_mode = 0;
                    if (ImGui::MenuItem("RGB", nullptr,
                                        ui_state.color_mode == 1))
                        ui_state.color_mode = 1;
                    if (ImGui::MenuItem("Single channel", "1",
                                        ui_state.color_mode == 2))
                        ui_state.color_mode = 2;
                    if (ImGui::MenuItem("Luminance", "L",
                                        ui_state.color_mode == 3))
                        ui_state.color_mode = 3;
                    if (ImGui::MenuItem("Single channel (Heatmap)", "H",
                                        ui_state.color_mode == 4))
                        ui_state.color_mode = 4;
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("OCIO")) {
                    ImGui::MenuItem("Use OCIO", nullptr, &ui_state.use_ocio);
                    if (ImGui::BeginMenu("Image color space")) {
                        if (ImGui::MenuItem("auto", nullptr,
                                            ui_state.ocio_image_color_space
                                                == "auto"))
                            ui_state.ocio_image_color_space = "auto";
                        if (ImGui::MenuItem("scene_linear", nullptr,
                                            ui_state.ocio_image_color_space
                                                == "scene_linear"))
                            ui_state.ocio_image_color_space = "scene_linear";
                        if (ImGui::MenuItem("sRGB", nullptr,
                                            ui_state.ocio_image_color_space
                                                == "sRGB"))
                            ui_state.ocio_image_color_space = "sRGB";
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Display/View")) {
                        if (ImGui::MenuItem("default / default", nullptr,
                                            ui_state.ocio_display == "default"
                                                && ui_state.ocio_view
                                                       == "default")) {
                            ui_state.ocio_display = "default";
                            ui_state.ocio_view    = "default";
                        }
                        if (ImGui::MenuItem("sRGB / Film", nullptr,
                                            ui_state.ocio_display == "sRGB"
                                                && ui_state.ocio_view
                                                       == "Film")) {
                            ui_state.ocio_display = "sRGB";
                            ui_state.ocio_view    = "Film";
                        }
                        if (ImGui::MenuItem("sRGB / Raw", nullptr,
                                            ui_state.ocio_display == "sRGB"
                                                && ui_state.ocio_view
                                                       == "Raw")) {
                            ui_state.ocio_display = "sRGB";
                            ui_state.ocio_view    = "Raw";
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
                ImGui::MenuItem("Image info...", "Ctrl+I",
                                &ui_state.show_info_window);
                ImGui::MenuItem("Preview controls...", nullptr,
                                &ui_state.show_preview_window);
                ImGui::MenuItem("Pixel closeup view...", "P",
                                &ui_state.show_pixelview_window);
                ImGui::MenuItem("Toggle area sample", "Ctrl+A",
                                &ui_state.show_area_probe_window);

                if (ImGui::BeginMenu("Slide Show")) {
                    if (ImGui::MenuItem("Start Slide Show", nullptr,
                                        ui_state.slide_show_running)) {
                        toggle_slide_show_action(ui_state, viewer);
                    }
                    if (ImGui::MenuItem("Loop slide show", nullptr,
                                        ui_state.slide_loop))
                        ui_state.slide_loop = !ui_state.slide_loop;
                    if (ImGui::MenuItem("Stop at end", nullptr,
                                        !ui_state.slide_loop))
                        ui_state.slide_loop = false;
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Sort")) {
                    if (ImGui::MenuItem("By Name"))
                        set_sort_mode_action(viewer, ImageSortMode::ByName);
                    if (ImGui::MenuItem("By File Path"))
                        set_sort_mode_action(viewer, ImageSortMode::ByPath);
                    if (ImGui::MenuItem("By Image Date"))
                        set_sort_mode_action(viewer,
                                             ImageSortMode::ByImageDate);
                    if (ImGui::MenuItem("By File Date"))
                        set_sort_mode_action(viewer, ImageSortMode::ByFileDate);
                    if (ImGui::MenuItem("Reverse current order"))
                        toggle_sort_reverse_action(viewer);
                    ImGui::EndMenu();
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Rotate Left", "Ctrl+Shift+L"))
                    rotate_left_requested = true;
                if (ImGui::MenuItem("Rotate Right", "Ctrl+Shift+R"))
                    rotate_right_requested = true;
                if (ImGui::MenuItem("Flip Horizontal"))
                    flip_horizontal_requested = true;
                if (ImGui::MenuItem("Flip Vertical"))
                    flip_vertical_requested = true;
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About"))
                    ImGui::OpenPopup("About imiv");
                ImGui::EndMenu();
            }

#if defined(IMGUI_ENABLE_TEST_ENGINE)
            if (show_test_engine_windows
                && env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_MENU")
                && ImGui::BeginMenu("Tests")) {
                ImGui::MenuItem("Show test engine windows", nullptr,
                                show_test_engine_windows);
                ImGui::EndMenu();
            }
#endif
            ImGui::EndMainMenuBar();
        }

        if (open_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            open_image_dialog_action(vk_state, viewer, ui_state,
                                     ui_state.subimage_index,
                                     ui_state.miplevel_index);
#else
            set_placeholder_status(viewer, "Open image");
#endif
            open_requested = false;
        }
        if (!recent_open_path.empty()) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            load_viewer_image(vk_state, viewer, &ui_state, recent_open_path,
                              ui_state.subimage_index, ui_state.miplevel_index);
#else
            set_placeholder_status(viewer, "Open recent image");
#endif
            recent_open_path.clear();
        }
        if (clear_recent_requested) {
            viewer.recent_images.clear();
            viewer.status_message = "Cleared recent files list";
            viewer.last_error.clear();
            clear_recent_requested = false;
        }
        if (reload_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            reload_current_image_action(vk_state, viewer, ui_state);
#else
            set_placeholder_status(viewer, "Reload image");
#endif
            reload_requested = false;
        }
        if (close_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            close_current_image_action(vk_state, viewer);
#else
            set_placeholder_status(viewer, "Close image");
#endif
            close_requested = false;
        }
        if (prev_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            next_sibling_image_action(vk_state, viewer, ui_state, -1);
#else
            set_placeholder_status(viewer, "Previous Image");
#endif
            prev_requested = false;
        }
        if (next_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            next_sibling_image_action(vk_state, viewer, ui_state, 1);
#else
            set_placeholder_status(viewer, "Next Image");
#endif
            next_requested = false;
        }
        if (toggle_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            toggle_image_action(vk_state, viewer, ui_state);
#else
            set_placeholder_status(viewer, "Toggle image");
#endif
            toggle_requested = false;
        }
        if (prev_subimage_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            change_subimage_action(vk_state, viewer, ui_state, -1);
#else
            set_placeholder_status(viewer, "Prev Subimage");
#endif
            prev_subimage_requested = false;
        }
        if (next_subimage_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            change_subimage_action(vk_state, viewer, ui_state, 1);
#else
            set_placeholder_status(viewer, "Next Subimage");
#endif
            next_subimage_requested = false;
        }
        if (prev_mip_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            change_miplevel_action(vk_state, viewer, ui_state, -1);
#else
            set_placeholder_status(viewer, "Prev MIP level");
#endif
            prev_mip_requested = false;
        }
        if (next_mip_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            change_miplevel_action(vk_state, viewer, ui_state, 1);
#else
            set_placeholder_status(viewer, "Next MIP level");
#endif
            next_mip_requested = false;
        }
        if (save_as_requested) {
            save_as_dialog_action(viewer);
            save_as_requested = false;
        }
        if (save_window_as_requested) {
            save_window_as_dialog_action(viewer);
            save_window_as_requested = false;
        }
        if (save_selection_as_requested) {
            save_selection_as_dialog_action(viewer);
            save_selection_as_requested = false;
        }
        if (fit_window_to_image_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            fit_window_to_image_action(window, viewer, ui_state);
#else
            viewer.status_message = "Fit window to image is unavailable";
#endif
            fit_window_to_image_requested = false;
        }
        if (full_screen_toggle_requested) {
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
            full_screen_toggle_requested = false;
        }
        if (delete_from_disk_requested) {
#if defined(IMIV_BACKEND_VULKAN_GLFW)
            if (!viewer.image.path.empty()) {
                const std::string to_delete = viewer.image.path;
                close_current_image_action(vk_state, viewer);
                std::error_code ec;
                if (std::filesystem::remove(to_delete, ec)) {
                    viewer.status_message = Strutil::fmt::format("Deleted {}",
                                                                 to_delete);
                    viewer.last_error.clear();
                    refresh_sibling_images(viewer);
                } else {
                    viewer.last_error
                        = ec ? Strutil::fmt::format("Delete failed: {}",
                                                    ec.message())
                             : "Delete failed";
                }
            }
#endif
            delete_from_disk_requested = false;
        }
        if (rotate_left_requested || rotate_right_requested
            || flip_horizontal_requested || flip_vertical_requested) {
            if (!viewer.image.path.empty()) {
                int orientation = clamp_orientation(viewer.image.orientation);
                if (rotate_left_requested) {
                    static const int next_orientation[] = { 0, 8, 5, 6, 7,
                                                            4, 1, 2, 3 };
                    orientation = next_orientation[orientation];
                }
                if (rotate_right_requested) {
                    static const int next_orientation[] = { 0, 6, 7, 8, 5,
                                                            2, 3, 4, 1 };
                    orientation = next_orientation[orientation];
                }
                if (flip_horizontal_requested) {
                    static const int next_orientation[] = { 0, 2, 1, 4, 3,
                                                            6, 5, 8, 7 };
                    orientation = next_orientation[orientation];
                }
                if (flip_vertical_requested) {
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
            rotate_left_requested     = false;
            rotate_right_requested    = false;
            flip_horizontal_requested = false;
            flip_vertical_requested   = false;
        }

#if defined(IMIV_BACKEND_VULKAN_GLFW)
        if (ui_state.slide_show_running && has_image
            && !viewer.sibling_images.empty()) {
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
#endif
        clamp_placeholder_ui_state(ui_state);

        if (!viewer.image.path.empty()) {
            ui_state.subimage_index = viewer.image.subimage;
            ui_state.miplevel_index = viewer.image.miplevel;
        } else {
            ui_state.subimage_index = 0;
            ui_state.miplevel_index = 0;
        }

#if defined(IMIV_BACKEND_VULKAN_GLFW)
        if (!viewer.image.path.empty()) {
            PreviewControls preview_controls = {};
            preview_controls.exposure        = ui_state.exposure;
            preview_controls.gamma           = ui_state.gamma;
            preview_controls.offset          = ui_state.offset;
            preview_controls.color_mode      = ui_state.color_mode;
            preview_controls.channel         = ui_state.current_channel;
            preview_controls.use_ocio        = ui_state.use_ocio ? 1 : 0;
            preview_controls.orientation     = viewer.image.orientation;
            preview_controls.linear_interpolation
                = ui_state.linear_interpolation ? 1 : 0;
            std::string preview_error;
            if (!update_preview_texture(vk_state, viewer.texture,
                                        preview_controls, preview_error)) {
                if (!preview_error.empty())
                    viewer.last_error = preview_error;
            }
        }
#endif

        const ImGuiID main_dockspace_id = begin_main_dockspace_host();
        setup_image_window_policy(main_dockspace_id,
                                  ui_state.image_window_force_dock);
        ImGuiWindowFlags main_window_flags
            = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar
              | ImGuiWindowFlags_NoScrollWithMouse;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin(k_image_window_title, nullptr, main_window_flags);
        ImGui::PopStyleVar();
        ui_state.image_window_force_dock = !ImGui::IsWindowDocked();

        const float status_bar_height
            = std::max(30.0f, ImGui::GetTextLineHeightWithSpacing()
                                  + ImGui::GetStyle().FramePadding.y * 2.0f
                                  + 8.0f);
        ImVec2 content_avail   = ImGui::GetContentRegionAvail();
        const float viewport_h = std::max(64.0f,
                                          content_avail.y - status_bar_height);

        const ImVec2 viewport_padding(8.0f, 8.0f);
        ImageViewportLayout image_layout;
        int display_width  = 0;
        int display_height = 0;
        if (!viewer.image.path.empty()) {
            display_width  = viewer.image.width;
            display_height = viewer.image.height;
            oriented_image_dimensions(viewer.image, display_width,
                                      display_height);
            if ((viewer.fit_request || ui_state.fit_image_to_window)
                && display_width > 0 && display_height > 0) {
                const ImVec2 child_size(content_avail.x, viewport_h);
                viewer.zoom = compute_fit_zoom(child_size, viewport_padding,
                                               display_width, display_height);
                viewer.zoom_pivot_pending     = false;
                viewer.zoom_pivot_frames_left = 0;
                viewer.norm_scroll            = ImVec2(0.5f, 0.5f);
                viewer.fit_request            = false;
                viewer.scroll_sync_frames_left
                    = std::max(viewer.scroll_sync_frames_left, 2);
            }

            const ImVec2 image_size(static_cast<float>(display_width)
                                        * viewer.zoom,
                                    static_cast<float>(display_height)
                                        * viewer.zoom);
            if (recenter_requested)
                recenter_view(viewer, image_size);
            image_layout = compute_image_viewport_layout(
                ImVec2(content_avail.x, viewport_h), viewport_padding,
                image_size, ImGui::GetStyle().ScrollbarSize);
            sync_view_scroll_from_display_scroll(
                viewer,
                ImVec2(viewer.norm_scroll.x * image_size.x,
                       viewer.norm_scroll.y * image_size.y),
                image_size);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, viewport_padding);
        if (!viewer.image.path.empty()
            && (image_layout.scroll_x || image_layout.scroll_y)) {
            ImGui::SetNextWindowContentSize(image_layout.content_size);
            if (viewer.scroll_sync_frames_left > 0)
                ImGui::SetNextWindowScroll(viewer.scroll);
        }
        ImGui::BeginChild("Viewport", ImVec2(0.0f, viewport_h), false,
                          ImGuiWindowFlags_HorizontalScrollbar
                              | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();

        if (!viewer.last_error.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 90, 90, 255));
            ImGui::TextWrapped("%s", viewer.last_error.c_str());
            ImGui::PopStyleColor();
            register_layout_dump_synthetic_item("text",
                                                viewer.last_error.c_str());
        }

        if (!viewer.image.path.empty()) {
            const ImVec2 image_size = image_layout.image_size;
            ImTextureRef main_texture_ref;
            ImTextureRef closeup_texture_ref;
            bool has_main_texture     = false;
            bool has_closeup_texture  = false;
            bool image_canvas_pressed = false;
            bool image_canvas_hovered = false;
            bool image_canvas_active  = false;

#if defined(IMIV_BACKEND_VULKAN_GLFW)
            VkDescriptorSet main_set = ui_state.linear_interpolation
                                           ? viewer.texture.set
                                           : viewer.texture.nearest_mag_set;
            if (main_set == VK_NULL_HANDLE)
                main_set = viewer.texture.set;
            if (main_set != VK_NULL_HANDLE) {
                main_texture_ref = ImTextureRef(static_cast<ImTextureID>(
                    reinterpret_cast<uintptr_t>(main_set)));
                has_main_texture = true;
            }
            if (viewer.texture.pixelview_set != VK_NULL_HANDLE) {
                closeup_texture_ref = ImTextureRef(static_cast<ImTextureID>(
                    reinterpret_cast<uintptr_t>(viewer.texture.pixelview_set)));
                has_closeup_texture = true;
            } else if (viewer.texture.set != VK_NULL_HANDLE) {
                closeup_texture_ref = main_texture_ref;
                has_closeup_texture = true;
            }
#endif
            ImageCoordinateMap coord_map;
            coord_map.source_width  = viewer.image.width;
            coord_map.source_height = viewer.image.height;
            coord_map.orientation   = viewer.image.orientation;
            current_child_visible_rect(viewport_padding, image_layout.scroll_x,
                                       image_layout.scroll_y,
                                       coord_map.viewport_rect_min,
                                       coord_map.viewport_rect_max);
            const ImVec2 viewport_center
                = rect_center(coord_map.viewport_rect_min,
                              coord_map.viewport_rect_max);
            const bool can_scroll_x = image_layout.scroll_x;
            const bool can_scroll_y = image_layout.scroll_y;
            if (viewer.zoom_pivot_pending
                || viewer.zoom_pivot_frames_left > 0) {
                apply_pending_zoom_pivot(viewer, coord_map, image_size,
                                         can_scroll_x, can_scroll_y);
            } else if (viewer.scroll_sync_frames_left > 0) {
                if (can_scroll_x)
                    ImGui::SetScrollX(viewer.scroll.x);
                else
                    ImGui::SetScrollX(0.0f);
                if (can_scroll_y)
                    ImGui::SetScrollY(viewer.scroll.y);
                else
                    ImGui::SetScrollY(0.0f);
                --viewer.scroll_sync_frames_left;
            } else {
                ImVec2 imgui_scroll = viewer.scroll;
                if (can_scroll_x)
                    imgui_scroll.x = ImGui::GetScrollX();
                if (can_scroll_y)
                    imgui_scroll.y = ImGui::GetScrollY();
                sync_view_scroll_from_display_scroll(viewer, imgui_scroll,
                                                     image_size);
            }
            coord_map.valid = (image_size.x > 0.0f && image_size.y > 0.0f);
            coord_map.image_rect_min
                = ImVec2(viewport_center.x - viewer.scroll.x,
                         viewport_center.y - viewer.scroll.y);
            coord_map.image_rect_max
                = ImVec2(coord_map.image_rect_min.x + image_size.x,
                         coord_map.image_rect_min.y + image_size.y);
            update_test_engine_mouse_space(coord_map.viewport_rect_min,
                                           coord_map.viewport_rect_max,
                                           (has_image && coord_map.valid)
                                               ? coord_map.image_rect_min
                                               : ImVec2(0.0f, 0.0f),
                                           (has_image && coord_map.valid)
                                               ? coord_map.image_rect_max
                                               : ImVec2(0.0f, 0.0f));
            coord_map.window_pos = ImGui::GetWindowPos();
            if (has_main_texture && coord_map.valid) {
                ImGui::SetCursorScreenPos(coord_map.image_rect_min);
                image_canvas_pressed = ImGui::InvisibleButton(
                    "##image_canvas", image_size,
                    ImGuiButtonFlags_MouseButtonLeft
                        | ImGuiButtonFlags_MouseButtonRight
                        | ImGuiButtonFlags_MouseButtonMiddle);
                image_canvas_hovered = ImGui::IsItemHovered(
                    ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                image_canvas_active = ImGui::IsItemActive();
                register_layout_dump_synthetic_item("image", "Image");
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                draw_list->PushClipRect(coord_map.viewport_rect_min,
                                        coord_map.viewport_rect_max, true);
                draw_list->AddImage(main_texture_ref, coord_map.image_rect_min,
                                    coord_map.image_rect_max);
                draw_list->PopClipRect();
            } else if (!has_main_texture) {
                ImGui::TextUnformatted("No texture");
                register_layout_dump_synthetic_item("text", "No texture");
            }

            if (ui_state.show_window_guides && coord_map.valid) {
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                draw_list->AddRect(coord_map.image_rect_min,
                                   coord_map.image_rect_max,
                                   IM_COL32(250, 210, 80, 255), 0.0f, 0, 1.5f);
                draw_list->AddRect(coord_map.viewport_rect_min,
                                   coord_map.viewport_rect_max,
                                   IM_COL32(80, 200, 255, 220), 0.0f, 0, 1.0f);

                ImVec2 center_screen(0.0f, 0.0f);
                if (source_uv_to_screen(coord_map, ImVec2(0.5f, 0.5f),
                                        center_screen)) {
                    const float r = 6.0f;
                    draw_list->AddLine(ImVec2(center_screen.x - r,
                                              center_screen.y),
                                       ImVec2(center_screen.x + r,
                                              center_screen.y),
                                       IM_COL32(255, 170, 60, 255), 1.3f);
                    draw_list->AddLine(ImVec2(center_screen.x,
                                              center_screen.y - r),
                                       ImVec2(center_screen.x,
                                              center_screen.y + r),
                                       IM_COL32(255, 170, 60, 255), 1.3f);
                }
            }

            const ImGuiIO& io         = ImGui::GetIO();
            const ImVec2 mouse        = io.MousePos;
            const bool mouse_in_image = point_in_rect(mouse,
                                                      coord_map.image_rect_min,
                                                      coord_map.image_rect_max);
            const bool mouse_in_viewport
                = point_in_rect(mouse, coord_map.viewport_rect_min,
                                coord_map.viewport_rect_max);
            const bool viewport_hovered = ImGui::IsWindowHovered(
                ImGuiHoveredFlags_None);
            const bool viewport_accepts_mouse = viewport_hovered
                                                && mouse_in_viewport;
            const bool image_canvas_accepts_mouse = image_canvas_hovered
                                                    || image_canvas_active;
            const bool image_canvas_clicked_left
                = image_canvas_pressed
                  && io.MouseReleased[ImGuiMouseButton_Left];
            const bool image_canvas_clicked_right
                = image_canvas_pressed
                  && io.MouseReleased[ImGuiMouseButton_Right];
            const bool empty_viewport_clicked_left
                = viewport_accepts_mouse && !mouse_in_image
                  && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
            const bool empty_viewport_clicked_right
                = viewport_accepts_mouse && !mouse_in_image
                  && ImGui::IsMouseClicked(ImGuiMouseButton_Right);

            ImVec2 source_uv(0.0f, 0.0f);
            int px = 0;
            int py = 0;
            std::vector<double> sampled;
            if (viewport_accepts_mouse && mouse_in_image
                && screen_to_source_uv(coord_map, mouse, source_uv)
                && source_uv_to_pixel(coord_map, source_uv, px, py)
                && sample_loaded_pixel(viewer.image, px, py, sampled)) {
                viewer.probe_valid    = true;
                viewer.probe_x        = px;
                viewer.probe_y        = py;
                viewer.probe_channels = std::move(sampled);
            } else if (ui_state.pixelview_follows_mouse
                       && (!viewport_accepts_mouse || !mouse_in_image)) {
                viewer.probe_valid = false;
                viewer.probe_channels.clear();
            }

            static bool pan_drag_active  = false;
            static bool zoom_drag_active = false;
            static ImVec2 drag_prev_mouse(0.0f, 0.0f);

            bool want_pan       = false;
            bool want_zoom_drag = false;
            if (viewport_accepts_mouse || image_canvas_accepts_mouse
                || pan_drag_active || zoom_drag_active) {
                if (ui_state.mouse_mode == 1) {
                    want_pan = ImGui::IsMouseDown(ImGuiMouseButton_Left)
                               || ImGui::IsMouseDown(ImGuiMouseButton_Right)
                               || ImGui::IsMouseDown(ImGuiMouseButton_Middle);
                } else if (ui_state.mouse_mode == 0) {
                    const bool want_middle_pan
                        = ImGui::IsMouseDown(ImGuiMouseButton_Middle)
                          && (pan_drag_active || image_canvas_accepts_mouse
                              || viewport_accepts_mouse);
                    const bool want_alt_left_pan
                        = io.KeyAlt && ImGui::IsMouseDown(ImGuiMouseButton_Left)
                          && (pan_drag_active || image_canvas_accepts_mouse
                              || viewport_accepts_mouse);
                    want_pan = want_middle_pan || want_alt_left_pan;
                    want_zoom_drag
                        = io.KeyAlt
                          && ImGui::IsMouseDown(ImGuiMouseButton_Right)
                          && (zoom_drag_active || image_canvas_accepts_mouse
                              || viewport_accepts_mouse);
                    if (!io.KeyAlt
                        && (image_canvas_clicked_left
                            || image_canvas_clicked_right
                            || empty_viewport_clicked_left
                            || empty_viewport_clicked_right)) {
                        if (image_canvas_clicked_left
                            || empty_viewport_clicked_left)
                            request_zoom_scale(pending_zoom, 2.0f, true);
                        if (image_canvas_clicked_right
                            || empty_viewport_clicked_right)
                            request_zoom_scale(pending_zoom, 0.5f, true);
                    }
                }
            }

            if (want_pan) {
                if (!pan_drag_active) {
                    pan_drag_active = true;
                    drag_prev_mouse = mouse;
                } else {
                    const float dx = mouse.x - drag_prev_mouse.x;
                    const float dy = mouse.y - drag_prev_mouse.y;
                    sync_view_scroll_from_display_scroll(
                        viewer,
                        ImVec2(viewer.scroll.x - dx, viewer.scroll.y - dy),
                        image_size);
                    viewer.scroll_sync_frames_left
                        = std::max(viewer.scroll_sync_frames_left, 2);
                    drag_prev_mouse              = mouse;
                    viewer.fit_request           = false;
                    ui_state.fit_image_to_window = false;
                }
            } else {
                pan_drag_active = false;
            }

            if (want_zoom_drag) {
                if (!zoom_drag_active) {
                    zoom_drag_active = true;
                    drag_prev_mouse  = mouse;
                } else {
                    const float dx    = mouse.x - drag_prev_mouse.x;
                    const float dy    = mouse.y - drag_prev_mouse.y;
                    const float scale = 1.0f + 0.005f * (dx + dy);
                    if (scale > 0.0f)
                        request_zoom_scale(pending_zoom, scale, true);
                    drag_prev_mouse = mouse;
                }
            } else {
                zoom_drag_active = false;
            }

            if (viewport_accepts_mouse && io.MouseWheel != 0.0f) {
                const float scale = (io.MouseWheel > 0.0f) ? 1.1f : 0.9f;
                request_zoom_scale(pending_zoom, scale, true);
            }

            apply_zoom_request(coord_map, viewer, ui_state, pending_zoom,
                               mouse);

            if (apply_forced_probe_from_env(viewer))
                viewer.probe_valid = true;

            const OverlayPanelRect pixel_panel
                = draw_pixel_closeup_overlay(viewer, ui_state, coord_map,
                                             closeup_texture_ref,
                                             has_closeup_texture, fonts);
            draw_area_probe_overlay(viewer, ui_state, coord_map, pixel_panel,
                                    fonts);
        } else if (viewer.last_error.empty()) {
            viewer.probe_valid = false;
            viewer.probe_channels.clear();
            draw_padded_message(
                "No image loaded. Use File/Open to load an image.");
            register_layout_dump_synthetic_item("text", "No image loaded.");
        }

        ImGui::EndChild();
        ImGui::Separator();
        register_layout_dump_synthetic_item("divider", "Main viewport");
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
        ImGui::BeginChild("StatusBarRegion", ImVec2(0.0f, status_bar_height),
                          false,
                          ImGuiWindowFlags_NoScrollbar
                              | ImGuiWindowFlags_NoScrollWithMouse);
        draw_embedded_status_bar(viewer, ui_state);
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::End();

        draw_info_window(viewer, ui_state.show_info_window);
        draw_preferences_window(ui_state, ui_state.show_preferences_window);
        draw_preview_window(ui_state, ui_state.show_preview_window);

        if (ImGui::BeginPopupModal("About imiv", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("imiv (Dear ImGui port of iv)");
            register_layout_dump_synthetic_item("text", "About imiv title");
            ImGui::TextUnformatted(
                "Image viewer port built with Dear ImGui and Vulkan.");
            register_layout_dump_synthetic_item("text", "About imiv body");
            if (ImGui::Button("Close"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

}  // namespace

RenderBackend
default_backend()
{
#if defined(IMIV_BACKEND_METAL_GLFW)
    return RenderBackend::MetalGlfw;
#elif defined(IMIV_BACKEND_VULKAN_GLFW)
    return RenderBackend::VulkanGlfw;
#else
#    error "imiv backend policy macro is not configured"
#endif
}

const char*
backend_name(RenderBackend backend)
{
    switch (backend) {
    case RenderBackend::VulkanGlfw: return "glfw+vulkan";
    case RenderBackend::MetalGlfw: return "glfw+metal";
    }
    return "unknown";
}

int
run(const AppConfig& config)
{
#if !defined(IMIV_BACKEND_VULKAN_GLFW)
    print(stderr, "imiv: backend '{}' is not implemented yet in this build\n",
          backend_name(default_backend()));
    (void)config;
    return EXIT_FAILURE;
#else
    AppConfig run_config = config;
    run_config.input_paths.erase(
        std::remove_if(run_config.input_paths.begin(),
                       run_config.input_paths.end(),
                       [](const std::string& path) {
                           return trim_ascii(path).empty();
                       }),
        run_config.input_paths.end());

    std::string startup_open_path;
    if (run_config.input_paths.empty()
        && read_env_value("IMIV_IMGUI_TEST_ENGINE_OPEN_PATH", startup_open_path)
        && !startup_open_path.empty()) {
        run_config.input_paths.push_back(startup_open_path);
    }

    const bool verbose_logging = run_config.verbose;
    const bool verbose_validation_output
        = verbose_logging
          || env_flag_is_truthy("IMIV_VULKAN_VERBOSE_VALIDATION");
    const bool log_imgui_texture_updates = env_flag_is_truthy(
        "IMIV_DEBUG_IMGUI_TEXTURES");

#    if defined(IMGUI_ENABLE_TEST_ENGINE)
    TestEngineConfig test_engine_cfg = gather_test_engine_config();
    TestEngineRuntime test_engine_runtime;
#    else
    bool want_test_engine
        = env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE")
          || env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT")
          || env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP")
          || env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_JUNIT_XML");
#    endif

    glfwSetErrorCallback(glfw_error_callback);
    configure_glfw_platform_preference(verbose_logging);
    if (!glfwInit()) {
        print(stderr, "imiv: glfwInit failed\n");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(1600, 900, "imiv", nullptr, nullptr);
    if (!window) {
        print(stderr, "imiv: failed to create GLFW window\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }
    center_glfw_window(window);

    if (!glfwVulkanSupported()) {
        print(stderr, "imiv: GLFW reports Vulkan is not supported\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    IMGUI_CHECKVERSION();
    if (!ImGui::CreateContext()) {
        print(stderr, "imiv: failed to create Dear ImGui context\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    VulkanState vk_state;
    vk_state.verbose_logging           = verbose_logging;
    vk_state.verbose_validation_output = verbose_validation_output;
    vk_state.log_imgui_texture_updates = log_imgui_texture_updates;
    ImVector<const char*> instance_extensions;
    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions  = glfwGetRequiredInstanceExtensions(
        &glfw_extension_count);
    for (uint32_t i = 0; i < glfw_extension_count; ++i)
        instance_extensions.push_back(glfw_extensions[i]);

    std::string startup_error;
    if (!setup_vulkan_instance(vk_state, instance_extensions, startup_error)) {
        print(stderr, "imiv: {}\n", startup_error);
        cleanup_vulkan(vk_state);
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    VkResult err = glfwCreateWindowSurface(vk_state.instance, window,
                                           vk_state.allocator,
                                           &vk_state.surface);
    if (err != VK_SUCCESS) {
        print(stderr, "imiv: glfwCreateWindowSurface failed\n");
        cleanup_vulkan(vk_state);
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    if (!setup_vulkan_device(vk_state, startup_error)) {
        print(stderr, "imiv: {}\n", startup_error);
        destroy_vulkan_surface(vk_state);
        cleanup_vulkan(vk_state);
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    int framebuffer_width  = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    if (!setup_vulkan_window(vk_state, framebuffer_width, framebuffer_height,
                             startup_error)) {
        print(stderr, "imiv: {}\n", startup_error);
        cleanup_vulkan_window(vk_state);
        cleanup_vulkan(vk_state);
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.IniFilename       = "imiv.ini";
    const AppFonts fonts = setup_app_fonts(verbose_logging);
    ImGui::StyleColorsDark();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGuiStyle& style                 = ImGui::GetStyle();
        style.WindowRounding              = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo init_info    = {};
    init_info.ApiVersion                   = vk_state.api_version;
    init_info.Instance                     = vk_state.instance;
    init_info.PhysicalDevice               = vk_state.physical_device;
    init_info.Device                       = vk_state.device;
    init_info.QueueFamily                  = vk_state.queue_family;
    init_info.Queue                        = vk_state.queue;
    init_info.PipelineCache                = vk_state.pipeline_cache;
    init_info.DescriptorPool               = vk_state.descriptor_pool;
    init_info.MinImageCount                = vk_state.min_image_count;
    init_info.ImageCount                   = vk_state.window_data.ImageCount;
    init_info.Allocator                    = vk_state.allocator;
    init_info.PipelineInfoMain.RenderPass  = vk_state.window_data.RenderPass;
    init_info.PipelineInfoMain.Subpass     = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn              = check_vk_result;
    if (!ImGui_ImplVulkan_Init(&init_info)) {
        print(stderr, "imiv: ImGui_ImplVulkan_Init failed\n");
        ImGui_ImplGlfw_Shutdown();
        cleanup_vulkan_window(vk_state);
        cleanup_vulkan(vk_state);
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    const bool platform_has_viewports
        = (io.BackendFlags & ImGuiBackendFlags_PlatformHasViewports) != 0;
    const bool renderer_has_viewports
        = (io.BackendFlags & ImGuiBackendFlags_RendererHasViewports) != 0;
#    if defined(IMIV_BACKEND_VULKAN_GLFW) && defined(GLFW_VERSION_MAJOR) \
        && ((GLFW_VERSION_MAJOR > 3)                                     \
            || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4))
    const int selected_glfw_platform = glfwGetPlatform();
#    else
    const int selected_glfw_platform = 0;
#    endif
    if (verbose_logging) {
        print("imiv: GLFW selected platform={} imgui_viewports platform={} "
              "renderer={}\n",
              glfw_platform_name(selected_glfw_platform),
              platform_has_viewports ? "yes" : "no",
              renderer_has_viewports ? "yes" : "no");
    }
    if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        && (!platform_has_viewports || !renderer_has_viewports)) {
        io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
        print("imiv: detached auxiliary windows disabled because the active "
              "GLFW platform backend does not support Dear ImGui "
              "multi-viewports\n");
    }

    if (run_config.verbose) {
        print("imiv: bootstrap initialized (backend policy: {})\n",
              backend_name(default_backend()));
        print("imiv: startup queue has {} image path(s)\n",
              run_config.input_paths.size());
        print("imiv: native file dialogs: {}\n",
              FileDialog::available() ? "enabled" : "disabled");
    }

#    if defined(IMGUI_ENABLE_TEST_ENGINE)
    g_imiv_test_runtime = nullptr;
    if (test_engine_cfg.want_test_engine) {
        test_engine_runtime.request_exit = false;
        test_engine_runtime.show_windows = test_engine_cfg.show_windows;
        test_engine_runtime.engine       = ImGuiTestEngine_CreateContext();
        g_imiv_test_runtime              = &test_engine_runtime;

        ImGuiTestEngineIO& test_io = ImGuiTestEngine_GetIO(
            test_engine_runtime.engine);
        test_io.ConfigVerboseLevel        = ImGuiTestVerboseLevel_Info;
        test_io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;
        test_io.ConfigRunSpeed            = ImGuiTestRunSpeed_Normal;
        test_io.ConfigCaptureEnabled      = true;
        test_io.ScreenCaptureFunc         = imiv_vulkan_screen_capture;
        test_io.ScreenCaptureUserData     = &vk_state;
        if (test_engine_cfg.trace || test_engine_cfg.automation_mode)
            test_io.ConfigLogToTTY = true;

        if (test_engine_cfg.junit_xml) {
            std::error_code ec;
            std::filesystem::path junit_path(test_engine_cfg.junit_xml_out);
            if (!junit_path.parent_path().empty())
                std::filesystem::create_directories(junit_path.parent_path(),
                                                    ec);
            test_io.ExportResultsFormat = ImGuiTestEngineExportFormat_JUnitXml;
            test_io.ExportResultsFilename
                = test_engine_cfg.junit_xml_out.c_str();
        }

        ImGuiTestEngine_Start(test_engine_runtime.engine,
                              ImGui::GetCurrentContext());
        if (test_engine_cfg.auto_screenshot) {
            ImGuiTest* smoke = register_imiv_smoke_tests(
                test_engine_runtime.engine);
            ImGuiTestEngine_QueueTest(test_engine_runtime.engine, smoke);
            test_engine_cfg.has_work = true;
        }
        if (test_engine_cfg.layout_dump) {
            ImGuiTest* dump = register_imiv_layout_dump_tests(
                test_engine_runtime.engine);
            ImGuiTestEngine_QueueTest(test_engine_runtime.engine, dump);
            test_engine_cfg.has_work = true;
        }
        if (test_engine_cfg.state_dump) {
            ImGuiTest* dump = register_imiv_state_dump_tests(
                test_engine_runtime.engine);
            ImGuiTestEngine_QueueTest(test_engine_runtime.engine, dump);
            test_engine_cfg.has_work = true;
        }
    }
#    else
    if (want_test_engine) {
        print(stderr,
              "imiv: IMIV_IMGUI_TEST_ENGINE requested but support is not "
              "compiled in. Configure with "
              "-DOIIO_IMIV_ENABLE_IMGUI_TEST_ENGINE=ON.\n");
    }
#    endif

    if (run_config.open_dialog) {
        FileDialog::DialogReply reply = FileDialog::open_image_file("");
        if (reply.result == FileDialog::Result::Okay)
            print("imiv: open dialog selected '{}'\n", reply.path);
        else if (reply.result == FileDialog::Result::Cancel)
            print("imiv: open dialog cancelled\n");
        else
            print(stderr, "imiv: open dialog failed: {}\n", reply.message);
    }
    if (run_config.save_dialog) {
        FileDialog::DialogReply reply
            = FileDialog::save_image_file("", "image.exr");
        if (reply.result == FileDialog::Result::Okay)
            print("imiv: save dialog selected '{}'\n", reply.path);
        else if (reply.result == FileDialog::Result::Cancel)
            print("imiv: save dialog cancelled\n");
        else
            print(stderr, "imiv: save dialog failed: {}\n", reply.message);
    }

    ViewerState viewer;
    PlaceholderUiState ui_state;
    std::string prefs_error;
    if (!load_persistent_state(ui_state, viewer, prefs_error)) {
        print(stderr, "imiv: failed to load preferences: {}\n", prefs_error);
        viewer.last_error
            = Strutil::fmt::format("failed to load preferences: {}",
                                   prefs_error);
    }
    if (!run_config.ocio_display.empty())
        ui_state.ocio_display = run_config.ocio_display;
    if (!run_config.ocio_view.empty())
        ui_state.ocio_view = run_config.ocio_view;
    if (!run_config.ocio_image_color_space.empty())
        ui_state.ocio_image_color_space = run_config.ocio_image_color_space;
    ui_state.use_ocio = (!run_config.ocio_display.empty()
                         || !run_config.ocio_view.empty()
                         || !run_config.ocio_image_color_space.empty());
    reset_per_image_preview_state(ui_state);
    clamp_placeholder_ui_state(ui_state);
    if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_AUX_WINDOWS")) {
        ui_state.show_info_window        = true;
        ui_state.show_preferences_window = true;
        ui_state.show_preview_window     = true;
        ui_state.show_pixelview_window   = true;
        ui_state.show_area_probe_window  = true;
    }
    if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_INFO"))
        ui_state.show_info_window = true;
    if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_PREFS"))
        ui_state.show_preferences_window = true;
    if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_PREVIEW"))
        ui_state.show_preview_window = true;
    if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_PIXEL"))
        ui_state.show_pixelview_window = true;
    if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_AREA"))
        ui_state.show_area_probe_window = true;
    if (ui_state.dark_palette)
        ImGui::StyleColorsDark();
    else
        ImGui::StyleColorsLight();

    if (!run_config.input_paths.empty()) {
        if (!load_viewer_image(vk_state, viewer, &ui_state,
                               run_config.input_paths[0],
                               ui_state.subimage_index,
                               ui_state.miplevel_index)) {
            print(stderr, "imiv: startup load failed for '{}'\n",
                  run_config.input_paths[0]);
        }
    } else {
        viewer.status_message = "Open an image to start preview";
    }

    glfwShowWindow(window);
    glfwPollEvents();
    force_center_glfw_window(window);

    bool request_exit         = false;
    bool applied_dark_palette = ui_state.dark_palette;
    int startup_center_frames = 90;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (startup_center_frames > 0) {
            center_glfw_window(window);
            --startup_center_frames;
        }

        int fb_width  = 0;
        int fb_height = 0;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);
        if (fb_width > 0 && fb_height > 0
            && (vk_state.swapchain_rebuild
                || vk_state.window_data.Width != fb_width
                || vk_state.window_data.Height != fb_height)) {
            ImGui_ImplVulkan_SetMinImageCount(vk_state.min_image_count);
            ImGui_ImplVulkanH_CreateOrResizeWindow(
                vk_state.instance, vk_state.physical_device, vk_state.device,
                &vk_state.window_data, vk_state.queue_family,
                vk_state.allocator, fb_width, fb_height,
                vk_state.min_image_count, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
            name_window_frame_objects(vk_state);
            vk_state.window_data.FrameIndex = 0;
            vk_state.swapchain_rebuild      = false;
        }
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
#    if defined(IMGUI_ENABLE_TEST_ENGINE)
        g_imiv_test_viewer   = &viewer;
        g_imiv_test_ui_state = &ui_state;
#    endif
        draw_viewer_ui(viewer, ui_state, fonts, request_exit
#    if defined(IMGUI_ENABLE_TEST_ENGINE)
                       ,
                       test_engine_runtime.engine
                           ? &test_engine_runtime.show_windows
                           : nullptr
#    endif
                       ,
                       window, vk_state);
        if (ui_state.dark_palette != applied_dark_palette) {
            if (ui_state.dark_palette)
                ImGui::StyleColorsDark();
            else
                ImGui::StyleColorsLight();
            applied_dark_palette = ui_state.dark_palette;
        }
#    if defined(IMGUI_ENABLE_TEST_ENGINE)
        if (test_engine_runtime.engine && test_engine_runtime.show_windows
            && !test_engine_cfg.automation_mode) {
            ImGuiTestEngine_ShowTestEngineWindows(test_engine_runtime.engine,
                                                  nullptr);
        }
#    endif

        ImGui::Render();
        ImDrawData* draw_data        = ImGui::GetDrawData();
        const bool main_is_minimized = (draw_data->DisplaySize.x <= 0.0f
                                        || draw_data->DisplaySize.y <= 0.0f);
        vk_state.window_data.ClearValue.color.float32[0] = 0.08f;
        vk_state.window_data.ClearValue.color.float32[1] = 0.08f;
        vk_state.window_data.ClearValue.color.float32[2] = 0.08f;
        vk_state.window_data.ClearValue.color.float32[3] = 1.0f;
        if (!main_is_minimized)
            frame_render(vk_state, draw_data);
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
#    if defined(IMGUI_ENABLE_TEST_ENGINE)
        if (test_engine_runtime.engine)
            ImGuiTestEngine_PostSwap(test_engine_runtime.engine);
#    endif
        if (!main_is_minimized)
            frame_present(vk_state);

#    if defined(IMGUI_ENABLE_TEST_ENGINE)
        if (test_engine_runtime.engine) {
            if (test_engine_runtime.request_exit
                && !test_engine_cfg.exit_on_finish)
                glfwSetWindowShouldClose(window, GLFW_TRUE);

            if (test_engine_cfg.exit_on_finish && test_engine_cfg.has_work) {
                ImGuiTestEngineIO& te_io = ImGuiTestEngine_GetIO(
                    test_engine_runtime.engine);
                if (!te_io.IsRunningTests && !te_io.IsCapturing
                    && ImGuiTestEngine_IsTestQueueEmpty(
                        test_engine_runtime.engine)) {
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
            }
        }
#    endif
        if (request_exit)
            glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    std::string prefs_save_error;
    if (!save_persistent_state(ui_state, viewer, prefs_save_error))
        print(stderr, "imiv: failed to save preferences: {}\n",
              prefs_save_error);

    err = vkDeviceWaitIdle(vk_state.device);
    check_vk_result(err);

    destroy_texture(vk_state, viewer.texture);
#    if defined(IMGUI_ENABLE_TEST_ENGINE)
    if (test_engine_runtime.engine)
        ImGuiTestEngine_Stop(test_engine_runtime.engine);
#    endif
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
#    if defined(IMGUI_ENABLE_TEST_ENGINE)
    if (test_engine_runtime.engine) {
        ImGuiTestEngine_DestroyContext(test_engine_runtime.engine);
        test_engine_runtime.engine = nullptr;
    }
    g_imiv_test_runtime  = nullptr;
    g_imiv_test_viewer   = nullptr;
    g_imiv_test_ui_state = nullptr;
#    endif

    cleanup_vulkan_window(vk_state);
    cleanup_vulkan(vk_state);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
#endif
}

}  // namespace Imiv
