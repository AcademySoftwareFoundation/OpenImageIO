// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_test_engine.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

#include <OpenImageIO/strutil.h>

#if USE_EXTERNAL_PUGIXML
#    include "pugixml.hpp"
#else
#    include <OpenImageIO/detail/pugixml/pugixml.hpp>
#endif

#if defined(IMGUI_ENABLE_TEST_ENGINE)
#    include <imgui_te_context.h>
#    include <imgui_te_engine.h>
#    include <imgui_te_exporters.h>
#    include <imgui_te_ui.h>
#endif

using namespace OIIO;

namespace Imiv {
namespace {

    bool read_env_value(const char* name, std::string& out)
    {
        out.clear();
        if (name == nullptr || name[0] == '\0')
            return false;
        const char* value = std::getenv(name);
        if (value == nullptr || value[0] == '\0')
            return false;
        out = value;
        return true;
    }

    bool parse_bool_value(const std::string& value, bool& out)
    {
        const string_view trimmed = Strutil::strip(value);
        if (trimmed == "1" || Strutil::iequals(trimmed, "true")
            || Strutil::iequals(trimmed, "yes")
            || Strutil::iequals(trimmed, "on")) {
            out = true;
            return true;
        }
        if (trimmed == "0" || Strutil::iequals(trimmed, "false")
            || Strutil::iequals(trimmed, "no")
            || Strutil::iequals(trimmed, "off")) {
            out = false;
            return true;
        }
        return false;
    }

    bool parse_int_value(const std::string& value, int& out)
    {
        const std::string trimmed = std::string(Strutil::strip(value));
        if (trimmed.empty())
            return false;
        char* end   = nullptr;
        long parsed = std::strtol(trimmed.c_str(), &end, 10);
        if (end == trimmed.c_str() || *end != '\0')
            return false;
        if (parsed < static_cast<long>(std::numeric_limits<int>::min())
            || parsed > static_cast<long>(std::numeric_limits<int>::max())) {
            return false;
        }
        out = static_cast<int>(parsed);
        return true;
    }

    bool parse_float_value(const std::string& value, float& out)
    {
        const std::string trimmed = std::string(Strutil::strip(value));
        if (trimmed.empty())
            return false;
        char* end    = nullptr;
        float parsed = std::strtof(trimmed.c_str(), &end);
        if (end == trimmed.c_str() || *end != '\0')
            return false;
        out = parsed;
        return true;
    }

    bool env_flag_is_truthy(const char* name)
    {
        std::string value;
        bool out = false;
        return read_env_value(name, value) && parse_bool_value(value, out)
               && out;
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

    int env_int_value(const char* name, int fallback)
    {
        int out = 0;
        return env_read_int_value(name, out) ? out : fallback;
    }

    enum class TestEngineMouseTargetMode {
        None,
        Absolute,
        WindowRel,
        ImageRel
    };

    struct TestEngineSyntheticAction {
        int delay_frames             = 0;
        int post_action_delay_frames = 0;
        bool has_key_chord           = false;
        ImGuiKeyChord key_chord      = 0;
        TestEngineMouseTargetMode mouse_target_mode
            = TestEngineMouseTargetMode::None;
        float mouse_x        = 0.0f;
        float mouse_y        = 0.0f;
        bool has_click       = false;
        int click_button     = 0;
        bool has_wheel       = false;
        float wheel_x        = 0.0f;
        float wheel_y        = 0.0f;
        bool has_drag        = false;
        float drag_dx        = 0.0f;
        float drag_dy        = 0.0f;
        int drag_button      = 0;
        bool has_hold_drag   = false;
        float hold_drag_dx   = 0.0f;
        float hold_drag_dy   = 0.0f;
        int hold_drag_button = 0;
        int hold_drag_frames = 1;
    };

    struct TestEngineScenarioOcioStep {
        bool has_use               = false;
        bool use_ocio              = false;
        bool has_display           = false;
        bool has_view              = false;
        bool has_image_color_space = false;
        std::string display;
        std::string view;
        std::string image_color_space;
    };

    struct TestEngineScenarioCaptureStep {
        bool screenshot   = false;
        bool layout       = false;
        bool state        = false;
        bool layout_items = false;
        int layout_depth  = 8;
    };

    struct TestEngineScenarioStep {
        std::string name;
        TestEngineSyntheticAction action;
        TestEngineScenarioOcioStep ocio;
        TestEngineScenarioCaptureStep capture;
    };

    struct TestEngineScenarioDefinition {
        std::filesystem::path out_dir;
        bool default_layout_items = false;
        int default_layout_depth  = 8;
        std::vector<TestEngineScenarioStep> steps;
    };

    bool parse_test_engine_key_chord(const std::string& value,
                                     ImGuiKeyChord& out_chord);

    bool parse_float_pair_value(string_view value, float& out_a, float& out_b)
    {
        std::vector<std::string> parts = Strutil::splits(value, ",", 2);
        if (parts.size() != 2)
            return false;
        return parse_float_value(std::string(Strutil::strip(parts[0])), out_a)
               && parse_float_value(std::string(Strutil::strip(parts[1])),
                                    out_b);
    }

    bool parse_bool_attr(const pugi::xml_attribute& attr, bool& out)
    {
        return attr && parse_bool_value(attr.as_string(), out);
    }

    bool parse_int_attr(const pugi::xml_attribute& attr, int& out)
    {
        return attr && parse_int_value(attr.as_string(), out);
    }

    bool parse_float_pair_attr(const pugi::xml_attribute& attr, float& out_a,
                               float& out_b)
    {
        return attr && parse_float_pair_value(attr.as_string(), out_a, out_b);
    }

    void set_process_env_value(const char* name, const std::string* value)
    {
        if (name == nullptr || name[0] == '\0')
            return;
#if defined(_WIN32)
        _putenv_s(name, value ? value->c_str() : "");
#else
        if (value)
            setenv(name, value->c_str(), 1);
        else
            unsetenv(name);
#endif
    }

    bool load_test_engine_scenario(const std::filesystem::path& path,
                                   TestEngineScenarioDefinition& out_scenario,
                                   std::string& error_message)
    {
        out_scenario = TestEngineScenarioDefinition {};
        error_message.clear();

        pugi::xml_document doc;
        const std::string path_string       = path.string();
        const pugi::xml_parse_result result = doc.load_file(
            path_string.c_str());
        if (!result) {
            error_message
                = Strutil::fmt::format("failed to parse scenario XML '{}': {}",
                                       path.string(), result.description());
            return false;
        }

        const pugi::xml_node root = doc.child("imiv-scenario");
        if (!root) {
            error_message = Strutil::fmt::format(
                "scenario file '{}' is missing <imiv-scenario> root",
                path.string());
            return false;
        }

        const pugi::xml_attribute out_dir_attr = root.attribute("out_dir");
        if (!out_dir_attr || out_dir_attr.as_string()[0] == '\0') {
            error_message = "scenario root requires non-empty out_dir attribute";
            return false;
        }
        out_scenario.out_dir = std::filesystem::path(out_dir_attr.as_string());

        int layout_depth = out_scenario.default_layout_depth;
        if (parse_int_attr(root.attribute("layout_depth"), layout_depth)
            && layout_depth > 0) {
            out_scenario.default_layout_depth = layout_depth;
        }
        bool layout_items = false;
        if (parse_bool_attr(root.attribute("layout_items"), layout_items))
            out_scenario.default_layout_items = layout_items;

        for (pugi::xml_node step_node = root.child("step"); step_node;
             step_node                = step_node.next_sibling("step")) {
            TestEngineScenarioStep step;
            const pugi::xml_attribute name_attr = step_node.attribute("name");
            if (!name_attr || name_attr.as_string()[0] == '\0') {
                error_message
                    = "scenario step is missing non-empty name attribute";
                return false;
            }
            step.name = name_attr.as_string();

            parse_int_attr(step_node.attribute("delay_frames"),
                           step.action.delay_frames);
            parse_int_attr(step_node.attribute("post_action_delay_frames"),
                           step.action.post_action_delay_frames);

            const pugi::xml_attribute key_chord_attr = step_node.attribute(
                "key_chord");
            if (key_chord_attr && key_chord_attr.as_string()[0] != '\0') {
                if (!parse_test_engine_key_chord(key_chord_attr.as_string(),
                                                 step.action.key_chord)) {
                    error_message = Strutil::fmt::format(
                        "scenario step '{}' has invalid key_chord '{}'",
                        step.name, key_chord_attr.as_string());
                    return false;
                }
                step.action.has_key_chord = true;
            }

            if (parse_float_pair_attr(step_node.attribute("mouse_pos"),
                                      step.action.mouse_x,
                                      step.action.mouse_y)) {
                step.action.mouse_target_mode
                    = TestEngineMouseTargetMode::Absolute;
            } else if (parse_float_pair_attr(step_node.attribute(
                                                 "mouse_pos_window_rel"),
                                             step.action.mouse_x,
                                             step.action.mouse_y)) {
                step.action.mouse_target_mode
                    = TestEngineMouseTargetMode::WindowRel;
            } else if (parse_float_pair_attr(step_node.attribute(
                                                 "mouse_pos_image_rel"),
                                             step.action.mouse_x,
                                             step.action.mouse_y)) {
                step.action.mouse_target_mode
                    = TestEngineMouseTargetMode::ImageRel;
            }

            if (parse_int_attr(step_node.attribute("mouse_click_button"),
                               step.action.click_button)) {
                step.action.has_click = true;
            }
            if (parse_float_pair_attr(step_node.attribute("mouse_wheel"),
                                      step.action.wheel_x,
                                      step.action.wheel_y)) {
                step.action.has_wheel = true;
            }
            if (parse_float_pair_attr(step_node.attribute("mouse_drag"),
                                      step.action.drag_dx,
                                      step.action.drag_dy)) {
                step.action.has_drag = true;
                parse_int_attr(step_node.attribute("mouse_drag_button"),
                               step.action.drag_button);
            }
            if (parse_float_pair_attr(step_node.attribute("mouse_drag_hold"),
                                      step.action.hold_drag_dx,
                                      step.action.hold_drag_dy)) {
                step.action.has_hold_drag = true;
                parse_int_attr(step_node.attribute("mouse_drag_hold_button"),
                               step.action.hold_drag_button);
                parse_int_attr(step_node.attribute("mouse_drag_hold_frames"),
                               step.action.hold_drag_frames);
            }

            if (parse_bool_attr(step_node.attribute("ocio_use"),
                                step.ocio.use_ocio)) {
                step.ocio.has_use = true;
            }
            const pugi::xml_attribute ocio_display_attr = step_node.attribute(
                "ocio_display");
            if (ocio_display_attr && ocio_display_attr.as_string()[0] != '\0') {
                step.ocio.has_display = true;
                step.ocio.display     = ocio_display_attr.as_string();
            }
            const pugi::xml_attribute ocio_view_attr = step_node.attribute(
                "ocio_view");
            if (ocio_view_attr && ocio_view_attr.as_string()[0] != '\0') {
                step.ocio.has_view = true;
                step.ocio.view     = ocio_view_attr.as_string();
            }
            const pugi::xml_attribute ocio_cs_attr = step_node.attribute(
                "ocio_image_color_space");
            if (ocio_cs_attr && ocio_cs_attr.as_string()[0] != '\0') {
                step.ocio.has_image_color_space = true;
                step.ocio.image_color_space     = ocio_cs_attr.as_string();
            }

            parse_bool_attr(step_node.attribute("screenshot"),
                            step.capture.screenshot);
            parse_bool_attr(step_node.attribute("layout"), step.capture.layout);
            parse_bool_attr(step_node.attribute("state"), step.capture.state);
            step.capture.layout_items  = out_scenario.default_layout_items;
            step.capture.layout_depth  = out_scenario.default_layout_depth;
            bool layout_items_override = false;
            if (parse_bool_attr(step_node.attribute("layout_items"),
                                layout_items_override)) {
                step.capture.layout_items = layout_items_override;
            }
            int layout_depth_override = step.capture.layout_depth;
            if (parse_int_attr(step_node.attribute("layout_depth"),
                               layout_depth_override)
                && layout_depth_override > 0) {
                step.capture.layout_depth = layout_depth_override;
            }

            out_scenario.steps.emplace_back(std::move(step));
        }

        if (out_scenario.steps.empty()) {
            error_message = "scenario file does not contain any <step> entries";
            return false;
        }
        return true;
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
        if (token.size() >= 2 && token[0] == 'f') {
            const char* digits = token.c_str() + 1;
            char* end          = nullptr;
            const long index   = std::strtol(digits, &end, 10);
            if (end != digits && *end == '\0' && index >= 1 && index <= 12)
                return static_cast<ImGuiKey>(ImGuiKey_F1 + (index - 1));
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
        const std::string trimmed = std::string(Strutil::strip(value));
        if (trimmed.empty())
            return false;

        ImGuiKeyChord chord = 0;
        bool have_key       = false;
        size_t begin        = 0;
        while (begin <= trimmed.size()) {
            const size_t plus = trimmed.find('+', begin);
            const size_t end  = (plus == std::string::npos) ? trimmed.size()
                                                            : plus;
            const std::string token = Strutil::lower(
                Strutil::strip(trimmed.substr(begin, end - begin)));
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

    bool env_read_key_chord_value(const char* name, ImGuiKeyChord& out)
    {
        std::string value;
        return read_env_value(name, value)
               && parse_test_engine_key_chord(value, out);
    }

    bool validate_test_output_path(const std::filesystem::path& path,
                                   std::string& error_message)
    {
        error_message.clear();
        if (path.empty()) {
            error_message = "output path is empty";
            return false;
        }
#if defined(NDEBUG)
        if (path.is_absolute()) {
            error_message
                = "absolute output paths are disabled in release builds";
            return false;
        }
#endif
        return true;
    }

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
    TestEngineHooks g_test_engine_hooks;

    bool layout_dump_items_enabled()
    {
        return env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP_ITEMS");
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
            const unsigned char c = *p;
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
                                bool include_items, int depth,
                                const std::vector<ImGuiWindow*>* extra_windows
                                = nullptr)
    {
        if (depth <= 0)
            depth = 1;

        std::string error_message;
        if (!validate_test_output_path(out_path, error_message)) {
            ctx->LogError("layout dump: %s", error_message.c_str());
            mark_test_error(ctx);
            return false;
        }

        std::error_code ec;
        if (!out_path.parent_path().empty())
            std::filesystem::create_directories(out_path.parent_path(), ec);

        FILE* f = nullptr;
#    if defined(_WIN32)
        if (fopen_s(&f, out_path.string().c_str(), "wb") != 0)
            f = nullptr;
#    else
        f = std::fopen(out_path.string().c_str(), "wb");
#    endif
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
        const char* image_window_title
            = g_test_engine_hooks.image_window_title
                  ? g_test_engine_hooks.image_window_title
                  : "Image";
        const char* window_names[] = {
            "##MainMenuBar",
            image_window_title,
            "iv Info",
            "iv Preferences",
            "iv Preview",
            "Dear ImGui Demo",
            "Dear ImGui Style Editor",
            "Dear ImGui Metrics/Debugger",
            "Dear ImGui Debug Log",
            "Dear ImGui ID Stack Tool",
            "About Dear ImGui",
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
        if (extra_windows != nullptr) {
            for (ImGuiWindow* extra_window : *extra_windows) {
                if (extra_window == nullptr)
                    continue;
                bool duplicate = false;
                for (const WindowDumpEntry& existing : windows) {
                    if (existing.info.Window == extra_window) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate)
                    continue;
                windows.push_back({ ctx->ItemInfo(extra_window->ID,
                                                  ImGuiTestOpFlags_NoError) });
            }
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
        std::string error_message;
        if (!validate_test_output_path(out_path, error_message)) {
            ctx->LogError("state dump: %s", error_message.c_str());
            mark_test_error(ctx);
            return false;
        }
        if (g_test_engine_hooks.write_viewer_state_json == nullptr) {
            ctx->LogError("state dump: viewer state callback is unavailable");
            mark_test_error(ctx);
            return false;
        }
        error_message.clear();
        if (!g_test_engine_hooks.write_viewer_state_json(
                out_path, g_test_engine_hooks.write_viewer_state_user_data,
                error_message)) {
            ctx->LogError("state dump: %s", error_message.empty()
                                                ? "viewer state dump failed"
                                                : error_message.c_str());
            mark_test_error(ctx);
            return false;
        }
        ctx->LogInfo("state dump: wrote %s", out_path.string().c_str());
        return true;
    }

    bool capture_main_viewport_screenshot(ImGuiTestContext* ctx,
                                          const char* out_file)
    {
        ctx->CaptureReset();
        if (out_file && out_file[0] != '\0') {
            std::string error_message;
            if (!validate_test_output_path(std::filesystem::path(out_file),
                                           error_message)) {
                ctx->LogError("screenshot: %s", error_message.c_str());
                mark_test_error(ctx);
                return false;
            }
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
        return true;
    }

    std::string sanitize_test_output_stem(const std::string& name)
    {
        std::string sanitized = name;
        for (char& c : sanitized) {
            const bool keep = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                              || (c >= '0' && c <= '9') || c == '_' || c == '-'
                              || c == '.';
            if (!keep)
                c = '_';
        }
        if (sanitized.empty())
            sanitized = "step";
        return sanitized;
    }

    void apply_test_engine_scenario_ocio_overrides(
        const TestEngineScenarioOcioStep& ocio)
    {
        if (ocio.has_use) {
            const std::string use_value = ocio.use_ocio ? "true" : "false";
            set_process_env_value("IMIV_IMGUI_TEST_ENGINE_OCIO_USE",
                                  &use_value);
        }
        if (ocio.has_display)
            set_process_env_value("IMIV_IMGUI_TEST_ENGINE_OCIO_DISPLAY",
                                  &ocio.display);
        if (ocio.has_view)
            set_process_env_value("IMIV_IMGUI_TEST_ENGINE_OCIO_VIEW",
                                  &ocio.view);
        if (ocio.has_image_color_space) {
            set_process_env_value(
                "IMIV_IMGUI_TEST_ENGINE_OCIO_IMAGE_COLOR_SPACE",
                &ocio.image_color_space);
        }
    }

    bool write_test_engine_scenario_step_outputs(
        ImGuiTestContext* ctx, const TestEngineScenarioDefinition& scenario,
        const TestEngineScenarioStep& step)
    {
        const std::string stem = sanitize_test_output_stem(step.name);
        const std::filesystem::path base_path = scenario.out_dir / stem;

        if (step.capture.screenshot) {
            const std::filesystem::path screenshot_path = base_path.string()
                                                          + ".png";
            const std::string screenshot_string = screenshot_path.string();
            if (!capture_main_viewport_screenshot(ctx,
                                                  screenshot_string.c_str())) {
                return false;
            }
        }

        if (step.capture.layout) {
            const std::filesystem::path layout_path = base_path.string()
                                                      + ".layout.json";
            if (!write_layout_dump_json(ctx, layout_path,
                                        step.capture.layout_items,
                                        step.capture.layout_depth)) {
                return false;
            }
        }

        if (step.capture.state) {
            const std::filesystem::path state_path = base_path.string()
                                                     + ".state.json";
            if (!write_viewer_state_json(ctx, state_path))
                return false;
        }

        return true;
    }

    bool resolve_action_mouse_pos(const TestEngineSyntheticAction& action,
                                  ImVec2& out_pos)
    {
        out_pos = ImVec2(0.0f, 0.0f);
        switch (action.mouse_target_mode) {
        case TestEngineMouseTargetMode::Absolute:
            out_pos = ImVec2(action.mouse_x, action.mouse_y);
            return true;
        case TestEngineMouseTargetMode::WindowRel:
            if (g_test_engine_mouse_space.viewport_valid) {
                out_pos = test_engine_rect_rel_pos(
                    g_test_engine_mouse_space.viewport_min,
                    g_test_engine_mouse_space.viewport_max, action.mouse_x,
                    action.mouse_y);
                return true;
            }
            return false;
        case TestEngineMouseTargetMode::ImageRel:
            if (g_test_engine_mouse_space.image_valid) {
                out_pos = test_engine_rect_rel_pos(
                    g_test_engine_mouse_space.image_min,
                    g_test_engine_mouse_space.image_max, action.mouse_x,
                    action.mouse_y);
                return true;
            }
            return false;
        case TestEngineMouseTargetMode::None:
        default: return false;
        }
    }

    int
    apply_test_engine_synthetic_actions(ImGuiTestContext* ctx,
                                        const TestEngineSyntheticAction& action)
    {
        int held_button = -1;
        if (action.has_key_chord) {
            ctx->KeyPress(action.key_chord);
            ctx->Yield(1);
        }

        ImVec2 mouse_pos(0.0f, 0.0f);
        if (resolve_action_mouse_pos(action, mouse_pos)) {
            ctx->MouseMoveToPos(mouse_pos);
            ctx->Yield(1);
        }

        if (action.has_click) {
            const int click_button = std::clamp(action.click_button, 0, 4);
            ctx->MouseClick(static_cast<ImGuiMouseButton>(click_button));
            ctx->Yield(1);
        }

        if (action.has_wheel) {
            ctx->MouseWheel(ImVec2(action.wheel_x, action.wheel_y));
            ctx->Yield(1);
        }

        if (action.has_drag) {
            const int drag_button    = std::clamp(action.drag_button, 0, 4);
            const ImVec2 current_pos = ImGui::GetIO().MousePos;
            ctx->MouseDown(static_cast<ImGuiMouseButton>(drag_button));
            ctx->Yield(1);
            ctx->MouseMoveToPos(ImVec2(current_pos.x + action.drag_dx,
                                       current_pos.y + action.drag_dy));
            ctx->Yield(1);
            ctx->MouseUp(static_cast<ImGuiMouseButton>(drag_button));
            ctx->Yield(1);
        }
        if (action.has_hold_drag) {
            const int drag_button = std::clamp(action.hold_drag_button, 0, 4);
            const int hold_frames = std::max(0, action.hold_drag_frames);
            ctx->MouseDown(static_cast<ImGuiMouseButton>(drag_button));
            ctx->Yield(1);
            const ImVec2 current_pos = ImGui::GetIO().MousePos;
            ctx->MouseMoveToPos(ImVec2(current_pos.x + action.hold_drag_dx,
                                       current_pos.y + action.hold_drag_dy));
            if (hold_frames > 0)
                ctx->Yield(hold_frames);
            held_button = drag_button;
        }
        return held_button;
    }

    int apply_test_engine_mouse_actions(ImGuiTestContext* ctx)
    {
        TestEngineSyntheticAction action;
        ImGuiKeyChord key_chord = 0;
        if (env_read_key_chord_value("IMIV_IMGUI_TEST_ENGINE_KEY_CHORD",
                                     key_chord)) {
            action.has_key_chord = true;
            action.key_chord     = key_chord;
        }
        if (env_read_float_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_IMAGE_REL_X",
                                 action.mouse_x)
            && env_read_float_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_IMAGE_REL_Y",
                                    action.mouse_y)) {
            action.mouse_target_mode = TestEngineMouseTargetMode::ImageRel;
        } else if (env_read_float_value(
                       "IMIV_IMGUI_TEST_ENGINE_MOUSE_WINDOW_REL_X",
                       action.mouse_x)
                   && env_read_float_value(
                       "IMIV_IMGUI_TEST_ENGINE_MOUSE_WINDOW_REL_Y",
                       action.mouse_y)) {
            action.mouse_target_mode = TestEngineMouseTargetMode::WindowRel;
        } else if (env_read_float_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_X",
                                        action.mouse_x)
                   && env_read_float_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_Y",
                                           action.mouse_y)) {
            action.mouse_target_mode = TestEngineMouseTargetMode::Absolute;
        }
        if (env_read_int_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_CLICK_BUTTON",
                               action.click_button)) {
            action.has_click = true;
        }
        if (env_read_float_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_WHEEL_X",
                                 action.wheel_x)
            || env_read_float_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_WHEEL_Y",
                                    action.wheel_y)) {
            action.has_wheel = true;
        }
        if (env_read_float_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_DRAG_DX",
                                 action.drag_dx)
            || env_read_float_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_DRAG_DY",
                                    action.drag_dy)) {
            action.has_drag = true;
            env_read_int_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_DRAG_BUTTON",
                               action.drag_button);
        }
        if (env_read_float_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_HOLD_DRAG_DX",
                                 action.hold_drag_dx)
            || env_read_float_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_HOLD_DRAG_DY",
                                    action.hold_drag_dy)) {
            action.has_hold_drag = true;
            env_read_int_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_HOLD_DRAG_BUTTON",
                               action.hold_drag_button);
            action.hold_drag_frames
                = env_int_value("IMIV_IMGUI_TEST_ENGINE_MOUSE_HOLD_FRAMES", 1);
        }
        return apply_test_engine_synthetic_actions(ctx, action);
    }

    void release_test_engine_held_mouse(ImGuiTestContext* ctx, int held_button)
    {
        if (held_button < 0)
            return;
        ctx->MouseUp(static_cast<ImGuiMouseButton>(held_button));
        ctx->Yield(1);
    }

    void imiv_test_smoke_screenshot(ImGuiTestContext* ctx)
    {
        const int delay_frames = env_int_value(
            "IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_DELAY_FRAMES", 3);
        ctx->Yield(delay_frames);
        const int held_button = apply_test_engine_mouse_actions(ctx);
        const int post_action_delay
            = env_int_value("IMIV_IMGUI_TEST_ENGINE_POST_ACTION_DELAY_FRAMES",
                            0);
        if (post_action_delay > 0)
            ctx->Yield(post_action_delay);

        const int frames_to_capture
            = env_int_value("IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_FRAMES", 1);
        const bool save_all = env_flag_is_truthy(
            "IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_SAVE_ALL");

        std::string out_value;
        const bool has_out
            = read_env_value("IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_OUT",
                             out_value)
              && !out_value.empty();
        const char* out = has_out ? out_value.c_str() : nullptr;

        const bool capture_last_only = (!save_all && frames_to_capture > 1);
        for (int i = 0; i < frames_to_capture; ++i) {
            if (capture_last_only && i + 1 != frames_to_capture) {
                ctx->Yield(1);
                continue;
            }
            if (out) {
                if (save_all && frames_to_capture > 1) {
                    const char* dot     = std::strrchr(out, '.');
                    char out_file[2048] = {};
                    if (dot && dot != out) {
                        const size_t base_len = static_cast<size_t>(dot - out);
                        std::snprintf(out_file, sizeof(out_file),
                                      "%.*s_f%03d%s",
                                      static_cast<int>(base_len), out, i, dot);
                    } else {
                        std::snprintf(out_file, sizeof(out_file),
                                      "%s_f%03d.png", out, i);
                    }
                    if (!capture_main_viewport_screenshot(ctx, out_file))
                        return;
                } else {
                    if (!capture_main_viewport_screenshot(ctx, out))
                        return;
                }
            } else {
                if (!capture_main_viewport_screenshot(ctx, nullptr))
                    return;
            }
            std::string layout_out_value;
            if (read_env_value(
                    "IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_LAYOUT_OUT",
                    layout_out_value)
                && !layout_out_value.empty()) {
                const bool include_items = env_flag_is_truthy(
                    "IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_LAYOUT_ITEMS");
                int depth = env_int_value(
                    "IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_LAYOUT_DEPTH", 8);
                if (depth <= 0)
                    depth = 1;
                if (!write_layout_dump_json(
                        ctx, std::filesystem::path(layout_out_value),
                        include_items, depth)) {
                    release_test_engine_held_mouse(ctx, held_button);
                    return;
                }
            }
            std::string state_out_value;
            if (read_env_value(
                    "IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT_STATE_OUT",
                    state_out_value)
                && !state_out_value.empty()) {
                if (!write_viewer_state_json(ctx, std::filesystem::path(
                                                      state_out_value))) {
                    release_test_engine_held_mouse(ctx, held_button);
                    return;
                }
            }
            ctx->Yield(1);
        }
        release_test_engine_held_mouse(ctx, held_button);
    }

    void imiv_test_dump_layout_json(ImGuiTestContext* ctx)
    {
        int delay_frames
            = env_int_value("IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP_DELAY_FRAMES",
                            3);
        ctx->Yield(delay_frames);
        const int held_button = apply_test_engine_mouse_actions(ctx);
        const int post_action_delay
            = env_int_value("IMIV_IMGUI_TEST_ENGINE_POST_ACTION_DELAY_FRAMES",
                            0);
        if (post_action_delay > 0)
            ctx->Yield(post_action_delay);

        const bool include_items = env_flag_is_truthy(
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
            release_test_engine_held_mouse(ctx, held_button);
            return;
        }
        release_test_engine_held_mouse(ctx, held_button);
    }

    void imiv_test_dump_viewer_state(ImGuiTestContext* ctx)
    {
        const int delay_frames
            = env_int_value("IMIV_IMGUI_TEST_ENGINE_STATE_DUMP_DELAY_FRAMES",
                            3);
        ctx->Yield(delay_frames);
        const int held_button = apply_test_engine_mouse_actions(ctx);
        const int post_action_delay
            = env_int_value("IMIV_IMGUI_TEST_ENGINE_POST_ACTION_DELAY_FRAMES",
                            0);
        if (post_action_delay > 0)
            ctx->Yield(post_action_delay);
        ctx->Yield(1);

        std::string out_value;
        if (!read_env_value("IMIV_IMGUI_TEST_ENGINE_STATE_DUMP_OUT", out_value)
            || out_value.empty()) {
            out_value = "viewer_state.json";
        }

        if (!write_viewer_state_json(ctx, std::filesystem::path(out_value))) {
            release_test_engine_held_mouse(ctx, held_button);
            return;
        }
        release_test_engine_held_mouse(ctx, held_button);
    }

    void imiv_test_run_scenario(ImGuiTestContext* ctx)
    {
        std::string scenario_file_value;
        if (!read_env_value("IMIV_IMGUI_TEST_ENGINE_SCENARIO_FILE",
                            scenario_file_value)
            || scenario_file_value.empty()) {
            ctx->LogError(
                "scenario: IMIV_IMGUI_TEST_ENGINE_SCENARIO_FILE is not set");
            mark_test_error(ctx);
            return;
        }

        TestEngineScenarioDefinition scenario;
        std::string error_message;
        if (!load_test_engine_scenario(std::filesystem::path(
                                           scenario_file_value),
                                       scenario, error_message)) {
            ctx->LogError("scenario: %s", error_message.c_str());
            mark_test_error(ctx);
            return;
        }

        std::error_code ec;
        std::filesystem::create_directories(scenario.out_dir, ec);
        if (ec) {
            ctx->LogError("scenario: failed to create output directory '%s': %s",
                          scenario.out_dir.string().c_str(),
                          ec.message().c_str());
            mark_test_error(ctx);
            return;
        }

        int held_button = -1;
        for (const TestEngineScenarioStep& step : scenario.steps) {
            release_test_engine_held_mouse(ctx, held_button);
            held_button = -1;

            ctx->LogInfo("scenario: step '%s'", step.name.c_str());

            if (step.action.delay_frames > 0)
                ctx->Yield(step.action.delay_frames);

            apply_test_engine_scenario_ocio_overrides(step.ocio);
            ctx->Yield(1);

            held_button = apply_test_engine_synthetic_actions(ctx, step.action);

            if (step.action.post_action_delay_frames > 0)
                ctx->Yield(step.action.post_action_delay_frames);

            if (step.capture.screenshot || step.capture.layout
                || step.capture.state) {
                if (!write_test_engine_scenario_step_outputs(ctx, scenario,
                                                             step)) {
                    release_test_engine_held_mouse(ctx, held_button);
                    return;
                }
            }

            release_test_engine_held_mouse(ctx, held_button);
            held_button = -1;
            ctx->Yield(1);
        }
    }

    void imiv_test_developer_menu_metrics(ImGuiTestContext* ctx)
    {
#    if defined(NDEBUG)
        ctx->LogInfo(
            "developer menu regression skipped: not available in release build");
        return;
#    else
        ctx->Yield(3);
        const ImGuiTestItemInfo developer_menu
            = ctx->ItemInfo("##MainMenuBar##MenuBar/Developer",
                            ImGuiTestOpFlags_NoError);
        if (developer_menu.ID == 0 || developer_menu.Window == nullptr) {
            ctx->LogError(
                "developer menu regression: Developer menu item not found");
            mark_test_error(ctx);
            return;
        }

        ctx->LogInfo("developer menu regression: opening Developer menu");
        ctx->ItemClick("##MainMenuBar##MenuBar/Developer");
        ctx->Yield(1);
        ctx->LogInfo("developer menu regression: clicking ImGui Demo");
        ctx->ItemClick("//$FOCUSED/ImGui Demo");
        ctx->Yield(2);

        const ImGuiTestItemInfo demo_window
            = ctx->WindowInfo("Dear ImGui Demo", ImGuiTestOpFlags_NoError);
        if (demo_window.ID == 0 || demo_window.Window == nullptr) {
            ctx->LogError("developer menu regression: demo window did not "
                          "open");
            mark_test_error(ctx);
            return;
        }

        std::string out_value;
        if (!read_env_value("IMIV_IMGUI_TEST_ENGINE_DEVELOPER_MENU_LAYOUT_OUT",
                            out_value)
            || out_value.empty()) {
            out_value = "developer_menu_metrics_layout.json";
        }

        int depth = env_int_value(
            "IMIV_IMGUI_TEST_ENGINE_DEVELOPER_MENU_LAYOUT_DEPTH", 8);
        if (depth <= 0)
            depth = 1;
        const bool include_items = env_flag_is_truthy(
            "IMIV_IMGUI_TEST_ENGINE_DEVELOPER_MENU_LAYOUT_ITEMS");
        std::vector<ImGuiWindow*> extra_windows = { demo_window.Window };
        if (!write_layout_dump_json(ctx, std::filesystem::path(out_value),
                                    include_items, depth, &extra_windows)) {
            return;
        }
#    endif
    }

    ImGuiTest* register_imiv_smoke_tests(ImGuiTestEngine* engine)
    {
        ImGuiTest* t = IM_REGISTER_TEST(engine, "imiv", "smoke_screenshot");
        t->TestFunc  = imiv_test_smoke_screenshot;
        return t;
    }

    ImGuiTest* register_imiv_layout_dump_tests(ImGuiTestEngine* engine)
    {
        ImGuiTest* t = IM_REGISTER_TEST(engine, "imiv", "dump_layout_json");
        t->TestFunc  = imiv_test_dump_layout_json;
        return t;
    }

    ImGuiTest* register_imiv_state_dump_tests(ImGuiTestEngine* engine)
    {
        ImGuiTest* t = IM_REGISTER_TEST(engine, "imiv", "dump_viewer_state");
        t->TestFunc  = imiv_test_dump_viewer_state;
        return t;
    }

    ImGuiTest* register_imiv_developer_menu_tests(ImGuiTestEngine* engine)
    {
        ImGuiTest* t = IM_REGISTER_TEST(engine, "imiv",
                                        "developer_menu_metrics_window");
        t->TestFunc  = imiv_test_developer_menu_metrics;
        return t;
    }

    ImGuiTest* register_imiv_scenario_tests(ImGuiTestEngine* engine)
    {
        ImGuiTest* t = IM_REGISTER_TEST(engine, "imiv", "scenario");
        t->TestFunc  = imiv_test_run_scenario;
        return t;
    }
#endif

}  // namespace

TestEngineConfig
gather_test_engine_config()
{
    TestEngineConfig cfg;
    cfg.trace = env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_TRACE");
    cfg.auto_screenshot
        = env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_AUTOSSCREENSHOT")
          || env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_AUTO_SCREENSHOT");

    std::string layout_out;
    const bool has_layout_out
        = read_env_value("IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP_OUT", layout_out)
          && !layout_out.empty();
    cfg.layout_dump = env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_LAYOUT_DUMP")
                      || has_layout_out;

    std::string state_out;
    const bool has_state_out
        = read_env_value("IMIV_IMGUI_TEST_ENGINE_STATE_DUMP_OUT", state_out)
          && !state_out.empty();
    cfg.state_dump = env_flag_is_truthy("IMIV_IMGUI_TEST_ENGINE_STATE_DUMP")
                     || has_state_out;
    cfg.scenario_run = read_env_value("IMIV_IMGUI_TEST_ENGINE_SCENARIO_FILE",
                                      cfg.scenario_file)
                       && !cfg.scenario_file.empty();
    cfg.developer_menu_metrics = env_flag_is_truthy(
        "IMIV_IMGUI_TEST_ENGINE_DEVELOPER_MENU_METRICS");
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
                           || cfg.state_dump || cfg.scenario_run
                           || cfg.developer_menu_metrics || cfg.junit_xml;
#if defined(IMGUI_ENABLE_TEST_ENGINE) && !defined(NDEBUG)
    cfg.want_test_engine = true;
#endif
    cfg.automation_mode = cfg.auto_screenshot || cfg.layout_dump
                          || cfg.state_dump || cfg.scenario_run
                          || cfg.developer_menu_metrics;
    cfg.exit_on_finish = env_flag_is_truthy(
                             "IMIV_IMGUI_TEST_ENGINE_EXIT_ON_FINISH")
                         || cfg.automation_mode;
    cfg.show_windows = false;
    return cfg;
}

void
test_engine_start(TestEngineRuntime& runtime, TestEngineConfig& config,
                  const TestEngineHooks& hooks)
{
    runtime.engine       = nullptr;
    runtime.request_exit = false;
    runtime.show_windows = config.show_windows;

#if defined(IMGUI_ENABLE_TEST_ENGINE)
    g_test_engine_hooks = hooks;
    if (!config.want_test_engine)
        return;

    ImGuiTestEngine* engine = ImGuiTestEngine_CreateContext();
    if (engine == nullptr)
        return;
    runtime.engine = engine;

    ImGuiTestEngineIO& test_io        = ImGuiTestEngine_GetIO(engine);
    test_io.ConfigVerboseLevel        = ImGuiTestVerboseLevel_Info;
    test_io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;
    test_io.ConfigRunSpeed            = ImGuiTestRunSpeed_Normal;
    test_io.ConfigCaptureEnabled      = true;
    test_io.ScreenCaptureFunc         = hooks.screen_capture;
    test_io.ScreenCaptureUserData     = hooks.screen_capture_user_data;
    if (config.trace || config.automation_mode)
        test_io.ConfigLogToTTY = true;

    if (config.junit_xml) {
        std::string error_message;
        if (validate_test_output_path(
                std::filesystem::path(config.junit_xml_out), error_message)) {
            std::error_code ec;
            std::filesystem::path junit_path(config.junit_xml_out);
            if (!junit_path.parent_path().empty())
                std::filesystem::create_directories(junit_path.parent_path(),
                                                    ec);
            test_io.ExportResultsFormat = ImGuiTestEngineExportFormat_JUnitXml;
            test_io.ExportResultsFilename = config.junit_xml_out.c_str();
        } else {
            config.junit_xml = false;
        }
    }

    ImGuiTestEngine_Start(engine, ImGui::GetCurrentContext());
    if (config.auto_screenshot) {
        ImGuiTest* smoke = register_imiv_smoke_tests(engine);
        ImGuiTestEngine_QueueTest(engine, smoke);
        config.has_work = true;
    }
    if (config.layout_dump) {
        ImGuiTest* dump = register_imiv_layout_dump_tests(engine);
        ImGuiTestEngine_QueueTest(engine, dump);
        config.has_work = true;
    }
    if (config.state_dump) {
        ImGuiTest* dump = register_imiv_state_dump_tests(engine);
        ImGuiTestEngine_QueueTest(engine, dump);
        config.has_work = true;
    }
    if (config.scenario_run) {
        ImGuiTest* scenario = register_imiv_scenario_tests(engine);
        ImGuiTestEngine_QueueTest(engine, scenario);
        config.has_work = true;
    }
    if (config.developer_menu_metrics) {
        ImGuiTest* menu_test = register_imiv_developer_menu_tests(engine);
        ImGuiTestEngine_QueueTest(engine, menu_test);
        config.has_work = true;
    }
#else
    (void)hooks;
#endif
}

void
test_engine_stop(TestEngineRuntime& runtime)
{
#if defined(IMGUI_ENABLE_TEST_ENGINE)
    ImGuiTestEngine* engine = reinterpret_cast<ImGuiTestEngine*>(
        runtime.engine);
    runtime.request_exit = false;
    runtime.show_windows = false;
    if (engine != nullptr)
        ImGuiTestEngine_Stop(engine);
#else
    runtime.request_exit = false;
    runtime.show_windows = false;
#endif
}

void
test_engine_destroy(TestEngineRuntime& runtime)
{
#if defined(IMGUI_ENABLE_TEST_ENGINE)
    ImGuiTestEngine* engine = reinterpret_cast<ImGuiTestEngine*>(
        runtime.engine);
    runtime.engine       = nullptr;
    runtime.request_exit = false;
    runtime.show_windows = false;
    g_test_engine_hooks  = {};
    if (engine != nullptr)
        ImGuiTestEngine_DestroyContext(engine);
#else
    runtime.engine       = nullptr;
    runtime.request_exit = false;
    runtime.show_windows = false;
#endif
}

bool*
test_engine_show_windows_ptr(TestEngineRuntime& runtime)
{
#if defined(IMGUI_ENABLE_TEST_ENGINE)
    return runtime.engine ? &runtime.show_windows : nullptr;
#else
    (void)runtime;
    return nullptr;
#endif
}

void
test_engine_maybe_show_windows(TestEngineRuntime& runtime,
                               const TestEngineConfig& config)
{
#if defined(IMGUI_ENABLE_TEST_ENGINE)
    ImGuiTestEngine* engine = reinterpret_cast<ImGuiTestEngine*>(
        runtime.engine);
    if (engine != nullptr && runtime.show_windows && !config.automation_mode)
        ImGuiTestEngine_ShowTestEngineWindows(engine, nullptr);
#else
    (void)runtime;
    (void)config;
#endif
}

void
test_engine_post_swap(TestEngineRuntime& runtime)
{
#if defined(IMGUI_ENABLE_TEST_ENGINE)
    ImGuiTestEngine* engine = reinterpret_cast<ImGuiTestEngine*>(
        runtime.engine);
    if (engine != nullptr)
        ImGuiTestEngine_PostSwap(engine);
#else
    (void)runtime;
#endif
}

bool
test_engine_should_close(TestEngineRuntime& runtime,
                         const TestEngineConfig& config)
{
#if defined(IMGUI_ENABLE_TEST_ENGINE)
    ImGuiTestEngine* engine = reinterpret_cast<ImGuiTestEngine*>(
        runtime.engine);
    if (engine == nullptr)
        return false;
    if (runtime.request_exit && !config.exit_on_finish)
        return true;
    if (config.exit_on_finish && config.has_work) {
        ImGuiTestEngineIO& test_io = ImGuiTestEngine_GetIO(engine);
        if (!test_io.IsRunningTests && !test_io.IsCapturing
            && ImGuiTestEngine_IsTestQueueEmpty(engine)) {
            return true;
        }
    }
    return false;
#else
    (void)runtime;
    (void)config;
    return false;
#endif
}

void
reset_layout_dump_synthetic_items()
{
#if defined(IMGUI_ENABLE_TEST_ENGINE)
    g_layout_dump_synthetic_item_counter = 0;
#endif
}

void
reset_test_engine_mouse_space()
{
#if defined(IMGUI_ENABLE_TEST_ENGINE)
    g_test_engine_mouse_space = TestEngineMouseSpaceState();
#endif
}

void
update_test_engine_mouse_space(const ImVec2& viewport_min,
                               const ImVec2& viewport_max,
                               const ImVec2& image_min, const ImVec2& image_max)
{
#if defined(IMGUI_ENABLE_TEST_ENGINE)
    g_test_engine_mouse_space.viewport_valid = true;
    g_test_engine_mouse_space.viewport_min   = viewport_min;
    g_test_engine_mouse_space.viewport_max   = viewport_max;
    g_test_engine_mouse_space.image_valid    = (image_max.x > image_min.x
                                             && image_max.y > image_min.y);
    g_test_engine_mouse_space.image_min      = image_min;
    g_test_engine_mouse_space.image_max      = image_max;
#else
    (void)viewport_min;
    (void)viewport_max;
    (void)image_min;
    (void)image_max;
#endif
}

void
register_layout_dump_synthetic_item(const char* kind, const char* label)
{
#if defined(IMGUI_ENABLE_TEST_ENGINE)
    if (!layout_dump_items_enabled())
        return;
    ImGuiContext* ui_ctx = ImGui::GetCurrentContext();
    if (ui_ctx == nullptr)
        return;

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    register_layout_dump_synthetic_rect(kind, label, min, max);
#else
    (void)kind;
    (void)label;
#endif
}

void
register_layout_dump_synthetic_rect(const char* kind, const char* label,
                                    const ImVec2& min, const ImVec2& max)
{
#if defined(IMGUI_ENABLE_TEST_ENGINE)
    if (!layout_dump_items_enabled())
        return;
    ImGuiContext* ui_ctx = ImGui::GetCurrentContext();
    if (ui_ctx == nullptr)
        return;
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
#else
    (void)kind;
    (void)label;
    (void)min;
    (void)max;
#endif
}

}  // namespace Imiv
