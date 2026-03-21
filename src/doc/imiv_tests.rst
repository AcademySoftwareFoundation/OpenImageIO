..
  Copyright Contributors to the OpenImageIO project.
  SPDX-License-Identifier: CC-BY-4.0


.. _chap-imiv-tests:

`imiv` Test Suite and ImGui Test Engine
#######################################

.. highlight:: bash


Overview
========

:program:`imiv` test automation is built around Dear ImGui Test Engine, with a
thin layer of :program:`imiv`-specific helpers for:

* screenshots of the main viewport;
* layout dumps of relevant Dear ImGui windows and items;
* viewer-state JSON dumps with backend and OCIO state;
* XML-driven multi-step UI scenarios;
* Python wrappers that generate fixtures, run focused regressions, and collect
  artifacts.

The intent is not only to catch regressions, but also to make new UI work easy
to observe and extend.


Build requirements
==================

To compile the automation hooks, configure :program:`imiv` with Dear ImGui
Test Engine available and enable:

.. code-block::

    -D OIIO_IMIV_ENABLE_IMGUI_TEST_ENGINE=ON

The build discovers the test-engine sources from either:

* `OIIO_IMIV_TEST_ENGINE_ROOT`, or
* the `IMGUI_TEST_ENGINE_ROOT` environment variable.

When the test engine is not compiled in, :program:`imiv` will warn if
automation is requested through `IMIV_IMGUI_TEST_ENGINE*` environment
variables.


Quick start
===========

The simplest entry point is the Python runner:

    `python src/imiv/tools/imiv_gui_test_run.py ...`

Screenshot only::

    python3 src/imiv/tools/imiv_gui_test_run.py \
      --bin build_u/bin/imiv \
      --open ASWF/logos/openimageio-stacked-gradient.png \
      --screenshot-out build_u/test_captures/smoke.png

Screenshot + layout + state + JUnit::

    python3 src/imiv/tools/imiv_gui_test_run.py \
      --bin build_u/bin/imiv \
      --open ASWF/logos/openimageio-stacked-gradient.png \
      --screenshot-out build_u/test_captures/smoke.png \
      --layout-json-out build_u/test_captures/smoke.layout.json \
      --layout-items \
      --state-json-out build_u/test_captures/smoke.state.json \
      --junit-out build_u/test_captures/imiv_tests.junit.xml

Scenario-driven run::

    python3 src/imiv/tools/imiv_gui_test_run.py \
      --bin build_u/bin/imiv \
      --open build_u/imiv_captures/ux_actions_regression/ux_actions_input.png \
      --scenario Testing/verify_opengl/runtime_ux/ux_actions.scenario.xml

Layout JSON to SVG::

    python3 src/imiv/tools/imiv_gui_test_run.py \
      --bin build_u/bin/imiv \
      --open ASWF/logos/openimageio-stacked-gradient.png \
      --layout-json-out build_u/test_captures/layout.json \
      --layout-items \
      --svg-out build_u/test_captures/layout.svg \
      --svg-items --svg-labels

The runner translates command-line flags into the `IMIV_IMGUI_TEST_ENGINE_*`
environment variables understood by the C++ integration layer.


Built-in test modes
===================

The C++ side registers several built-in Dear ImGui Test Engine tests:

* `imiv/smoke_screenshot`
  screenshot capture with optional synthetic input.
* `imiv/dump_layout_json`
  layout JSON export.
* `imiv/dump_viewer_state`
  viewer-state JSON export.
* `imiv/scenario`
  XML-driven multi-step scenario execution.
* `imiv/developer_menu_metrics_window`
  focused debug-build regression around the Developer menu.

Most users should drive those modes through the Python wrappers, but it is
useful to know that the wrappers are not inventing separate automation paths;
they are exercising these built-in test registrations.


Scenario XML
============

Scenario files are rooted at `<imiv-scenario>` and must provide an `out_dir`
attribute. Optional root attributes:

* `layout_items`
  default boolean for per-step layout item capture.
* `layout_depth`
  default gather depth for per-step layout capture.

Each `<step>` must provide a non-empty `name`.

Supported step attributes
-------------------------

Timing:

* `delay_frames`
* `post_action_delay_frames`

Keyboard and item actions:

* `key_chord`
* `set_ref`
* `item_click`

Mouse positioning and input:

* `mouse_pos`
* `mouse_pos_window_rel`
* `mouse_pos_image_rel`
* `mouse_click_button`
* `mouse_wheel`
* `mouse_drag`
* `mouse_drag_button`
* `mouse_drag_hold`
* `mouse_drag_hold_button`
* `mouse_drag_hold_frames`

OCIO overrides:

* `ocio_use`
* `ocio_display`
* `ocio_view`
* `ocio_image_color_space`

Capture flags:

* `screenshot`
* `layout`
* `state`
* `layout_items`
* `layout_depth`

Minimal example::

    <?xml version='1.0' encoding='utf-8'?>
    <imiv-scenario out_dir="../../verify/runtime" layout_items="true">
      <step name="select_drag"
            key_chord="ctrl+a"
            mouse_pos_image_rel="0.18,0.25"
            mouse_drag="180,120"
            mouse_drag_button="0"
            state="true"
            post_action_delay_frames="2" />
      <step name="fit_image_to_window"
            key_chord="alt+f"
            state="true"
            post_action_delay_frames="3" />
    </imiv-scenario>

Real examples live in:

* `Testing/verify_opengl/runtime_ux/ux_actions.scenario.xml`
* `Testing/verify_metal/runtime_ocio_live/ocio_live.scenario.xml`

Key-chord strings use tokens such as `ctrl+a`, `alt+f`, `ctrl+period`,
`pageup`, `f12`, `kpadd`, and similar spellings understood by the parser in
`imiv_test_engine.cpp`.


Direct Dear ImGui Test Engine usage
===================================

Not every regression needs an XML scenario. For focused UI interactions,
direct test-engine calls in C++ are often clearer.

Example pattern used by the developer-menu regression:

.. code-block:: cpp

    const ImGuiTestItemInfo developer_menu =
        ctx->ItemInfo("##MainMenuBar##MenuBar/Developer",
                      ImGuiTestOpFlags_NoError);
    ctx->ItemClick("##MainMenuBar##MenuBar/Developer");
    ctx->Yield(1);
    ctx->ItemClick("//$FOCUSED/ImGui Demo");
    ctx->Yield(2);

That style is useful when:

* the flow is short and deterministic;
* the test needs direct control over ImGui references;
* you are building a reusable primitive before deciding whether it belongs in
  the higher-level XML scenario format.


Automation artifacts
====================

The automation layer intentionally produces both visual and structural output.

Screenshots
-----------

PNG screenshots capture the main Dear ImGui viewport. They are useful for:

* smoke testing a backend;
* comparing sampling or OCIO output;
* producing cropped diffs with tools such as :program:`idiff`.

Layout dumps
------------

Layout dumps are JSON files containing the active windows and, optionally, the
gathered items within them. They are useful for:

* checking whether a window or control exists at all;
* comparing window placement and hierarchy;
* producing SVG overlays with `imiv_layout_json_to_svg.py`.

Viewer-state dumps
------------------

Viewer-state JSON is often the best assertion surface for new regressions. It
already includes fields such as:

* `image_loaded`, `image_path`, `zoom`, and `fit_image_to_window`
* selection and Area Sample state
* backend state (`active`, `requested`, `next_launch`, compiled backends)
* OCIO state (requested source, resolved source, resolved config path,
  display/view, available menus)

Example excerpt::

    {
      "image_loaded": true,
      "zoom": 1.000000,
      "selection_active": false,
      "backend": {
        "active": "vulkan",
        "requested": "auto",
        "next_launch": "vulkan"
      }
    }

JUnit XML
---------

JUnit XML export is useful when automation is launched outside CTest and you
still want CI-friendly pass/fail reporting.


Backend-wide verification
=========================

The canonical multi-check wrapper is:

    `python src/imiv/tools/imiv_backend_verify.py ...`

Examples:

Vulkan::

    python3 src/imiv/tools/imiv_backend_verify.py \
      --backend vulkan \
      --build-dir build_u \
      --out-dir Testing/verify_vulkan \
      --trace

OpenGL::

    python3 src/imiv/tools/imiv_backend_verify.py \
      --backend opengl \
      --build-dir build_u \
      --out-dir Testing/verify_opengl \
      --trace

Metal::

    python3 src/imiv/tools/imiv_backend_verify.py \
      --backend metal \
      --build-dir build \
      --out-dir Testing/verify_metal \
      --trace

This wrapper fans out into the focused regression scripts for smoke, UX,
sampling/orientation where available, OCIO fallback/config-source/live-update
coverage, and stores the resulting logs in files such as:

* `verify_smoke.log`
* `verify_ux.log`
* `verify_screenshot.log`
* `verify_sampling.log`
* `verify_orientation.log`
* `verify_ocio_missing.log`
* `verify_ocio_config_source.log`
* `verify_ocio_live.log`
* `verify_ocio_live_display.log`


CTest integration
=================

When :program:`imiv` is built with tests enabled and Dear ImGui Test Engine is
available, focused regressions are added to CTest. Useful commands:

List the :program:`imiv` tests::

    ctest --test-dir build_u -N | rg imiv

Run a focused UI regression::

    ctest --test-dir build_u -V -R '^imiv_ux_actions_regression$'

Run the backend-preference regression::

    ctest --test-dir build_u -V -R '^imiv_backend_preferences_regression$'

If `OIIO_IMIV_ADD_BACKEND_VERIFY_CTEST=ON` was enabled at configure time, the
longer backend-wide verification entries are also added:

* `imiv_backend_verify_vulkan`
* `imiv_backend_verify_opengl`
* `imiv_backend_verify_metal`


Extending the suite
===================

When adding new UI or UX behavior, the most durable workflow is usually:

1. Make the feature observable.
   Add stable UI labels, a layout-dump synthetic marker, a viewer-state JSON
   field, or some combination of the three.
2. Prefer state assertions over screenshot-only assertions.
   Screenshots are valuable, but state dumps usually fail more clearly.
3. Keep tests isolated.
   Set `IMIV_CONFIG_HOME` in wrappers so preferences and recent-file history do
   not leak between runs.
4. Reuse existing helpers.
   Generate fixtures with OIIO tools when practical, and build on
   `imiv_gui_test_run.py` or the existing focused regression scripts before
   inventing a new runner.
5. Add backend coverage intentionally.
   If a feature is backend-sensitive, decide whether it needs a backend-wide
   verification entry, a focused regression, or both.

One subtle but important rule: the Python runner rewrites output paths
relative to the working directory used to launch :program:`imiv`. That is the
recommended way to drive automation. It keeps path handling consistent, and it
avoids the stricter absolute-path restrictions enforced by the C++ hooks in
release builds.
