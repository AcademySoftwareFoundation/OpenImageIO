// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_app.h"
#include "imiv_actions.h"
#include "imiv_build_config.h"
#include "imiv_developer_tools.h"
#include "imiv_drag_drop.h"
#include "imiv_file_dialog.h"
#include "imiv_frame.h"
#include "imiv_image_library.h"
#include "imiv_menu.h"
#include "imiv_navigation.h"
#include "imiv_ocio.h"
#include "imiv_parse.h"
#include "imiv_persistence.h"
#include "imiv_platform_glfw.h"
#include "imiv_renderer.h"
#include "imiv_style.h"
#include "imiv_test_engine.h"
#include "imiv_types.h"
#include "imiv_ui.h"
#include "imiv_viewer.h"

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

#include <OpenImageIO/half.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>

using namespace OIIO;

#if defined(IMIV_EMBED_FONTS) && IMIV_EMBED_FONTS
#    include "imiv_font_droidsans_ttf.h"
#    include "imiv_font_droidsansmono_ttf.h"
#endif

namespace Imiv {

namespace {

    struct NativeDialogWindowScope {
        GLFWwindow* window            = nullptr;
        PlaceholderUiState* ui_state  = nullptr;
        int suspend_depth             = 0;
        bool restore_floating_on_exit = false;
    };

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

    ImFont* load_embedded_font_if_present(const unsigned char* data,
                                          size_t size_bytes, float size_pixels)
    {
        if (data == nullptr || size_bytes == 0)
            return nullptr;
        ImGuiIO& io = ImGui::GetIO();
        ImFontConfig config;
        config.FontDataOwnedByAtlas = false;
        return io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(data),
                                              static_cast<int>(size_bytes),
                                              size_pixels, &config);
    }

    AppFonts setup_app_fonts(bool verbose_logging)
    {
        AppFonts fonts;
        ImGuiIO& io                  = ImGui::GetIO();
        const char* ui_font_source   = "missing";
        const char* mono_font_source = "missing";

        const std::filesystem::path font_root = executable_directory_path()
                                                / "fonts";
        const std::filesystem::path ui_font_path = font_root / "Droid_Sans"
                                                   / "DroidSans.ttf";
        const std::filesystem::path mono_font_path
            = font_root / "Droid_Sans_Mono" / "DroidSansMono.ttf";

#if defined(IMIV_EMBED_FONTS) && IMIV_EMBED_FONTS
        fonts.ui = load_embedded_font_if_present(g_imiv_font_droidsans_ttf,
                                                 g_imiv_font_droidsans_ttf_size,
                                                 16.0f);
        if (fonts.ui)
            ui_font_source = "embedded";
        fonts.mono
            = load_embedded_font_if_present(g_imiv_font_droidsansmono_ttf,
                                            g_imiv_font_droidsansmono_ttf_size,
                                            16.0f);
        if (fonts.mono)
            mono_font_source = "embedded";
#endif

        if (!fonts.ui) {
            fonts.ui = load_font_if_present(ui_font_path, 16.0f);
            if (fonts.ui)
                ui_font_source = "file";
        }
        if (!fonts.mono) {
            fonts.mono = load_font_if_present(mono_font_path, 16.0f);
            if (fonts.mono)
                mono_font_source = "file";
        }
        if (!fonts.ui) {
            fonts.ui       = io.Fonts->AddFontDefault();
            ui_font_source = "default";
        }
        if (!fonts.mono) {
            fonts.mono       = fonts.ui;
            mono_font_source = (fonts.ui == fonts.mono && ui_font_source)
                                   ? ui_font_source
                                   : "default";
        }
        io.FontDefault = fonts.ui;

        print("imiv: fonts ui={} mono={}\n", ui_font_source, mono_font_source);
        return fonts;
    }
    bool default_developer_mode_enabled()
    {
#if defined(NDEBUG)
        return false;
#else
        return true;
#endif
    }

    bool resolve_developer_mode_enabled(const AppConfig& config,
                                        bool verbose_logging)
    {
        bool enabled = default_developer_mode_enabled();

        std::string env_value;
        if (read_env_value("OIIO_DEVMODE", env_value)) {
            bool parsed_value = false;
            if (parse_bool_string(env_value, parsed_value)) {
                enabled = parsed_value;
            } else if (verbose_logging) {
                print(stderr,
                      "imiv: ignoring invalid OIIO_DEVMODE value '{}'; "
                      "expected 0/1/true/false/on/off/yes/no\n",
                      env_value);
            }
        }

        if (config.developer_mode_explicit)
            enabled = config.developer_mode;
        return enabled;
    }

    BackendKind requested_backend_for_launch(const AppConfig& config,
                                             const PlaceholderUiState& ui_state)
    {
        if (config.requested_backend != BackendKind::Auto)
            return config.requested_backend;
        return sanitize_backend_kind(ui_state.renderer_backend);
    }

    DisplayFormatPreference
    requested_display_format_for_launch(const AppConfig& config,
                                        const PlaceholderUiState& ui_state,
                                        bool verbose_logging)
    {
        DisplayFormatPreference requested = sanitize_display_format_preference(
            ui_state.display_format);
        std::string env_value;
        if (read_env_value("IMIV_DISPLAY_FORMAT", env_value)) {
            DisplayFormatPreference env_format = DisplayFormatPreference::Auto;
            if (parse_display_format_preference(env_value, env_format)) {
                requested = env_format;
            } else if (verbose_logging) {
                print(stderr,
                      "imiv: ignoring invalid IMIV_DISPLAY_FORMAT value '{}'; "
                      "expected auto/rgba8/rgb10a2/hdr\n",
                      env_value);
            }
        }
        if (config.display_format_explicit)
            requested = config.requested_display_format;
        return sanitize_display_format_preference(static_cast<int>(requested));
    }

    void apply_glfw_topmost_state_to_platform_windows(GLFWwindow* main_window,
                                                      bool always_on_top)
    {
        if (main_window != nullptr)
            platform_glfw_set_window_floating(main_window, always_on_top);

        ImGuiContext* ctx = ImGui::GetCurrentContext();
        if (ctx == nullptr)
            return;
        if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            == 0) {
            return;
        }

        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        for (int i = 0; i < platform_io.Viewports.Size; ++i) {
            ImGuiViewport* viewport = platform_io.Viewports[i];
            if (viewport == nullptr || viewport->PlatformHandle == nullptr)
                continue;
            GLFWwindow* viewport_window = static_cast<GLFWwindow*>(
                viewport->PlatformHandle);
            platform_glfw_set_window_floating(viewport_window, always_on_top);
        }
    }

    void native_dialog_scope_callback(bool begin_scope, void* user_data)
    {
        auto* scope = static_cast<NativeDialogWindowScope*>(user_data);
        if (scope == nullptr || scope->window == nullptr
            || scope->ui_state == nullptr)
            return;

        if (begin_scope) {
            ++scope->suspend_depth;
            if (scope->suspend_depth == 1
                && scope->ui_state->window_always_on_top
                && platform_glfw_is_window_floating(scope->window)) {
                apply_glfw_topmost_state_to_platform_windows(scope->window,
                                                             false);
                scope->restore_floating_on_exit = true;
            }
            return;
        }

        if (scope->suspend_depth > 0)
            --scope->suspend_depth;
        if (scope->suspend_depth == 0 && scope->restore_floating_on_exit) {
            apply_glfw_topmost_state_to_platform_windows(
                scope->window, scope->ui_state->window_always_on_top);
            scope->restore_floating_on_exit = false;
        }
    }

    std::vector<std::string> expand_startup_input_paths(const AppConfig& config,
                                                        bool verbose_logging,
                                                        ImageSortMode sort_mode,
                                                        bool sort_reverse)
    {
        std::vector<std::string> expanded;
        for (const std::string& input_path : config.input_paths) {
            std::error_code ec;
            const std::filesystem::path path(input_path);
            if (std::filesystem::is_directory(path, ec) && !ec) {
                std::vector<std::string> directory_paths;
                std::string error_message;
                if (!collect_directory_image_paths(input_path, sort_mode,
                                                   sort_reverse,
                                                   directory_paths,
                                                   error_message)) {
                    print(stderr, "imiv: {}\n", error_message);
                    continue;
                }
                if (directory_paths.empty()) {
                    print(stderr,
                          "imiv: no supported image files found in '{}'\n",
                          input_path);
                    continue;
                }
                expanded.insert(expanded.end(), directory_paths.begin(),
                                directory_paths.end());
                continue;
            }
            if (ec && verbose_logging) {
                print(stderr,
                      "imiv: ignoring directory probe error for '{}': {}\n",
                      input_path, ec.message());
            }
            expanded.push_back(input_path);
        }
        return expanded;
    }
}  // namespace



int
run(const AppConfig& config)
{
    AppConfig run_config = config;
    run_config.input_paths.erase(
        std::remove_if(run_config.input_paths.begin(),
                       run_config.input_paths.end(),
                       [](const std::string& path) {
                           return Strutil::strip(path).empty();
                       }),
        run_config.input_paths.end());

    std::string startup_open_path;
    if (run_config.input_paths.empty()
        && read_env_value("IMIV_IMGUI_TEST_ENGINE_OPEN_PATH", startup_open_path)
        && !startup_open_path.empty()) {
        run_config.input_paths.push_back(startup_open_path);
    }
    MultiViewWorkspace workspace;
    ImageLibraryState library;
    PlaceholderUiState ui_state;
    ViewerState& viewer = ensure_primary_image_view(workspace).viewer;
    DeveloperUiState developer_ui;
    viewer.rawcolor       = run_config.rawcolor;
    viewer.no_autopremult = run_config.no_autopremult;
    std::string prefs_error;
    if (!load_persistent_state(ui_state, viewer, library, prefs_error)) {
        print(stderr, "imiv: failed to load preferences: {}\n", prefs_error);
        viewer.last_error
            = Strutil::fmt::format("failed to load preferences: {}",
                                   prefs_error);
    }
    run_config.input_paths
        = expand_startup_input_paths(run_config, run_config.verbose,
                                     library.sort_mode, library.sort_reverse);
    const BackendKind requested_backend
        = requested_backend_for_launch(run_config, ui_state);
    DisplayFormatPreference requested_display_format
        = requested_display_format_for_launch(run_config, ui_state,
                                              run_config.verbose);
    if (requested_display_format == DisplayFormatPreference::Hdr) {
        print(
            "imiv: display format 'hdr' is not implemented yet; using auto\n");
        requested_display_format = DisplayFormatPreference::Auto;
    }

    const bool verbose_logging = run_config.verbose;
    const bool verbose_validation_output
        = verbose_logging
          || env_flag_is_truthy("IMIV_VULKAN_VERBOSE_VALIDATION");
    const bool log_imgui_texture_updates = env_flag_is_truthy(
        "IMIV_DEBUG_IMGUI_TEXTURES");
    developer_ui.enabled = resolve_developer_mode_enabled(run_config,
                                                          verbose_logging);

#if defined(IMGUI_ENABLE_TEST_ENGINE)
    TestEngineConfig test_engine_cfg = gather_test_engine_config();
    TestEngineRuntime test_engine_runtime;
#else
    bool want_test_engine
        = env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE")
          || env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT")
          || env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP")
          || env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_JUNIT_XML");
#endif

    std::string startup_error;
    if (!platform_glfw_init(verbose_logging, startup_error)) {
        print(stderr, "imiv: {}\n", startup_error);
        return EXIT_FAILURE;
    }

    refresh_runtime_backend_info(verbose_logging, startup_error);

    const BackendKind active_backend = resolve_backend_request(
        requested_backend);
    if (active_backend == BackendKind::Auto) {
        if (compiled_backend_count() == 0) {
            print(stderr,
                  "imiv: no renderer backend is compiled into this build\n");
        } else {
            print(stderr,
                  "imiv: no compiled renderer backend is currently available\n");
            for (const BackendRuntimeInfo& info : runtime_backend_info()) {
                if (!info.build_info.compiled || info.available)
                    continue;
                print(stderr, "imiv:   {} unavailable: {}\n",
                      info.build_info.display_name,
                      info.unavailable_reason.empty()
                          ? "backend is unavailable"
                          : info.unavailable_reason);
            }
        }
        platform_glfw_terminate();
        return EXIT_FAILURE;
    }
    if (requested_backend != BackendKind::Auto
        && requested_backend != active_backend) {
        if (!backend_kind_is_compiled(requested_backend)) {
            print("imiv: requested backend '{}' is not compiled into this "
                  "build; using '{}'\n",
                  backend_cli_name(requested_backend),
                  backend_runtime_name(active_backend));
        } else {
            const std::string_view unavailable_reason
                = backend_unavailable_reason(requested_backend);
            print("imiv: requested backend '{}' is unavailable at runtime{}; "
                  "using '{}'\n",
                  backend_cli_name(requested_backend),
                  unavailable_reason.empty()
                      ? ""
                      : Strutil::fmt::format(" ({})", unavailable_reason),
                  backend_runtime_name(active_backend));
        }
    }

    const std::string window_title
        = Strutil::fmt::format("ImIv v.{} [{}]", OIIO_VERSION_STRING,
                               backend_cli_name(active_backend));
    GLFWwindow* window
        = platform_glfw_create_main_window(active_backend,
                                           requested_display_format, 1600, 900,
                                           window_title.c_str(), startup_error);
    if (window == nullptr) {
        print(stderr, "imiv: {}\n", startup_error);
        platform_glfw_terminate();
        return EXIT_FAILURE;
    }

    if (active_backend == BackendKind::Vulkan
        && !platform_glfw_supports_vulkan(startup_error)) {
        print(stderr, "imiv: {}\n", startup_error);
        platform_glfw_destroy_window(window);
        platform_glfw_terminate();
        return EXIT_FAILURE;
    }

    IMGUI_CHECKVERSION();
    if (!ImGui::CreateContext()) {
        print(stderr, "imiv: failed to create Dear ImGui context\n");
        platform_glfw_destroy_window(window);
        platform_glfw_terminate();
        return EXIT_FAILURE;
    }

    RendererState renderer_state;
    renderer_select_backend(renderer_state, active_backend);
    renderer_state.requested_display_format  = requested_display_format;
    renderer_state.verbose_logging           = verbose_logging;
    renderer_state.verbose_validation_output = verbose_validation_output;
    renderer_state.log_imgui_texture_updates = log_imgui_texture_updates;
    const auto fail_bootstrap = [&](bool destroy_surface, bool cleanup_window,
                                    bool shutdown_platform_imgui) {
        print(stderr, "imiv: {}\n", startup_error);
        if (shutdown_platform_imgui)
            platform_glfw_imgui_shutdown();
        if (cleanup_window)
            renderer_cleanup_window(renderer_state);
        if (destroy_surface)
            renderer_destroy_surface(renderer_state);
        renderer_cleanup(renderer_state);
        ImGui::DestroyContext();
        platform_glfw_destroy_window(window);
        platform_glfw_terminate();
        return EXIT_FAILURE;
    };
    ImVector<const char*> instance_extensions;
    if (active_backend == BackendKind::Vulkan)
        platform_glfw_collect_vulkan_instance_extensions(instance_extensions);

    if (!renderer_setup_instance(renderer_state, instance_extensions,
                                 startup_error)) {
        return fail_bootstrap(false, false, false);
    }

    if (!renderer_create_surface(renderer_state, window, startup_error)) {
        return fail_bootstrap(false, false, false);
    }

    if (!renderer_setup_device(renderer_state, startup_error)) {
        return fail_bootstrap(true, false, false);
    }

    int framebuffer_width  = 0;
    int framebuffer_height = 0;
    platform_glfw_get_framebuffer_size(window, framebuffer_width,
                                       framebuffer_height);
    if (!renderer_setup_window(renderer_state, framebuffer_width,
                               framebuffer_height, startup_error)) {
        return fail_bootstrap(false, true, false);
    }

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.IniFilename       = nullptr;
    const AppFonts fonts = setup_app_fonts(verbose_logging);
    apply_imgui_app_style(AppStylePreset::ImGuiDark);
    const std::filesystem::path settings_load_path = imgui_ini_load_path();
    if (!settings_load_path.empty())
        ImGui::LoadIniSettingsFromDisk(settings_load_path.string().c_str());
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGuiStyle& style                 = ImGui::GetStyle();
        style.WindowRounding              = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    platform_glfw_imgui_init(window, active_backend);
    if (!renderer_imgui_init(renderer_state, startup_error)) {
        return fail_bootstrap(false, true, true);
    }

    const bool platform_has_viewports
        = (io.BackendFlags & ImGuiBackendFlags_PlatformHasViewports) != 0;
    const bool renderer_has_viewports
        = (io.BackendFlags & ImGuiBackendFlags_RendererHasViewports) != 0;
    const int selected_glfw_platform = platform_glfw_selected_platform();
    if (verbose_logging) {
        print("imiv: GLFW selected platform={} imgui_viewports platform={} "
              "renderer={}\n",
              platform_glfw_name(selected_glfw_platform),
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
        print("imiv: bootstrap initialized (backend: {})\n",
              backend_runtime_name(active_backend));
        print("imiv: developer mode: {}\n",
              developer_ui.enabled ? "enabled" : "disabled");
        for (const BackendRuntimeInfo& info : runtime_backend_info()) {
            print("imiv: backend {} ({}) compiled={} available={} "
                  "build_default={} platform_default={}{}\n",
                  info.build_info.display_name, info.build_info.cli_name,
                  info.build_info.compiled ? "yes" : "no",
                  info.available ? "yes" : "no",
                  info.build_info.active_build ? "yes" : "no",
                  info.build_info.platform_default ? "yes" : "no",
                  (!info.available && !info.unavailable_reason.empty())
                      ? Strutil::fmt::format(" reason='{}'",
                                             info.unavailable_reason)
                      : std::string());
        }
        print("imiv: startup queue has {} image path(s)\n",
              run_config.input_paths.size());
        print("imiv: native file dialogs: {}\n",
              FileDialog::available() ? "enabled" : "disabled");
        print("imiv: requested display format: {}\n",
              display_format_cli_name(requested_display_format));
    }

#if !defined(IMGUI_ENABLE_TEST_ENGINE)
    if (want_test_engine) {
        print(stderr,
              "imiv: IMIV_IMGUI_TEST_ENGINE requested but support is not "
              "compiled in. Configure with "
              "-DOIIO_IMIV_ENABLE_IMGUI_TEST_ENGINE=ON.\n");
    }
#endif

    if (run_config.open_dialog) {
        FileDialog::DialogReply reply = FileDialog::open_image_files("");
        if (reply.result == FileDialog::Result::Okay)
            print("imiv: open dialog selected {} path(s)\n",
                  reply.paths.empty() ? 0 : reply.paths.size());
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
    OIIO::attribute("imagebuf:use_imagecache", 1);
    if (std::shared_ptr<ImageCache> imagecache = ImageCache::create(true))
        imagecache->attribute("unassociatedalpha",
                              run_config.no_autopremult ? 1 : 0);
    reset_view_recipe(viewer.recipe);
    if (!run_config.ocio_display.empty())
        viewer.recipe.ocio_display = run_config.ocio_display;
    if (!run_config.ocio_view.empty())
        viewer.recipe.ocio_view = run_config.ocio_view;
    if (!run_config.ocio_image_color_space.empty())
        viewer.recipe.ocio_image_color_space = run_config.ocio_image_color_space;
    if (!run_config.ocio_display.empty() || !run_config.ocio_view.empty()
        || !run_config.ocio_image_color_space.empty()) {
        viewer.recipe.use_ocio = true;
    }
    clamp_view_recipe(viewer.recipe);
    apply_view_recipe_to_ui_state(viewer.recipe, ui_state);
    clamp_placeholder_ui_state(ui_state);
    if (viewer.recipe.use_ocio) {
        std::string ocio_preflight_error;
        bool ocio_preflight_ok = false;
        switch (active_backend) {
        case BackendKind::Vulkan:
            ocio_preflight_ok
                = preflight_ocio_runtime_shader(ui_state, nullptr,
                                                ocio_preflight_error);
            break;
        case BackendKind::Metal:
            ocio_preflight_ok
                = preflight_ocio_runtime_shader_metal(ui_state, nullptr,
                                                      ocio_preflight_error);
            break;
        case BackendKind::OpenGL:
            ocio_preflight_ok
                = preflight_ocio_runtime_shader_glsl(ui_state, nullptr,
                                                     ocio_preflight_error);
            break;
        case BackendKind::Auto:
            ocio_preflight_error
                = "OCIO preview is not implemented on this renderer";
            ocio_preflight_ok = false;
            break;
        }
        if (!ocio_preflight_ok) {
            viewer.recipe.use_ocio = false;
            apply_view_recipe_to_ui_state(viewer.recipe, ui_state);
            if (verbose_logging) {
                print("imiv: OCIO preflight unavailable, using standard "
                      "preview fallback: {}\n",
                      ocio_preflight_error);
            }
        }
    }
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
    if (env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_SHOW_AREA")) {
        ui_state.show_area_probe_window = true;
        ui_state.mouse_mode             = 3;
    }
    set_area_sample_enabled(viewer, ui_state, ui_state.show_area_probe_window);
    apply_imgui_app_style(sanitize_app_style_preset(ui_state.style_preset));
    apply_glfw_topmost_state_to_platform_windows(window,
                                                 ui_state.window_always_on_top);

    NativeDialogWindowScope native_dialog_scope = { window, &ui_state };
    FileDialog::set_native_dialog_scope_hook(native_dialog_scope_callback,
                                             &native_dialog_scope);

    if (!run_config.input_paths.empty()) {
        append_loaded_image_paths(library, run_config.input_paths);
        sync_viewer_library_state(viewer, library);
        if (!load_viewer_image(renderer_state, viewer, library, &ui_state,
                               run_config.input_paths[0],
                               ui_state.subimage_index,
                               ui_state.miplevel_index)) {
            print(stderr, "imiv: startup load failed for '{}'\n",
                  run_config.input_paths[0]);
        }
    } else {
        viewer.status_message = "Open an image to start preview";
    }

#if defined(IMIV_BACKEND_VULKAN_GLFW) || defined(IMIV_BACKEND_METAL_GLFW) \
    || defined(IMIV_BACKEND_OPENGL_GLFW)
    install_drag_drop(window, viewer);
#endif

#if defined(IMGUI_ENABLE_TEST_ENGINE)
    ViewerStateJsonWriteContext test_engine_state_ctx
        = { &viewer, &workspace, &ui_state, active_backend };
    TestEngineHooks test_engine_hooks;
    test_engine_hooks.image_window_title       = k_image_window_title;
    test_engine_hooks.screen_capture           = renderer_screen_capture;
    test_engine_hooks.screen_capture_user_data = &renderer_state;
    test_engine_hooks.write_viewer_state_json
        = write_test_engine_viewer_state_json;
    test_engine_hooks.write_viewer_state_user_data = &test_engine_state_ctx;
    test_engine_start(test_engine_runtime, test_engine_cfg, test_engine_hooks);
#endif

    platform_glfw_show_window(window);
    platform_glfw_poll_events();
    force_center_glfw_window(window);

    auto save_combined_settings = [&](std::string& save_error_message) {
        size_t imgui_ini_size      = 0;
        const char* imgui_ini_text = ImGui::SaveIniSettingsToMemory(
            &imgui_ini_size);
        io.WantSaveIniSettings = false;
        return save_persistent_state(ui_state, viewer, library, imgui_ini_text,
                                     imgui_ini_size, save_error_message);
    };

    bool request_exit         = false;
    int applied_style_preset  = ui_state.style_preset;
    int startup_center_frames = 90;
    while (!platform_glfw_should_close(window)) {
        platform_glfw_poll_events();
        if (startup_center_frames > 0) {
            center_glfw_window(window);
            --startup_center_frames;
        }
        if (native_dialog_scope.suspend_depth == 0) {
            apply_glfw_topmost_state_to_platform_windows(
                window, ui_state.window_always_on_top);
        }

        int fb_width  = 0;
        int fb_height = 0;
        platform_glfw_get_framebuffer_size(window, fb_width, fb_height);
        if (fb_width > 0 && fb_height > 0
            && renderer_needs_main_window_resize(renderer_state, fb_width,
                                                 fb_height)) {
            renderer_resize_main_window(renderer_state, fb_width, fb_height);
        }
        if (platform_glfw_is_iconified(window)) {
            platform_glfw_sleep(10);
            continue;
        }

        renderer_imgui_new_frame(renderer_state);
        platform_glfw_imgui_new_frame();
        ImGui::NewFrame();
        draw_viewer_ui(workspace, library, ui_state, developer_ui, fonts,
                       request_exit
#if defined(IMGUI_ENABLE_TEST_ENGINE)
                       ,
                       test_engine_show_windows_ptr(test_engine_runtime)
#endif
                           ,
                       window, renderer_state);
        if (ui_state.style_preset != applied_style_preset) {
            ui_state.style_preset = static_cast<int>(
                sanitize_app_style_preset(ui_state.style_preset));
            apply_imgui_app_style(
                sanitize_app_style_preset(ui_state.style_preset));
            applied_style_preset = ui_state.style_preset;
        }
#if defined(IMGUI_ENABLE_TEST_ENGINE)
        test_engine_maybe_show_windows(test_engine_runtime, test_engine_cfg);
#endif

        ImGui::Render();
        ImDrawData* draw_data        = ImGui::GetDrawData();
        const bool main_is_minimized = (draw_data->DisplaySize.x <= 0.0f
                                        || draw_data->DisplaySize.y <= 0.0f);
        renderer_set_main_clear_color(renderer_state, 0.08f, 0.08f, 0.08f,
                                      1.0f);
        if (!main_is_minimized)
            renderer_frame_render(renderer_state, draw_data);
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            renderer_prepare_platform_windows(renderer_state);
            ImGui::UpdatePlatformWindows();
            if (native_dialog_scope.suspend_depth == 0) {
                apply_glfw_topmost_state_to_platform_windows(
                    window, ui_state.window_always_on_top);
            }
            ImGui::RenderPlatformWindowsDefault();
            renderer_finish_platform_windows(renderer_state);
        }
        ImageViewWindow* active_view_window = active_image_view(workspace);
        process_developer_post_render_actions(developer_ui,
                                              active_view_window != nullptr
                                                  ? active_view_window->viewer
                                                  : viewer,
                                              renderer_state);
#if defined(IMGUI_ENABLE_TEST_ENGINE)
        test_engine_post_swap(test_engine_runtime);
#endif
        if (!main_is_minimized)
            renderer_frame_present(renderer_state);

#if defined(IMGUI_ENABLE_TEST_ENGINE)
        if (test_engine_should_close(test_engine_runtime, test_engine_cfg))
            platform_glfw_request_close(window);
#endif
        if (io.WantSaveIniSettings) {
            std::string save_error_message;
            if (!save_combined_settings(save_error_message)) {
                print(stderr, "imiv: failed to save preferences: {}\n",
                      save_error_message);
            }
        }
        if (request_exit)
            platform_glfw_request_close(window);
    }

    std::string prefs_save_error;
    if (!save_combined_settings(prefs_save_error))
        print(stderr, "imiv: failed to save preferences: {}\n",
              prefs_save_error);

    if (!renderer_wait_idle(renderer_state, prefs_save_error)
        && !prefs_save_error.empty())
        print(stderr, "imiv: failed to wait for renderer idle: {}\n",
              prefs_save_error);

    for (const std::unique_ptr<ImageViewWindow>& view :
         workspace.view_windows) {
        if (view == nullptr)
            continue;
        renderer_destroy_texture(renderer_state, view->viewer.texture);
    }
    if (!renderer_wait_idle(renderer_state, prefs_save_error)
        && !prefs_save_error.empty())
        print(stderr, "imiv: failed to finalize renderer idle: {}\n",
              prefs_save_error);
#if defined(IMGUI_ENABLE_TEST_ENGINE)
    test_engine_stop(test_engine_runtime);
#endif
    FileDialog::set_native_dialog_scope_hook(nullptr, nullptr);
    uninstall_drag_drop(window);
    const auto cleanup_renderer_backend = [&] {
        renderer_cleanup_window(renderer_state);
        renderer_cleanup(renderer_state);
    };
    if (active_backend == BackendKind::OpenGL)
        cleanup_renderer_backend();
    renderer_imgui_shutdown(renderer_state);
    platform_glfw_imgui_shutdown();
    ImGui::DestroyContext();
#if defined(IMGUI_ENABLE_TEST_ENGINE)
    test_engine_destroy(test_engine_runtime);
#endif

    if (active_backend != BackendKind::OpenGL)
        cleanup_renderer_backend();
    platform_glfw_destroy_window(window);
    platform_glfw_terminate();
    return EXIT_SUCCESS;
}

}  // namespace Imiv
