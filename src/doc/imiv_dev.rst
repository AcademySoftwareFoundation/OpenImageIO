..
  Copyright Contributors to the OpenImageIO project.
  SPDX-License-Identifier: CC-BY-4.0


.. _chap-imiv-dev:

`imiv` Developer Guide
######################

.. highlight:: bash


Overview
========

The main :program:`imiv` task today is not feature count. It is keeping the
viewer understandable while it grows toward :program:`iv` parity.

The design rules are simple:

* keep application, viewer, and UI code backend-agnostic;
* keep renderer-specific code behind a small contract;
* make backend gaps explicit rather than silent;
* keep visible behavior observable through tests, layout dumps, or state dumps;
* prefer Dear ImGui public API calls over internal helpers.

The rest of this page explains how that is done in the current code.


Source layout
=============

The current codebase is split by responsibility first, then by backend.
That keeps most feature work in shared code and leaves backend files focused
on GPU upload, preview rendering, and platform integration.

Core entry points
-----------------

* `src/imiv/imiv_main.cpp`
  parses CLI arguments in `getargs()`, builds `AppConfig`, handles small
  process-level options such as `--list-backends`, and calls `Imiv::run()`.
* `src/imiv/imiv_app.cpp`
  owns startup, backend resolution, GLFW bootstrap, Dear ImGui context setup,
  the main frame loop, and shutdown.
* `src/imiv/imiv_frame.cpp`
  assembles one UI frame. This is the highest-value file for understanding
  how menus, actions, the image window, docking, and auxiliary windows fit
  together.

Shared viewer/UI code
---------------------

* `src/imiv/imiv_viewer.h` and `src/imiv/imiv_viewer.cpp`
  define the persistent viewer state, preference storage, image metadata
  extraction, and config-file handling.
* `src/imiv/imiv_actions.cpp`
  contains state-changing actions such as load, reload, save, navigation, and
  orientation edits. Most mutations that survive beyond the current frame
  should land here instead of being buried inside window drawing code.
* `src/imiv/imiv_menu.cpp`
  defines the main menu, keyboard shortcuts, and the frame-action queue used
  to defer mutations until after UI collection.
* `src/imiv/imiv_aux_windows.cpp`
  draws auxiliary windows such as Preferences, Info, and Preview.
* `src/imiv/imiv_image_view.cpp`, `src/imiv/imiv_navigation.cpp`,
  `src/imiv/imiv_overlays.cpp`, and `src/imiv/imiv_ui.cpp`
  handle the image viewport, coordinate transforms, mouse interaction,
  overlays, sampling helpers, and small reusable UI helpers.

Renderer seam
-------------

* `src/imiv/imiv_renderer.h`
  is the renderer-neutral API used by the rest of the viewer.
* `src/imiv/imiv_renderer_backend.h`
  defines the backend vtable boundary.
* `src/imiv/imiv_renderer.cpp`
  dispatches shared renderer calls to the selected backend.

Backend implementations
-----------------------

* Vulkan:
  `imiv_renderer_vulkan.cpp` plus `imiv_vulkan_*` modules.
* Metal:
  `imiv_renderer_metal.mm`.
* OpenGL:
  `imiv_renderer_opengl.cpp`.

Supporting subsystems
---------------------

* `src/imiv/imiv_ocio.cpp`
  holds renderer-agnostic OCIO selection logic and target-specific shader
  generation.
* `src/imiv/imiv_file_dialog.cpp`
  wraps optional nativefiledialog integration.
* `src/imiv/imiv_test_engine.cpp`
  integrates Dear ImGui Test Engine and automation helpers.
* `src/imiv/shaders/`
  contains the static Vulkan shader sources used for upload and preview.


Startup and shutdown flow
=========================

The easiest way to understand :program:`imiv` is to follow
`imiv_main.cpp` -> `Imiv::run()` -> `draw_viewer_ui()`.

Launch path
-----------

The startup sequence in `src/imiv/imiv_app.cpp` is:

1. `imiv_main.cpp` parses CLI arguments into `AppConfig`.
2. `run()` loads persistent app state with `load_persistent_state()`.
3. `run()` initializes GLFW and refreshes runtime backend availability.
4. `run()` resolves the requested backend with
   `requested_backend_for_launch()` and `resolve_backend_request()`.
5. The main window is created for the resolved backend.
6. The Dear ImGui context is created and configured:

   * `ImGuiConfigFlags_NavEnableKeyboard`
   * `ImGuiConfigFlags_DockingEnable`
   * `ImGuiConfigFlags_ViewportsEnable`

7. Fonts and the base application style are loaded.
8. Dear ImGui layout data is loaded from disk with
   `ImGui::LoadIniSettingsFromDisk()`.
9. The selected renderer backend is initialized:

   * instance/device setup;
   * swapchain or drawable setup;
   * backend-specific Dear ImGui renderer bootstrap.

10. Optional OCIO preview support is preflighted for the active backend.
11. Startup images are loaded with `load_viewer_image()`.
12. Drag and drop and optional Dear ImGui Test Engine hooks are installed.

Main loop
---------

The loop in `run()` keeps the order narrow and explicit:

* poll GLFW events;
* resize the backend main window if needed;
* start a new Dear ImGui frame:

  * `renderer_imgui_new_frame()`
  * `platform_glfw_imgui_new_frame()`
  * `ImGui::NewFrame()`

* call `draw_viewer_ui()`;
* apply any style preset change requested by the UI;
* render Dear ImGui draw data with `ImGui::Render()`;
* render the main window through the selected backend;
* if multi-viewports are enabled, call
  `ImGui::UpdatePlatformWindows()` and
  `ImGui::RenderPlatformWindowsDefault()`;
* execute delayed developer actions such as screenshot capture;
* present the frame;
* save settings if Dear ImGui requests it.

Shutdown path
-------------

Before shutdown, `run()`:

* serializes Dear ImGui settings with `ImGui::SaveIniSettingsToMemory()`;
* writes combined settings through `save_persistent_state()`;
* waits for the active renderer to go idle;
* destroys the loaded viewer texture;
* stops test-engine integration;
* shuts down the renderer backend, Dear ImGui, GLFW, and the main window.

This ordering matters. The code avoids destroying Dear ImGui state while the
renderer still owns textures or platform windows that may reference it.


State ownership and persistence
===============================

Shared state is split into a few clear buckets. Keeping this split intact makes
new work easier to reason about.

`ViewerState`
-------------

`ViewerState` in `src/imiv/imiv_viewer.h` is the per-view runtime model for the
currently displayed image and its interaction state. It owns:

* the current `LoadedImage`;
* the current `ViewRecipe`;
* status and error text;
* zoom, scroll, zoom pivot, and fit behavior;
* selection and area-probe state;
* the current loaded-image index within the shared library;
* windowed/fullscreen placement;
* the current `RendererTexture`.

If state is tied to one image pane and its navigation state, it usually
belongs here.

`ViewRecipe`
------------

`ViewRecipe` in `src/imiv/imiv_viewer.h` is the per-view preview/export recipe.
It currently owns the presentation settings that should travel with one image
view:

* exposure, gamma, and offset;
* interpolation mode;
* channel and color-mode selection;
* OCIO enable state;
* OCIO display, view, and image-color-space choices.

This is the current source of truth for per-view preview state. At runtime,
the active view's recipe is copied into the UI editing state before menus and
tool windows are drawn, then copied back into the active `ViewerState` after
UI edits are applied.

That mirror step is intentional. It keeps most existing Dear ImGui code
procedural while establishing one durable place for future `Save View As...`
or CPU-side export processing.

The first full view-recipe CPU export path is now `Export As...`.

It currently:

* reconstructs an oriented RGBA image from the loaded source pixels;
* applies the current view recipe for exposure, gamma, offset, channel/color
  display, and OCIO display/view state;
* writes the resulting oriented RGBA view image through OIIO.

That makes `ViewRecipe` an actual preview/export seam rather than only runtime
UI state.

`Export Selection As...` is the matching cropped variant. It uses the current
selection rectangle in source pixel space, builds a cropped source `ImageBuf`,
then runs the same view-recipe export path on that cropped image.

`Save Selection As...` remains intentionally narrower:

* it reconstructs an `ImageBuf` from the loaded source pixels;
* crops the selected ROI;
* bakes source orientation with `ImageBufAlgo::reorient()`;
* writes the result through OIIO.

That keeps one export path source-oriented and one export path view-oriented.

`ImageLibraryState` and `MultiViewWorkspace`
--------------------------------------------

The first multi-view slice introduces two more shared state buckets in
`src/imiv/imiv_viewer.h`:

* `ImageLibraryState`
  owns the shared loaded-image queue, recent-image history, and sort mode;
* `MultiViewWorkspace`
  owns the open image windows, the active view id, and Image List visibility.

Each `ImageViewWindow` owns one `ViewerState`. The main `Image` window is the
primary view. Additional `Image N` windows are created from
`File -> New view from current image` or by double-clicking entries in the
Image List window. Folder-open startup and `File -> Open Folder...` also feed
the same shared loaded-image library rather than creating a separate browsing
mode.

This split matters: queue history is now global to the workspace, but image
interaction state remains per view.

That same shared library path now covers startup multi-open, `Open Folder...`,
and multi-file drag/drop. There is one queue model, not separate browsing and
drop-import modes.

The `Image List` window now also exposes per-row workspace state rather than
being a passive history view:

* `>` marks the image shown in the active image view;
* `[N]` reports how many open image views currently show that path;
* a small inline close button appears for rows visible in the active view;
* the row popup menu routes the shared-library actions:
  `Open in active view`, `Open in new view`, `Close in active view`,
  `Close in all views`, and `Remove from session`.

Those actions are implemented against the shared `ImageLibraryState` plus the
current `MultiViewWorkspace`. `Close` mutates view bindings only. `Remove`
edits the shared session queue and then retargets or clears any views that
were showing the removed path.

Folder-open path filtering
--------------------------

The current folder-open path is intentionally cheap:

* it scans one directory non-recursively;
* it filters candidate files by the readable extension set reported by OIIO's
  plugin registry;
* it does not open every file just to decide whether it belongs in the queue.

That is the right default for folders containing many ordinary still images.
It is only a queue-building filter, not a guarantee that every accepted file
will decode successfully later.

`PlaceholderUiState`
--------------------

`PlaceholderUiState` is the persistent global UI configuration plus the
active-view editing mirror. It owns:

* visibility toggles for auxiliary windows;
* global presentation and app settings such as fit-to-window;
* global OCIO config-source selection and user-config path;
* saved backend preference for the next launch;
* docking policy state such as `image_window_force_dock`.

If state describes how the UI should look or how preview rendering should be
configured across launches for the application as a whole, it usually belongs
here.

Preview controls such as exposure, gamma, offset, interpolation, channel
selection, and OCIO display/view no longer live here as the source of truth.
They are mirrored into `PlaceholderUiState` only while editing the active
view, then written back to that view's `ViewRecipe`.

The focused `imiv_view_recipe_regression.py` regression exists specifically to
lock that behavior down: it opens multiple image views, edits one view's
recipe, switches active views, and verifies that the inactive view's recipe
state stays unchanged.

`DeveloperUiState`
------------------

`DeveloperUiState` is more temporary. It drives the runtime-controlled
`Developer` menu, its auxiliary Dear ImGui diagnostic windows, and delayed
actions such as the manual screenshot flow.

The effective developer-mode policy is resolved in `imiv_app.cpp`:

* Debug builds default to developer mode enabled;
* Release builds default to developer mode disabled;
* `OIIO_DEVMODE` overrides the build default when set to a boolean value;
* `--devmode` overrides both and forces developer mode on for that launch.

Combined settings file
----------------------

The current design intentionally stores application settings and Dear ImGui
layout state together in a single `imiv.inf` file.

`src/imiv/imiv_viewer.cpp` writes:

* the Dear ImGui `.ini` text first, straight from
  `ImGui::SaveIniSettingsToMemory()`;
* then an `ImivApp` settings section with `PlaceholderUiState`,
  the primary view's `ViewRecipe`, `ImageLibraryState`, and a small amount of
  `ViewerState`.

This is worth preserving. It gives :program:`imiv` one file for:

* dock/layout state;
* window placement;
* renderer preference;
* preview defaults from the primary view recipe;
* recent-image history.

The `IMIV_CONFIG_HOME` environment variable exists mainly for isolated local
repros and tests.


Frame composition
=================

`draw_viewer_ui()` in `src/imiv/imiv_frame.cpp` is the per-frame assembly
point. That function is intentionally procedural. It is easier to maintain than
spreading frame ownership across many windows.

The current order is:

1. reset per-frame test and layout-dump helpers;
2. apply any test-engine overrides;
3. collect keyboard shortcuts with `collect_viewer_shortcuts()`;
4. draw the main menu with `draw_viewer_main_menu()`;
5. queue any developer screenshot work;
6. execute queued state mutations with `execute_viewer_frame_actions()`;
7. process drag and drop and auto-subimage work;
8. sync the active view recipe into the UI mirror and clamp state;
9. build the dockspace host window;
10. draw the main image window and any secondary `Image N` windows, each with
    its own copied `ViewRecipe`;
11. draw the Image List window, auxiliary windows, and popups;
12. write any UI edits from the active view back into its `ViewRecipe`;
13. draw developer-mode Dear ImGui diagnostic windows and the drag overlay.

Two design choices here are important:

* visible state changes are not scattered through menu items and shortcut
  handlers;
* per-view preview texture updates happen inside the image-window loop using
  each view's own recipe, so the window code can stay renderer-neutral while
  still supporting independent view settings.


Docking, windows, and viewports
===============================

Dockspace host
--------------

The docking model lives in `begin_main_dockspace_host()` and
`setup_image_window_policy()` in `src/imiv/imiv_frame.cpp`.

The host window:

* covers the main viewport work area;
* removes title bar, borders, and padding;
* disables docking into the host itself;
* creates one root dockspace with `ImGui::DockSpace()`.

The main image window:

* is named `Image`;
* is assigned to the dockspace with `ImGui::SetNextWindowDockID()`;
* uses `ImGuiWindowClass` to request `ImGuiDockNodeFlags_AutoHideTabBar`.

Secondary image windows:

* are named `Image N`;
* are created as ordinary Dear ImGui windows with the same image-window class;
* are forced into the main dockspace on first creation;
* currently use `ImGuiDockNodeFlags_NoUndocking`;
* share the same renderer backend as the primary view, but keep their own
  `ViewerState` and `ViewRecipe`.

Initial dock layout policy
--------------------------

:program:`imiv` still avoids programmatic dock trees for most windows, but the
current multi-view slice makes one deliberate exception:

* if `Image List` becomes visible for a multi-file load and no saved layout for
  that window exists yet, `imiv` uses a small `DockBuilder` split to create a
  right-side pane of roughly 200 pixels;
* the primary `Image` window is docked into the remaining left-side node;
* later layout changes are still owned by Dear ImGui settings persistence.

This keeps the default presentation usable for multi-image loads without
turning the whole application into a hard-coded dock tree.

Window model
------------

The main windows are:

* the dockspace host window;
* the `Image` window;
* zero or more `Image N` windows;
* `Image List`;
* `iv Info`;
* `iv Preferences`;
* `Preview`;
* the `About imiv` modal;
* optional Dear ImGui diagnostics from the runtime-enabled `Developer` menu.

Auxiliary windows in `src/imiv/imiv_aux_windows.cpp` use
`ImGuiCond_FirstUseEver` defaults. After that, Dear ImGui layout persistence
takes over.

Multi-viewports
---------------

`run()` enables `ImGuiConfigFlags_ViewportsEnable`, but keeps the feature
conditional at runtime.

If the active GLFW platform backend or the active Dear ImGui renderer backend
does not report viewport support through `io.BackendFlags`,
:program:`imiv` disables detached auxiliary windows for that run and prints a
clear message. This is better than exposing a half-working multi-window path.


Menus, shortcuts, and actions
=============================

The menu and command path is split into three layers.

Collection layer
----------------

`src/imiv/imiv_menu.cpp` gathers user intent through:

* `collect_viewer_shortcuts()`;
* `draw_viewer_main_menu()`.

Both functions mostly write to `ViewerFrameActions` instead of mutating state
directly.

Execution layer
---------------

`execute_viewer_frame_actions()` applies those requests once per frame. This
keeps file I/O, texture uploads, fullscreen changes, and navigation changes
out of the immediate menu-building code.

Behavior layer
--------------

`src/imiv/imiv_actions.cpp` contains the actual operations:

* open/reload/close/save;
* navigation through image lists and subimages;
* fullscreen and slideshow actions;
* delete/replace/toggle operations;
* texture lifetime management around image reloads.

This split is worth keeping. It helps in three ways:

* menu code stays readable;
* state mutations happen in one place per frame;
* tests can assert frame actions and end state more easily.

When adding a feature, the normal path is:

1. collect intent in menu or shortcut code;
2. execute it in `execute_viewer_frame_actions()`;
3. put the durable behavior in `imiv_actions.cpp` or shared helpers.


Image window and helper modules
===============================

The image viewport is not a stock Dear ImGui widget. It is a small composed
system built from public Dear ImGui primitives.

Image window composition
------------------------

`draw_image_window_contents()` in `src/imiv/imiv_image_view.cpp` is the core
viewer widget.

It is responsible for:

* computing the viewport and status-bar split;
* handling fit-to-window and recenter requests;
* creating the scrollable child region with `ImGui::BeginChild()`;
* requesting renderer-neutral texture references with
  `renderer_get_viewer_texture_refs()`;
* defining the mouse interaction area with
  `ImGui::InvisibleButton("##image_canvas", ...)`;
* drawing the actual image with `ImDrawList::AddImage()`;
* updating probe, selection, area-sample, and zoom state.

Why this is built as a composition
----------------------------------

Dear ImGui does not ship a native image-viewer widget with the behaviors
:program:`imiv` needs. The current implementation keeps that custom behavior on
top of public API building blocks instead of introducing a private widget
framework.

Current custom compositions include:

* the image canvas itself:
  `BeginChild` + `InvisibleButton` + draw-list image rendering;
* preview mode buttons in `src/imiv/imiv_aux_windows.cpp`, which are regular
  `ImGui::Button()` calls with temporary styling;
* `input_text_string()`, a small wrapper that adapts `ImGui::InputText()` to
  `std::string`;
* overlay drawing in `src/imiv/imiv_overlays.cpp`, which uses draw-list
  primitives rather than custom Dear ImGui internals.

These are fine. The rule is not "never build a custom control." The rule is
"build it from public Dear ImGui pieces unless there is no practical option."

Navigation helpers
------------------

`src/imiv/imiv_navigation.cpp` keeps geometry and coordinate transforms out of
window code. It owns:

* orientation-aware source/display UV transforms;
* screen <-> source mapping;
* viewport layout and scrollbar calculations;
* zoom pivot and recenter behavior;
* fit zoom and scroll synchronization.

This separation matters because the same coordinate math is used by:

* image drawing;
* probe and selection logic;
* overlays;
* automated layout and interaction tests.

Overlay helpers
---------------

`src/imiv/imiv_overlays.cpp` draws:

* the pixel closeup;
* the area-probe overlay;
* the selection overlay;
* the embedded status bar.

It also registers synthetic layout-dump items when a visible element needs a
stable identifier for regression tests.

Small UI helpers
----------------

`src/imiv/imiv_ui.cpp` contains small, sharp helpers that would otherwise
clutter the window code. Examples include pixel sampling, padded status
messages, and formatting for probe output.


Renderer contract and backend rules
===================================

The shared code does not know about `VkImage`, `id<MTLTexture>`, or raw GL
object names. That is the main boundary to defend.

Shared renderer contract
------------------------

`src/imiv/imiv_renderer.h` defines the types shared UI code is allowed to use:

* `RendererState`
* `RendererTexture`
* `RendererPreviewControls`
* renderer lifecycle and frame functions

The viewer asks the backend for:

* a texture upload from a `LoadedImage`;
* preview refresh from the current preview controls;
* renderer-neutral texture references for Dear ImGui display;
* platform-window rendering and screen capture.

Everything else stays inside the backend.

Rules worth preserving
----------------------

* Shared headers should not expose backend-native types.
* UI code should not branch on backend internals unless the feature really is
  backend-specific.
* Unsupported behavior should fail or fall back explicitly.
* New shared renderer API should describe a concept every backend can
  understand.

Texture lifetime
----------------

`load_viewer_image()` in `src/imiv/imiv_actions.cpp` makes the texture
lifetime rule explicit:

* load CPU-side image data first;
* create the new backend texture;
* quiesce the old texture lifetime with `renderer_wait_idle()` if needed;
* destroy the old backend texture;
* swap in the new viewer state.

This is intentionally conservative. It avoids dangling GPU work during rapid
reloads and backend switches.


Backend rules
=============

The shared/backend split is an intentional design constraint, not an accident.

Rules that should continue to hold:

* Shared headers must not expose backend-native types such as `Vk*`, Metal
  Objective-C objects, or raw GL object identifiers.
* Backend-specific work belongs behind the renderer seam, not in general UI or
  viewer code.
* Platform-specific bootstrap work belongs in the GLFW platform layer, not in
  per-feature UI code.
* Backend gaps should fail or degrade explicitly rather than silently
  pretending to be supported.


Backend model and status
========================

Runtime selection is currently:

* platform: GLFW
* renderer: Vulkan, Metal, or OpenGL

Build-time selection is controlled by:

* `OIIO_IMIV_DEFAULT_RENDERER`
* `OIIO_IMIV_ENABLE_VULKAN`
* `OIIO_IMIV_ENABLE_METAL`
* `OIIO_IMIV_ENABLE_OPENGL`

The current development roles are:

* Vulkan:
  reference backend for renderer-side feature parity and regression coverage.
* Metal:
  native macOS backend using a Metal/MSL path and participating in the shared
  backend verifier.
* OpenGL:
  native GLSL, non-compute backend that also participates in the shared
  backend verifier rather than reusing the Vulkan compute/SPIR-V pipeline.

When a new feature requires renderer work, the safest default is to land it on
Vulkan first, then bring the other backends to matching behavior.


Preview pipeline and shader design
==================================

All backends follow the same high-level model:

1. OIIO loads source pixels into `LoadedImage`.
2. The backend owns a source texture representation.
3. The backend produces one or more preview textures from that source.
4. Shared UI code displays only the preview textures through Dear ImGui.

This keeps image loading, GPU upload, preview shading, and UI presentation
separate.

Why Vulkan normalizes upload to RGBA
------------------------------------

The Vulkan path in `src/imiv/imiv_vulkan_texture.cpp` uses the compute shader
`src/imiv/shaders/imiv_upload_to_rgba.comp` to convert the source image into a
normalized RGBA image on upload.

That choice is deliberate.

The compute upload path handles:

* 1, 2, 3, or 4+ channel inputs;
* integer, half, float, and optionally double source types;
* conversion to one backend-friendly sampled format.

The point is not only convenience. It simplifies the rest of the renderer:

* preview shaders sample one predictable RGBA source contract;
* channel-count branching happens once during upload instead of in every
  preview pass;
* later preview changes such as exposure, gamma, heatmap, orientation, and
  OCIO do not require re-uploading source pixels.

When double-precision input arrives and the fp64 compute pipeline is
unavailable, the Vulkan backend falls back to CPU conversion to float. That
fallback is explicit in `create_texture()`.

Why preview output is separate from the source texture
------------------------------------------------------

Vulkan keeps both:

* `source_image` for normalized uploaded data;
* `image` as the preview render target shown through Dear ImGui.

OpenGL and Metal follow the same idea in their own way:

* both keep a source texture;
* both render into separate preview textures rather than sampling the source
  texture directly in the UI path.

This is important because preview parameters change much more often than source
content.

Separate preview textures mean:

* exposure, gamma, offset, color-mode, orientation, and OCIO changes stay
  cheap;
* the UI always displays a ready-to-sample preview result;
* source upload and preview rendering can evolve independently.

Linear and nearest preview textures
-----------------------------------

OpenGL and Metal keep both linear and nearest preview textures. The Vulkan
path also exposes separate sampling behavior for the main view and closeup.

This is not duplication for its own sake. It lets :program:`imiv` choose:

* smoother filtered display for the main image view;
* nearest-neighbor display for pixel inspection and closeup.

That avoids re-rendering just to swap sampling behavior for inspection tools.

Static preview shader responsibilities
--------------------------------------

The static Vulkan preview shaders in `src/imiv/shaders/` are a useful summary
of what every backend preview path needs to do.

`imiv_preview.vert`
    builds the fullscreen triangle used to render the preview pass.

`imiv_preview.frag`
    handles display-to-source orientation mapping, color-mode switching,
    single-channel and luma display, heatmap display, exposure, gamma, and
    offset.

That shader also keeps the OCIO branch explicit with a TODO rather than
pretending the static shader path already applies OCIO. Actual OCIO preview is
provided through the runtime-generated backend-specific OCIO programs.

Backend-specific shader paths
-----------------------------

Vulkan
    uses build-time SPIR-V for the static upload and preview shaders from
    `src/imiv/shaders/`, with optional runtime shader compilation support for
    generated OCIO shaders.

OpenGL
    stays on native GLSL. It uploads supported source formats directly as GL
    textures, using native ``R/RG/RGB/RGBA`` uploads where possible, and
    renders preview textures through a small fullscreen-triangle program.

Metal
    stays on native MSL. It uploads typed source pixel data into a Metal
    buffer, runs a compute upload pass to normalize the source into the
    backend sampling format, and renders preview textures through a native
    Metal pipeline.

The OpenGL and Metal paths are intentionally not copies of the Vulkan compute
pipeline. That keeps each backend aligned with its native toolchain and makes
backend failures easier to diagnose.


Huge images and proxy recommendation
====================================

The current load path is still a full-image viewer path, not a true proxy or
tile-on-demand design.

Today, `read_image_file()` in `src/imiv/imiv_viewer.cpp` does create an
`ImageCache`, but it then reads the whole image into `LoadedImage`, builds
metadata/long-info rows from that loaded image, and the renderer uploads a
full source texture for the active backend.

Implications:

* `max_memory_ic_mb` is useful as an ImageCache tuning knob, but it does not
  make :program:`imiv` a true huge-image viewer;
* the current path is appropriate for ordinary still images and regression
  work, but it is not the right long-term design for very large plates,
  stitched panoramas, or other images that should stay sparse on the CPU and
  GPU.

Recommendation for future work:

* do not extend the current `LoadedImage -> full backend texture` path to
  chase huge-image support;
* introduce a dedicated proxy/tiled path instead, backed by OIIO
  `ImageCache`/ImageBuf proxy access or a similar sparse-image abstraction;
* keep `ViewRecipe` independent from that storage choice so the same per-view
  recipe can drive either full-image preview or a future tile/proxy backend;
* keep CPU export and `Save View As...` built on `ViewRecipe`, not on backend
  texture state.

That separation is the important design guardrail: image storage strategy for
huge files should change independently from per-view preview/export semantics.

OCIO integration
----------------

`src/imiv/imiv_ocio.cpp` is shared logic, not a backend implementation dump.

It handles:

* config-source selection and fallback;
* display/view/image-color-space resolution;
* target-specific shader text generation for Vulkan, OpenGL, and Metal.

At startup, `run()` preflights OCIO for the active backend. If the runtime
shader path is unavailable, :program:`imiv` disables OCIO preview for that run
and keeps the basic preview working.


Configuring the build
=====================

Important cache variables from `src/imiv/CMakeLists.txt` include:

* `OIIO_IMIV_IMGUI_ROOT`
  path to the Dear ImGui checkout used by :program:`imiv`.
* `OIIO_IMIV_TEST_ENGINE_ROOT`
  path to the Dear ImGui Test Engine checkout.
* `OIIO_IMIV_DEFAULT_RENDERER`
  runtime default backend (`auto`, `vulkan`, `metal`, `opengl`).
* `OIIO_IMIV_ENABLE_VULKAN`, `OIIO_IMIV_ENABLE_METAL`,
  `OIIO_IMIV_ENABLE_OPENGL`
  per-backend build switches (`AUTO`, `ON`, `OFF`).
* `OIIO_IMIV_ENABLE_IMGUI_TEST_ENGINE`
  compile test-engine support when sources are available.
* `OIIO_IMIV_USE_NATIVEFILEDIALOG`
  enable native file-open/save integration.
* `OIIO_IMIV_ADD_BACKEND_VERIFY_CTEST`
  add the longer per-backend verification CTest entries.

Common examples:

Linux or WSL multi-backend build::

    cmake -S . -B build \
      -D OIIO_IMIV_ENABLE_VULKAN=AUTO \
      -D OIIO_IMIV_ENABLE_OPENGL=AUTO \
      -D OIIO_IMIV_ENABLE_METAL=OFF \
      -D OIIO_IMIV_DEFAULT_RENDERER=vulkan \
      -D OIIO_IMIV_ENABLE_IMGUI_TEST_ENGINE=ON

macOS build with Metal default and OpenGL also compiled::

    cmake -S . -B build \
      -D OIIO_IMIV_ENABLE_METAL=AUTO \
      -D OIIO_IMIV_ENABLE_OPENGL=AUTO \
      -D OIIO_IMIV_ENABLE_VULKAN=AUTO \
      -D OIIO_IMIV_DEFAULT_RENDERER=metal \
      -D OIIO_IMIV_ENABLE_IMGUI_TEST_ENGINE=ON

At the time of this writing, the shared backend verifier is green on macOS for
all three compiled backends:

* Vulkan
* Metal
* OpenGL

The test-engine sources are discovered either from
`OIIO_IMIV_TEST_ENGINE_ROOT` or from the `IMGUI_TEST_ENGINE_ROOT`
environment variable.


Runtime selection and preferences
=================================

The launch-time backend resolution order is:

1. the `--backend` command-line option, if supplied;
2. the saved `renderer_backend` preference from `imiv.inf`;
3. `auto`, which resolves to the first compiled backend available to the
   current binary.

Backend preference changes made in the Preferences window are persistent but
take effect on the next launch. This is intentional and should remain true for
all backends.

For isolated local repros and tests, set `IMIV_CONFIG_HOME` so preference
changes do not bleed into your normal user config.


Dear ImGui API policy
=====================

Production :program:`imiv` code is written against Dear ImGui public API first.
That is an explicit maintenance choice.

Public Dear ImGui API used throughout `imiv`
--------------------------------------------

Common examples in the current code are:

* context and frame lifecycle:
  `ImGui::CreateContext()`, `ImGui::NewFrame()`, `ImGui::Render()`;
* docking and platform windows:
  `ImGui::DockSpace()`, `ImGui::SetNextWindowDockID()`,
  `ImGui::UpdatePlatformWindows()`,
  `ImGui::RenderPlatformWindowsDefault()`;
* settings persistence:
  `ImGui::LoadIniSettingsFromDisk()`,
  `ImGui::SaveIniSettingsToMemory()`;
* standard windows and widgets:
  `ImGui::Begin()`, `ImGui::End()`, `ImGui::BeginChild()`,
  `ImGui::BeginTable()`, `ImGui::BeginMainMenuBar()`,
  `ImGui::BeginPopupModal()`, `ImGui::Button()`,
  `ImGui::Checkbox()`, `ImGui::InputText()`;
* interaction helpers:
  `ImGui::Shortcut()`, `ImGui::InvisibleButton()`;
* draw-list rendering:
  `ImDrawList::AddImage()`.

Current production `src/imiv` sources still use Dear ImGui public API first.
There is now one explicit exception in `imiv_frame.cpp` for the initial
`Image List` dock split, which uses `imgui_internal.h` `DockBuilder` helpers.
That exception should stay narrow.

Maintenance-sensitive integrations
----------------------------------

Some parts of :program:`imiv` still depend on APIs or helper layers that are
outside the plain `imgui.h` application surface. These are valid choices, but
they need extra care when updating Dear ImGui.

Vulkan backend helper types
^^^^^^^^^^^^^^^^^^^^^^^^^^^

The Vulkan backend uses `ImGui_ImplVulkanH_Window` and related
`ImGui_ImplVulkanH_*` helpers from the Dear ImGui Vulkan backend support code.

Files using this include:

* `src/imiv/imiv_types.h`
* `src/imiv/imiv_vulkan_runtime.cpp`
* `src/imiv/imiv_vulkan_window.cpp`
* `src/imiv/imiv_capture.cpp`

These helpers are practical, but they are not the same stability level as the
core public UI API. When updating Dear ImGui, check them first.

Local Metal backend fork
^^^^^^^^^^^^^^^^^^^^^^^^

`src/imiv/external/imgui_impl_metal_imiv.mm` is an intentional local fork of
the Dear ImGui Metal backend.

It adds:

* `ImGui_ImplMetal_CreateUserTextureID()`
* `ImGui_ImplMetal_DestroyUserTextureID()`

That extension exists because :program:`imiv` needs per-texture sampler choice
for the linear and nearest preview textures. This is a reasonable design, but
it is not upstream vanilla Dear ImGui backend code. Treat it as local
maintenance surface and re-check it whenever upstream Metal backend code
changes.

Dear ImGui Test Engine internals
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

`src/imiv/imiv_test_engine.cpp` integrates Dear ImGui Test Engine and uses
test-engine hooks and types such as:

* `ImGuiContext*`
* `ImGuiWindow*`
* `ImGuiTestEngineHook_ItemAdd()`

This is expected for test-engine integration, but it should stay isolated to
the test layer. Do not spread test-engine internal assumptions through normal
viewer code.

Practical rule
--------------

If a new feature can be built with `imgui.h` and the normal backend interfaces,
do that. If it cannot, isolate the non-public dependency in one file and
document why it exists.


Adding or changing UI
=====================

The easiest way to make :program:`imiv` harder to maintain is to mix viewer
state, renderer details, and automation plumbing in one patch.

The safer workflow is:

1. Put new durable state in `ViewerState` or `PlaceholderUiState`.
2. Implement the behavior change in shared actions/navigation code unless it
   truly requires renderer work.
3. Expose the feature through menu/UI code.
4. Add regression visibility through state dumps, layout markers, or both.

Practical guidance:

* Keep shortcut handling in sync with menu actions.
* Keep the `ViewerFrameActions` split intact instead of mutating durable state
  directly inside menu drawing code.
* Prefer small helper wrappers over ad hoc widget copies when the same UI
  pattern appears in several places.
* Preserve current behavior across backends unless the limitation is
  intentional and documented.
* When a visible element matters to automated layout dumps, add a synthetic
  marker with `register_layout_dump_synthetic_item()` or
  `register_layout_dump_synthetic_rect()`.
* When a behavior change is better asserted structurally than visually, extend
  the viewer-state JSON written from `imiv_frame.cpp`.
* If a control needs custom drawing, build it from public Dear ImGui pieces
  first. The image canvas model in `imiv_image_view.cpp` is the current
  pattern to follow.


Adding renderer work
====================

If a feature needs backend-specific implementation, treat `imiv_renderer.h`
and `imiv_renderer_backend.h` as the contract boundary.

Recommendations:

* Add shared renderer API only when the concept belongs in all backends.
* Keep unsupported behavior explicit; do not silently emulate a Vulkan-only
  assumption in shared code.
* Do not push backend-native types upward into viewer/UI layers.
* Match Vulkan behavior on other backends rather than inventing divergent UI
  semantics.
* Keep the source-texture/preview-texture split unless there is a strong,
  measured reason to change it.
* Treat linear-vs-nearest inspection behavior as user-facing functionality,
  not an implementation accident.

Backend-specific constraints worth preserving:

* OpenGL should remain a native GLSL, non-compute path.
* Metal should stay on a native Metal/MSL path.
* Vulkan remains the primary reference path for compute upload, runtime shader
  compilation, and parity-driven regression work.


Debugging and local development
===============================

Useful local tools and switches:

* `imiv -v image.exr`
  prints verbose startup and backend-selection logs.
* `IMIV_VULKAN_VERBOSE_VALIDATION=1`
  enables noisier Vulkan validation logging.
* `IMIV_DEBUG_IMGUI_TEXTURES=1`
  helps debug Dear ImGui texture update behavior.
* Debug builds expose the `Developer` menu, including Dear ImGui diagnostic
  windows and a manual main-window capture action on `F12`.
* `IMIV_CONFIG_HOME=/tmp/imiv-dev`
  isolates config, layout, and backend preference changes during local
  debugging and test authoring.

For broader regression coverage across compiled backends, see the
:doc:`test-suite guide <imiv_tests>`.
