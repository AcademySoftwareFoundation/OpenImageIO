// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_app.h"
#include "imiv_actions.h"
#include "imiv_drag_drop.h"
#include "imiv_file_dialog.h"
#include "imiv_frame.h"
#include "imiv_menu.h"
#include "imiv_navigation.h"
#include "imiv_ocio.h"
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

namespace Imiv {

namespace {

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

        const string_view trimmed = Strutil::strip(value);
        if (trimmed.empty())
            return false;
        if (trimmed == "1")
            return true;
        if (trimmed == "0")
            return false;
        return Strutil::iequals(trimmed, "true")
               || Strutil::iequals(trimmed, "yes")
               || Strutil::iequals(trimmed, "on");
    }

    bool parse_bool_value(string_view value, bool& out_value)
    {
        const string_view trimmed = Strutil::strip(value);
        if (trimmed.empty())
            return false;
        if (trimmed == "1" || Strutil::iequals(trimmed, "true")
            || Strutil::iequals(trimmed, "yes")
            || Strutil::iequals(trimmed, "on")) {
            out_value = true;
            return true;
        }
        if (trimmed == "0" || Strutil::iequals(trimmed, "false")
            || Strutil::iequals(trimmed, "no")
            || Strutil::iequals(trimmed, "off")) {
            out_value = false;
            return true;
        }
        return false;
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
            if (parse_bool_value(env_value, parsed_value)) {
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

    std::string build_main_window_title(BackendKind active_backend)
    {
        return Strutil::fmt::format("ImIv v.{} [{}]", OIIO_VERSION_STRING,
                                    backend_cli_name(active_backend));
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
    const BackendKind requested_backend = requested_backend_for_launch(
        run_config, ui_state);

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

    const std::string window_title = build_main_window_title(active_backend);
    GLFWwindow* window = platform_glfw_create_main_window(
        active_backend, 1600, 900, window_title.c_str(), startup_error);
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
    renderer_state.verbose_logging           = verbose_logging;
    renderer_state.verbose_validation_output = verbose_validation_output;
    renderer_state.log_imgui_texture_updates = log_imgui_texture_updates;
    ImVector<const char*> instance_extensions;
    if (active_backend == BackendKind::Vulkan)
        platform_glfw_collect_vulkan_instance_extensions(instance_extensions);

    if (!renderer_setup_instance(renderer_state, instance_extensions,
                                 startup_error)) {
        print(stderr, "imiv: {}\n", startup_error);
        renderer_cleanup(renderer_state);
        ImGui::DestroyContext();
        platform_glfw_destroy_window(window);
        platform_glfw_terminate();
        return EXIT_FAILURE;
    }

    if (!renderer_create_surface(renderer_state, window, startup_error)) {
        print(stderr, "imiv: {}\n", startup_error);
        renderer_cleanup(renderer_state);
        ImGui::DestroyContext();
        platform_glfw_destroy_window(window);
        platform_glfw_terminate();
        return EXIT_FAILURE;
    }

    if (!renderer_setup_device(renderer_state, startup_error)) {
        print(stderr, "imiv: {}\n", startup_error);
        renderer_destroy_surface(renderer_state);
        renderer_cleanup(renderer_state);
        ImGui::DestroyContext();
        platform_glfw_destroy_window(window);
        platform_glfw_terminate();
        return EXIT_FAILURE;
    }

    int framebuffer_width  = 0;
    int framebuffer_height = 0;
    platform_glfw_get_framebuffer_size(window, framebuffer_width,
                                       framebuffer_height);
    if (!renderer_setup_window(renderer_state, framebuffer_width,
                               framebuffer_height, startup_error)) {
        print(stderr, "imiv: {}\n", startup_error);
        renderer_cleanup_window(renderer_state);
        renderer_cleanup(renderer_state);
        ImGui::DestroyContext();
        platform_glfw_destroy_window(window);
        platform_glfw_terminate();
        return EXIT_FAILURE;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.IniFilename       = nullptr;
    const AppFonts fonts = setup_app_fonts(verbose_logging);
    apply_imgui_app_style(AppStylePreset::ImGuiDark);
    const std::filesystem::path settings_load_path
        = persistent_state_file_path_for_load();
    std::error_code settings_load_ec;
    if (settings_load_path.filename() == "imiv.inf"
        && std::filesystem::exists(settings_load_path, settings_load_ec)
        && !settings_load_ec) {
        ImGui::LoadIniSettingsFromDisk(settings_load_path.string().c_str());
    } else {
        const std::filesystem::path legacy_imgui_path
            = legacy_imgui_ini_file_path();
        settings_load_ec.clear();
        if (std::filesystem::exists(legacy_imgui_path, settings_load_ec)
            && !settings_load_ec) {
            ImGui::LoadIniSettingsFromDisk(legacy_imgui_path.string().c_str());
        }
    }
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGuiStyle& style                 = ImGui::GetStyle();
        style.WindowRounding              = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    platform_glfw_imgui_init(window, active_backend);
    if (!renderer_imgui_init(renderer_state, startup_error)) {
        print(stderr, "imiv: {}\n", startup_error);
        platform_glfw_imgui_shutdown();
        renderer_cleanup_window(renderer_state);
        renderer_cleanup(renderer_state);
        ImGui::DestroyContext();
        platform_glfw_destroy_window(window);
        platform_glfw_terminate();
        return EXIT_FAILURE;
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
    if (!run_config.ocio_display.empty())
        ui_state.ocio_display = run_config.ocio_display;
    if (!run_config.ocio_view.empty())
        ui_state.ocio_view = run_config.ocio_view;
    if (!run_config.ocio_image_color_space.empty())
        ui_state.ocio_image_color_space = run_config.ocio_image_color_space;
    if (!run_config.ocio_display.empty() || !run_config.ocio_view.empty()
        || !run_config.ocio_image_color_space.empty()) {
        ui_state.use_ocio = true;
    }
    reset_per_image_preview_state(ui_state);
    clamp_placeholder_ui_state(ui_state);
    if (ui_state.use_ocio) {
        std::string ocio_preflight_error;
        bool ocio_preflight_ok = false;
        switch (active_backend) {
        case BackendKind::Vulkan:
            ocio_preflight_ok = preflight_ocio_runtime_shader(
                ui_state, nullptr, ocio_preflight_error);
            break;
        case BackendKind::Metal:
            ocio_preflight_ok = preflight_ocio_runtime_shader_metal(
                ui_state, nullptr, ocio_preflight_error);
            break;
        case BackendKind::OpenGL:
            ocio_preflight_ok = preflight_ocio_runtime_shader_glsl(
                ui_state, nullptr, ocio_preflight_error);
            break;
        case BackendKind::Auto:
            ocio_preflight_error
                = "OCIO preview is not implemented on this renderer";
            ocio_preflight_ok = false;
            break;
        }
        if (!ocio_preflight_ok) {
            ui_state.use_ocio = false;
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

    if (!run_config.input_paths.empty()) {
        append_loaded_image_paths(library, run_config.input_paths);
        viewer.loaded_image_paths = library.loaded_image_paths;
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
    ViewerStateJsonWriteContext test_engine_state_ctx = { &viewer, &workspace,
                                                          &ui_state,
                                                          active_backend };
    TestEngineHooks test_engine_hooks;
    test_engine_hooks.image_window_title       = image_window_title();
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
            ImGui::RenderPlatformWindowsDefault();
            renderer_finish_platform_windows(renderer_state);
        }
        process_developer_post_render_actions(developer_ui,
                                              active_image_view(workspace)
                                                  != nullptr
                                                  ? active_image_view(workspace)
                                                        ->viewer
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

    for (const std::unique_ptr<ImageViewWindow>& view : workspace.view_windows) {
        if (view == nullptr)
            continue;
        renderer_destroy_texture(renderer_state, view->viewer.texture);
    }
#if defined(IMGUI_ENABLE_TEST_ENGINE)
    test_engine_stop(test_engine_runtime);
#endif
    uninstall_drag_drop(window);
    if (active_backend == BackendKind::OpenGL) {
        renderer_cleanup_window(renderer_state);
        renderer_cleanup(renderer_state);
    }
    renderer_imgui_shutdown(renderer_state);
    platform_glfw_imgui_shutdown();
    ImGui::DestroyContext();
#if defined(IMGUI_ENABLE_TEST_ENGINE)
    test_engine_destroy(test_engine_runtime);
#endif

    if (active_backend != BackendKind::OpenGL) {
        renderer_cleanup_window(renderer_state);
        renderer_cleanup(renderer_state);
    }
    platform_glfw_destroy_window(window);
    platform_glfw_terminate();
    return EXIT_SUCCESS;
}

}  // namespace Imiv
