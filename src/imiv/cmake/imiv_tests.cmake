# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (TARGET imiv
        AND OIIO_BUILD_TESTS
        AND BUILD_TESTING
        AND OIIO_IMIV_ADD_UPLOAD_SMOKE_CTEST
        AND UNIX)
    find_program (OIIO_IMIV_BASH_EXECUTABLE NAMES bash)
    if (OIIO_IMIV_BASH_EXECUTABLE)
        add_test (
            NAME imiv_upload_smoke
            COMMAND
                "${OIIO_IMIV_BASH_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_upload_corpus_ctest.sh"
                "${CMAKE_BINARY_DIR}")
        set_tests_properties (
            imiv_upload_smoke PROPERTIES
                LABELS "imiv;imiv_upload_smoke;gui"
                TIMEOUT 1800)
    else ()
        message (STATUS
                 "imiv: OIIO_IMIV_ADD_UPLOAD_SMOKE_CTEST requested but no bash executable was found")
    endif ()
endif ()

set (OIIO_IMIV_TEST_OCIO_CONFIG ""
     CACHE STRING
           "Optional external OCIO config for imiv live-update regressions")
set (_imiv_ocio_live_update_config "ocio://default")
if (NOT "${OIIO_IMIV_TEST_OCIO_CONFIG}" STREQUAL "")
    set (_imiv_ocio_live_update_config "${OIIO_IMIV_TEST_OCIO_CONFIG}")
endif ()
set (_imiv_ocio_config_source_input "ocio://default")
set (_imiv_ctest_default_backend_args --backend "${_imiv_selected_renderer}")
set (_imiv_ctest_stable_ui_backend_args ${_imiv_ctest_default_backend_args})
if (APPLE AND _imiv_enabled_metal)
    set (_imiv_ctest_stable_ui_backend_args --backend metal)
elseif ((TARGET OpenGL::GL OR TARGET OpenGL::OpenGL) AND _imiv_enabled_opengl)
    set (_imiv_ctest_stable_ui_backend_args --backend opengl)
elseif (_imiv_enabled_vulkan)
    set (_imiv_ctest_stable_ui_backend_args --backend vulkan)
endif ()
if (TARGET imiv
        AND OIIO_BUILD_TESTS
        AND BUILD_TESTING
        AND Python3_EXECUTABLE
        AND _imiv_test_engine_sources)
    if (_imiv_renderer_is_vulkan AND _imiv_has_runtime_glslang)
        add_test (
            NAME imiv_ocio_live_update_regression
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_ocio_live_update_regression.py"
                --bin "$<TARGET_FILE:imiv>"
                --cwd "$<TARGET_FILE_DIR:imiv>"
                ${_imiv_ctest_default_backend_args}
                --oiiotool "$<TARGET_FILE:oiiotool>"
                --idiff "$<TARGET_FILE:idiff>"
                --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
                --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/ocio_live_update_regression"
                --image "${CMAKE_BINARY_DIR}/imiv_captures/ocio_live_update_regression/ocio_live_input.exr"
                --ocio-config "${_imiv_ocio_live_update_config}"
                --switch-mode view)
        set_tests_properties (
            imiv_ocio_live_update_regression PROPERTIES
                LABELS "imiv;imiv_ocio;gui"
                TIMEOUT 300)

        add_test (
            NAME imiv_ocio_live_display_update_regression
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_ocio_live_update_regression.py"
                --bin "$<TARGET_FILE:imiv>"
                --cwd "$<TARGET_FILE_DIR:imiv>"
                ${_imiv_ctest_default_backend_args}
                --oiiotool "$<TARGET_FILE:oiiotool>"
                --idiff "$<TARGET_FILE:idiff>"
                --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
                --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/ocio_live_display_update_regression"
                --image "${CMAKE_BINARY_DIR}/imiv_captures/ocio_live_display_update_regression/ocio_live_input.exr"
                --ocio-config "${_imiv_ocio_live_update_config}"
                --switch-mode display)
        set_tests_properties (
            imiv_ocio_live_display_update_regression PROPERTIES
                LABELS "imiv;imiv_ocio;gui"
                TIMEOUT 300)
    elseif (_imiv_renderer_is_opengl)
        add_test (
            NAME imiv_opengl_ocio_live_update_regression
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_ocio_live_update_regression.py"
                --bin "$<TARGET_FILE:imiv>"
                --cwd "$<TARGET_FILE_DIR:imiv>"
                --backend opengl
                --oiiotool "$<TARGET_FILE:oiiotool>"
                --idiff "$<TARGET_FILE:idiff>"
                --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
                --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/opengl_ocio_live_update_regression"
                --image "${CMAKE_BINARY_DIR}/imiv_captures/opengl_ocio_live_update_regression/ocio_live_input.exr"
                --ocio-config "${_imiv_ocio_live_update_config}"
                --switch-mode view)
        set_tests_properties (
            imiv_opengl_ocio_live_update_regression PROPERTIES
                LABELS "imiv;imiv_ocio;imiv_opengl;gui"
                TIMEOUT 300)

        add_test (
            NAME imiv_opengl_ocio_live_display_update_regression
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_ocio_live_update_regression.py"
                --bin "$<TARGET_FILE:imiv>"
                --cwd "$<TARGET_FILE_DIR:imiv>"
                --backend opengl
                --oiiotool "$<TARGET_FILE:oiiotool>"
                --idiff "$<TARGET_FILE:idiff>"
                --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
                --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/opengl_ocio_live_display_update_regression"
                --image "${CMAKE_BINARY_DIR}/imiv_captures/opengl_ocio_live_display_update_regression/ocio_live_input.exr"
                --ocio-config "${_imiv_ocio_live_update_config}"
                --switch-mode display)
        set_tests_properties (
            imiv_opengl_ocio_live_display_update_regression PROPERTIES
                LABELS "imiv;imiv_ocio;imiv_opengl;gui"
                TIMEOUT 300)
    elseif (_imiv_renderer_is_metal)
        add_test (
            NAME imiv_metal_ocio_live_update_regression
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_ocio_live_update_regression.py"
                --bin "$<TARGET_FILE:imiv>"
                --cwd "$<TARGET_FILE_DIR:imiv>"
                --backend metal
                --oiiotool "$<TARGET_FILE:oiiotool>"
                --idiff "$<TARGET_FILE:idiff>"
                --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
                --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/metal_ocio_live_update_regression"
                --image "${CMAKE_BINARY_DIR}/imiv_captures/metal_ocio_live_update_regression/ocio_live_input.exr"
                --ocio-config "${_imiv_ocio_live_update_config}"
                --switch-mode view)
        set_tests_properties (
            imiv_metal_ocio_live_update_regression PROPERTIES
                LABELS "imiv;imiv_ocio;imiv_metal;gui"
                TIMEOUT 300)

        add_test (
            NAME imiv_metal_ocio_live_display_update_regression
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_ocio_live_update_regression.py"
                --bin "$<TARGET_FILE:imiv>"
                --cwd "$<TARGET_FILE_DIR:imiv>"
                --backend metal
                --oiiotool "$<TARGET_FILE:oiiotool>"
                --idiff "$<TARGET_FILE:idiff>"
                --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
                --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/metal_ocio_live_display_update_regression"
                --image "${CMAKE_BINARY_DIR}/imiv_captures/metal_ocio_live_display_update_regression/ocio_live_input.exr"
                --ocio-config "${_imiv_ocio_live_update_config}"
                --switch-mode display)
        set_tests_properties (
            imiv_metal_ocio_live_display_update_regression PROPERTIES
                LABELS "imiv;imiv_ocio;imiv_metal;gui"
                TIMEOUT 300)
    endif ()
endif ()

if (TARGET imiv
        AND OIIO_BUILD_TESTS
        AND BUILD_TESTING
        AND Python3_EXECUTABLE
        AND _imiv_test_engine_sources)
    if (_imiv_renderer_is_opengl)
        add_test (
            NAME imiv_opengl_smoke_regression
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_opengl_smoke_regression.py"
                --bin "$<TARGET_FILE:imiv>"
                --cwd "$<TARGET_FILE_DIR:imiv>"
                --backend opengl
                --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
                --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/opengl_smoke_regression"
                --open "${PROJECT_SOURCE_DIR}/ASWF/logos/openimageio-stacked-gradient.png")
        set_tests_properties (
            imiv_opengl_smoke_regression PROPERTIES
                LABELS "imiv;imiv_opengl;gui"
                TIMEOUT 180)

        add_test (
            NAME imiv_opengl_multiopen_ocio_regression
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_opengl_multiopen_ocio_regression.py"
                --bin "$<TARGET_FILE:imiv>"
                --cwd "$<TARGET_FILE_DIR:imiv>"
                --backend opengl
                --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
                --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/opengl_multiopen_ocio_regression"
                --open "${PROJECT_SOURCE_DIR}/ASWF/logos/openimageio-stacked-gradient.png")
        set_tests_properties (
            imiv_opengl_multiopen_ocio_regression PROPERTIES
                LABELS "imiv;imiv_opengl;imiv_ocio;gui"
                TIMEOUT 180)
    elseif (_imiv_renderer_is_metal)
        add_test (
            NAME imiv_metal_screenshot_regression
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_metal_screenshot_regression.py"
                --bin "$<TARGET_FILE:imiv>"
                --cwd "$<TARGET_FILE_DIR:imiv>"
                --backend metal
                --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
                --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/metal_screenshot_regression"
                --open "${PROJECT_SOURCE_DIR}/ASWF/logos/openimageio-stacked-gradient.png")
        set_tests_properties (
            imiv_metal_screenshot_regression PROPERTIES
                LABELS "imiv;imiv_metal;gui"
                TIMEOUT 180)

        if (TARGET oiiotool)
            add_test (
                NAME imiv_metal_orientation_regression
                COMMAND
                    "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_metal_orientation_regression.py"
                    --bin "$<TARGET_FILE:imiv>"
                    --cwd "$<TARGET_FILE_DIR:imiv>"
                    --backend metal
                    --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
                    --oiiotool "$<TARGET_FILE:oiiotool>"
                    --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/metal_orientation_regression"
                    --open "${PROJECT_SOURCE_DIR}/ASWF/logos/openimageio-stacked-gradient.png")
            set_tests_properties (
                imiv_metal_orientation_regression PROPERTIES
                    LABELS "imiv;imiv_metal;gui"
                    TIMEOUT 180)
        endif ()
    endif ()

    add_test (
        NAME imiv_ocio_config_source_regression
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_ocio_config_source_regression.py"
            --bin "$<TARGET_FILE:imiv>"
            --cwd "$<TARGET_FILE_DIR:imiv>"
            ${_imiv_ctest_default_backend_args}
            --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
            --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/ocio_config_source_regression"
            --ocio-config "${_imiv_ocio_config_source_input}")
    set_tests_properties (
        imiv_ocio_config_source_regression PROPERTIES
            LABELS "imiv;imiv_ocio;gui"
            TIMEOUT 240
            RUN_SERIAL TRUE)

    add_test (
        NAME imiv_ocio_missing_fallback_regression
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_ocio_missing_fallback_regression.py"
            --bin "$<TARGET_FILE:imiv>"
            --cwd "$<TARGET_FILE_DIR:imiv>"
            ${_imiv_ctest_default_backend_args}
            --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
            --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/ocio_missing_fallback_regression"
            --open "${PROJECT_SOURCE_DIR}/ASWF/logos/openimageio-stacked-gradient.png")
    set_tests_properties (
        imiv_ocio_missing_fallback_regression PROPERTIES
            LABELS "imiv;imiv_ocio;gui"
            TIMEOUT 180)

    add_test (
        NAME imiv_developer_menu_regression
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_developer_menu_regression.py"
            --bin "$<TARGET_FILE:imiv>"
            --cwd "$<TARGET_FILE_DIR:imiv>"
            ${_imiv_ctest_default_backend_args}
            --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
            --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/developer_menu_regression"
            --open "${PROJECT_SOURCE_DIR}/ASWF/logos/openimageio-stacked-gradient.png")
    set_tests_properties (
        imiv_developer_menu_regression PROPERTIES
            LABELS "imiv;imiv_devmode;gui"
            TIMEOUT 120)

    add_test (
        NAME imiv_area_probe_closeup_regression
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_area_probe_closeup_regression.py"
            --bin "$<TARGET_FILE:imiv>"
            --cwd "$<TARGET_FILE_DIR:imiv>"
            ${_imiv_ctest_default_backend_args}
            --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
            --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/area_probe_closeup_regression"
            --open "${PROJECT_SOURCE_DIR}/ASWF/logos/openimageio-stacked-gradient.png")
    set_tests_properties (
        imiv_area_probe_closeup_regression PROPERTIES
            LABELS "imiv;gui"
            TIMEOUT 120)

    add_test (
        NAME imiv_auto_subimage_regression
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_auto_subimage_regression.py"
            --bin "$<TARGET_FILE:imiv>"
            --cwd "$<TARGET_FILE_DIR:imiv>"
            ${_imiv_ctest_default_backend_args}
            --oiiotool "$<TARGET_FILE:oiiotool>"
            --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
            --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/auto_subimage_regression"
            --image "${CMAKE_BINARY_DIR}/imiv_captures/auto_subimage_regression/auto_subimages.tif")
    set_tests_properties (
        imiv_auto_subimage_regression PROPERTIES
            LABELS "imiv;gui"
            TIMEOUT 180)

    add_test (
        NAME imiv_ux_actions_regression
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_ux_actions_regression.py"
            --bin "$<TARGET_FILE:imiv>"
            --cwd "$<TARGET_FILE_DIR:imiv>"
            ${_imiv_ctest_default_backend_args}
            --oiiotool "$<TARGET_FILE:oiiotool>"
            --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
            --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/ux_actions_regression")
    set_tests_properties (
        imiv_ux_actions_regression PROPERTIES
            LABELS "imiv;gui"
            TIMEOUT 360)

    add_test (
        NAME imiv_multiview_regression
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_multiview_regression.py"
            --bin "$<TARGET_FILE:imiv>"
            --cwd "$<TARGET_FILE_DIR:imiv>"
            ${_imiv_ctest_default_backend_args}
            --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
            --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/multiview_regression"
            --open "${PROJECT_SOURCE_DIR}/ASWF/logos/openimageio-stacked-gradient.png")
    set_tests_properties (
        imiv_multiview_regression PROPERTIES
            LABELS "imiv;gui;imiv_multiview"
            TIMEOUT 180)

    add_test (
        NAME imiv_image_list_regression
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_image_list_regression.py"
            --bin "$<TARGET_FILE:imiv>"
            --cwd "$<TARGET_FILE_DIR:imiv>"
            ${_imiv_ctest_default_backend_args}
            --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
            --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/image_list_regression")
    set_tests_properties (
        imiv_image_list_regression PROPERTIES
            LABELS "imiv;gui;imiv_multiview"
            TIMEOUT 180)

    add_test (
        NAME imiv_image_list_interaction_regression
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_image_list_interaction_regression.py"
            --bin "$<TARGET_FILE:imiv>"
            --cwd "$<TARGET_FILE_DIR:imiv>"
            ${_imiv_ctest_stable_ui_backend_args}
            --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
            --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/image_list_interaction_regression")
    set_tests_properties (
        imiv_image_list_interaction_regression PROPERTIES
            LABELS "imiv;gui;imiv_multiview"
            TIMEOUT 180)

    if (TARGET oiiotool)
        add_test (
            NAME imiv_image_list_center_regression
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_image_list_center_regression.py"
                --bin "$<TARGET_FILE:imiv>"
                --cwd "$<TARGET_FILE_DIR:imiv>"
                --backend opengl
                --oiiotool "$<TARGET_FILE:oiiotool>"
                --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
                --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/image_list_center_regression")
        set_tests_properties (
            imiv_image_list_center_regression PROPERTIES
                LABELS "imiv;gui;imiv_multiview"
                TIMEOUT 180)
    endif ()

    add_test (
        NAME imiv_drag_drop_regression
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_drag_drop_regression.py"
            --bin "$<TARGET_FILE:imiv>"
            --cwd "$<TARGET_FILE_DIR:imiv>"
            ${_imiv_ctest_stable_ui_backend_args}
            --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
            --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/drag_drop_regression")
    set_tests_properties (
        imiv_drag_drop_regression PROPERTIES
            LABELS "imiv;gui;imiv_multiview"
            TIMEOUT 180)

    add_test (
        NAME imiv_view_recipe_regression
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_view_recipe_regression.py"
            --bin "$<TARGET_FILE:imiv>"
            ${_imiv_ctest_stable_ui_backend_args}
            --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
            --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/view_recipe_regression")
    set_tests_properties (
        imiv_view_recipe_regression PROPERTIES
            LABELS "imiv;gui;imiv_multiview"
            TIMEOUT 180)

    add_test (
        NAME imiv_open_folder_regression
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_open_folder_regression.py"
            --bin "$<TARGET_FILE:imiv>"
            --cwd "$<TARGET_FILE_DIR:imiv>"
            ${_imiv_ctest_default_backend_args}
            --oiiotool "$<TARGET_FILE:oiiotool>"
            --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
            --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/open_folder_regression")
    set_tests_properties (
        imiv_open_folder_regression PROPERTIES
            LABELS "imiv;gui;imiv_multiview"
            TIMEOUT 180)

    add_test (
        NAME imiv_save_selection_regression
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_save_selection_regression.py"
            --bin "$<TARGET_FILE:imiv>"
            --cwd "$<TARGET_FILE_DIR:imiv>"
            ${_imiv_ctest_stable_ui_backend_args}
            --oiiotool "$<TARGET_FILE:oiiotool>"
            --idiff "$<TARGET_FILE:idiff>"
            --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
            --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/save_selection_regression")
    set_tests_properties (
        imiv_save_selection_regression PROPERTIES
            LABELS "imiv;gui;imiv_export"
            TIMEOUT 180)

    add_test (
        NAME imiv_export_selection_regression
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_export_selection_regression.py"
            --bin "$<TARGET_FILE:imiv>"
            --cwd "$<TARGET_FILE_DIR:imiv>"
            ${_imiv_ctest_stable_ui_backend_args}
            --oiiotool "$<TARGET_FILE:oiiotool>"
            --idiff "$<TARGET_FILE:idiff>"
            --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
            --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/export_selection_regression")
    set_tests_properties (
        imiv_export_selection_regression PROPERTIES
            LABELS "imiv;gui;imiv_export"
            TIMEOUT 180)

    add_test (
        NAME imiv_save_window_regression
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_save_window_regression.py"
            --bin "$<TARGET_FILE:imiv>"
            --cwd "$<TARGET_FILE_DIR:imiv>"
            ${_imiv_ctest_stable_ui_backend_args}
            --oiiotool "$<TARGET_FILE:oiiotool>"
            --idiff "$<TARGET_FILE:idiff>"
            --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
            --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/save_window_regression")
    set_tests_properties (
        imiv_save_window_regression PROPERTIES
            LABELS "imiv;gui;imiv_export"
            TIMEOUT 180)

    add_test (
        NAME imiv_save_window_ocio_regression
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_save_window_ocio_regression.py"
            --bin "$<TARGET_FILE:imiv>"
            --cwd "$<TARGET_FILE_DIR:imiv>"
            ${_imiv_ctest_stable_ui_backend_args}
            --oiiotool "$<TARGET_FILE:oiiotool>"
            --idiff "$<TARGET_FILE:idiff>"
            --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
            --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/save_window_ocio_regression")
    set_tests_properties (
        imiv_save_window_ocio_regression PROPERTIES
            LABELS "imiv;gui;imiv_export;imiv_ocio"
            TIMEOUT 180)

    add_test (
        NAME imiv_rgb_input_regression
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_rgb_input_regression.py"
            --bin "$<TARGET_FILE:imiv>"
            --cwd "$<TARGET_FILE_DIR:imiv>"
            ${_imiv_ctest_default_backend_args}
            --oiiotool "$<TARGET_FILE:oiiotool>"
            --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
            --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/rgb_input_regression"
            --source-image "${PROJECT_SOURCE_DIR}/ASWF/logos/openimageio-stacked-gradient.png")
    set_tests_properties (
        imiv_rgb_input_regression PROPERTIES
            LABELS "imiv;imiv_rgb;gui"
            TIMEOUT 120)

    add_test (
        NAME imiv_sampling_regression
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_sampling_regression.py"
            --bin "$<TARGET_FILE:imiv>"
            --cwd "$<TARGET_FILE_DIR:imiv>"
            ${_imiv_ctest_default_backend_args}
            --oiiotool "$<TARGET_FILE:oiiotool>"
            --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
            --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/sampling_regression")
    set_tests_properties (
        imiv_sampling_regression PROPERTIES
            LABELS "imiv;imiv_sampling;gui"
            TIMEOUT 180)

    if (_imiv_enabled_vulkan)
        add_test (
            NAME imiv_large_image_switch_regression_vulkan
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_large_image_switch_regression.py"
                --bin "$<TARGET_FILE:imiv>"
                --cwd "$<TARGET_FILE_DIR:imiv>"
                --backend vulkan
                --oiiotool "$<TARGET_FILE:oiiotool>"
                --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
                --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/large_image_switch_regression_vulkan")
        set_tests_properties (
            imiv_large_image_switch_regression_vulkan PROPERTIES
                LABELS "imiv;gui;imiv_vulkan;imiv_large"
                TIMEOUT 300)
    endif ()

    if (_imiv_enabled_opengl)
        add_test (
            NAME imiv_large_image_switch_regression_opengl
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_large_image_switch_regression.py"
                --bin "$<TARGET_FILE:imiv>"
                --cwd "$<TARGET_FILE_DIR:imiv>"
                --backend opengl
                --oiiotool "$<TARGET_FILE:oiiotool>"
                --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
                --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/large_image_switch_regression_opengl")
        set_tests_properties (
            imiv_large_image_switch_regression_opengl PROPERTIES
                LABELS "imiv;gui;imiv_opengl;imiv_large"
                TIMEOUT 300)
    endif ()

    if (_imiv_enabled_metal)
        add_test (
            NAME imiv_large_image_switch_regression_metal
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_large_image_switch_regression.py"
                --bin "$<TARGET_FILE:imiv>"
                --cwd "$<TARGET_FILE_DIR:imiv>"
                --backend metal
                --oiiotool "$<TARGET_FILE:oiiotool>"
                --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
                --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/large_image_switch_regression_metal")
        set_tests_properties (
            imiv_large_image_switch_regression_metal PROPERTIES
                LABELS "imiv;gui;imiv_metal;imiv_large"
                TIMEOUT 300)
    endif ()

    if (_imiv_enabled_backend_count GREATER 1)
        add_test (
            NAME imiv_backend_preferences_regression
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_backend_preferences_regression.py"
                --bin "$<TARGET_FILE:imiv>"
                --cwd "$<TARGET_FILE_DIR:imiv>"
                ${_imiv_ctest_default_backend_args}
                --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
                --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/backend_preferences_regression"
                --open "${PROJECT_SOURCE_DIR}/ASWF/logos/openimageio-stacked-gradient.png")
        set_tests_properties (
            imiv_backend_preferences_regression PROPERTIES
                LABELS "imiv;gui;imiv_backend"
                TIMEOUT 180
                SKIP_RETURN_CODE 77)
    endif ()

    if (OIIO_IMIV_ADD_BACKEND_VERIFY_CTEST)
        if (_imiv_enabled_vulkan)
            add_test (
                NAME imiv_backend_verify_vulkan
                COMMAND
                    "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_backend_verify.py"
                    --backend vulkan
                    --build-dir "${CMAKE_BINARY_DIR}"
                    --config "$<CONFIG>"
                    --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/backend_verify_vulkan"
                    --skip-configure
                    --skip-build)
            set_tests_properties (
                imiv_backend_verify_vulkan PROPERTIES
                    LABELS "imiv;imiv_backend_verify;imiv_vulkan;gui"
                    TIMEOUT 1800
                    RUN_SERIAL TRUE)
        endif ()
        if (_imiv_enabled_opengl)
            add_test (
                NAME imiv_backend_verify_opengl
                COMMAND
                    "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_backend_verify.py"
                    --backend opengl
                    --build-dir "${CMAKE_BINARY_DIR}"
                    --config "$<CONFIG>"
                    --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/backend_verify_opengl"
                    --skip-configure
                    --skip-build)
            set_tests_properties (
                imiv_backend_verify_opengl PROPERTIES
                    LABELS "imiv;imiv_backend_verify;imiv_opengl;gui"
                    TIMEOUT 1800
                    RUN_SERIAL TRUE)
        endif ()
        if (_imiv_enabled_metal)
            add_test (
                NAME imiv_backend_verify_metal
                COMMAND
                    "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_backend_verify.py"
                    --backend metal
                    --build-dir "${CMAKE_BINARY_DIR}"
                    --config "$<CONFIG>"
                    --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/backend_verify_metal"
                    --skip-configure
                    --skip-build)
            set_tests_properties (
                imiv_backend_verify_metal PROPERTIES
                    LABELS "imiv;imiv_backend_verify;imiv_metal;gui"
                    TIMEOUT 1800
                    RUN_SERIAL TRUE)
        endif ()
    endif ()
elseif (TARGET imiv
        AND OIIO_BUILD_TESTS
        AND BUILD_TESTING)
    message (STATUS
             "imiv: developer menu regression test not added (requires Python3 and ImGui Test Engine)")
endif ()
