// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <filesystem>
#include <string>

#include <imgui.h>

namespace Imiv {

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
    void* engine      = nullptr;
    bool request_exit = false;
    bool show_windows = false;
};

using TestEngineScreenCaptureFn = bool (*)(ImGuiID viewport_id, int x, int y,
                                           int w, int h, unsigned int* pixels,
                                           void* user_data);
using TestEngineWriteViewerStateJsonFn
    = bool (*)(const std::filesystem::path& out_path, void* user_data,
               std::string& error_message);

struct TestEngineHooks {
    const char* image_window_title                           = "Image";
    TestEngineScreenCaptureFn screen_capture                 = nullptr;
    void* screen_capture_user_data                           = nullptr;
    TestEngineWriteViewerStateJsonFn write_viewer_state_json = nullptr;
    void* write_viewer_state_user_data                       = nullptr;
};

TestEngineConfig
gather_test_engine_config();
void
test_engine_start(TestEngineRuntime& runtime, TestEngineConfig& config,
                  const TestEngineHooks& hooks);
void
test_engine_stop(TestEngineRuntime& runtime);
void
test_engine_destroy(TestEngineRuntime& runtime);
bool*
test_engine_show_windows_ptr(TestEngineRuntime& runtime);
void
test_engine_maybe_show_windows(TestEngineRuntime& runtime,
                               const TestEngineConfig& config);
void
test_engine_post_swap(TestEngineRuntime& runtime);
bool
test_engine_should_close(TestEngineRuntime& runtime,
                         const TestEngineConfig& config);

void
reset_layout_dump_synthetic_items();
void
reset_test_engine_mouse_space();
void
update_test_engine_mouse_space(const ImVec2& viewport_min,
                               const ImVec2& viewport_max,
                               const ImVec2& image_min,
                               const ImVec2& image_max);
void
register_layout_dump_synthetic_item(const char* kind, const char* label);

}  // namespace Imiv
