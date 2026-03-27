..
  Copyright Contributors to the OpenImageIO project.
  SPDX-License-Identifier: CC-BY-4.0


.. _chap-imiv-user:

`imiv` User Guide
#################

.. highlight:: bash


Overview
========

:program:`imiv` is an interactive image viewer aimed at modernizing
:program:`iv` on top of Dear ImGui. The current build already supports:

* opening one or more images at startup;
* switching between compiled renderer backends;
* navigation, zoom, channel and color-mode controls;
* OCIO display/view selection;
* pixel closeup and Area Sample inspection tools;
* persistent preferences and recent-file history.

The user-facing workflow is intentionally still smaller than the developer and
test material, because not every historical :program:`iv` feature is wired up
yet.

Current multi-backend builds can include Vulkan, Metal, and OpenGL in the same
binary. On supported platforms, the active backend may be selected per-launch
with ``--backend`` or persistently from the Preferences window.


Using `imiv`
============

The :program:`imiv` utility is invoked as follows:

    `imiv` *options* *filename* ...

Any filenames listed on the command line are queued as the initial loaded
image list. For example::

    imiv frame.exr

    imiv shotA.exr shotB.exr shotC.exr

    imiv --backend vulkan frame.exr

    imiv --display "sRGB - Display" \
         --view "ACES 2.0 - SDR 100 nits (Rec.709)" \
         --image-color-space ACEScg image.exr

To see which renderer backends were compiled into the current binary and which
of those are currently usable on this machine::

    imiv --list-backends

Use :program:`imiv --help` to print the option summary for the current build.


`imiv` command-line options
===========================

.. option:: -v

    Enable verbose startup logging, including backend selection and dependency
    information.

.. option:: -F

    Run in foreground mode. This is primarily useful for debugging and
    automation.

.. option:: --backend BACKEND

    Request a renderer backend at launch time. Valid values are `auto`,
    `vulkan`, `metal`, and `opengl`.

    The command-line request takes precedence over the saved backend
    preference. If the requested backend was not compiled into the current
    binary, or was compiled but is not currently available at runtime,
    :program:`imiv` falls back to the resolved default backend and prints a
    message.

.. option:: --list-backends

    Print the backend support compiled into the current :program:`imiv`
    binary, including runtime availability and any unavailability reason, then
    exit.

.. option:: --devmode

    Enable the `Developer` menu and its auxiliary Dear ImGui diagnostic tools
    for this launch.

    In Debug builds, developer mode is enabled by default. In Release builds,
    it is disabled by default unless requested explicitly.

.. option:: --display NAME

    Set the initial OCIO display selection.

.. option:: --view NAME

    Set the initial OCIO view selection.

.. option:: --image-color-space NAME

    Set the initial OCIO image color-space selection.

.. option:: --rawcolor

    Disable automatic conversion to RGB on image load.

.. option:: --no-autopremult

    Disable the automatic premultiplication path used for images with
    unassociated alpha.

.. option:: --open-dialog

    Open the native file-open dialog and report the result to the terminal.
    This is also a quick way to verify whether native file dialog support is
    configured in the current build.

.. option:: --save-dialog

    Open the native file-save dialog and report the result to the terminal.


Opening and browsing images
===========================

Images may be opened in several ways:

* by listing files on the command line;
* through `File -> Open...`;
* by drag-and-drop onto the main window;
* through `File -> Open recent...`.

:program:`imiv` keeps an explicit loaded-image list rather than relying only on
directory siblings. At the moment, the list deduplicates paths after
normalization, so repeated selection of the same file is not treated as a
separate loaded image entry.

Useful browsing actions:

* `PgUp` / `PgDown` moves to the previous or next loaded image.
* `T` toggles between the current and previously viewed image.
* `<` / `>` moves between subimages and mip levels where available.
* `Ctrl+R` reloads the current image from disk.
* `Ctrl+W` closes the current image.


Exporting images
================

The current export actions are still smaller than :program:`iv`, but
:program:`imiv` now has three real GUI-driven CPU export paths:

* `File -> Export As...`
  writes the current image pane as an oriented RGBA view export;
* `Ctrl+Shift+S`
  invokes the same action directly;

* `File -> Save Selection As...`
  writes the current pixel selection to a new file;
* `Ctrl+Alt+S`
  invokes the same action directly.
* `File -> Export Selection As...`
  writes the selected pixel region as an oriented RGBA view export;
* `Ctrl+Shift+Alt+S`
  invokes the same action directly.

`Export As...` currently exports:

* the current image with orientation baked to the saved output;
* the active view recipe for exposure, gamma, offset, channel/color display,
  and OCIO display/view if OCIO is enabled.

It is intentionally a view export, not a source-preserving rewrite. The saved
image is written as an oriented RGBA result and may differ in channel count and
numeric type from the original source image.

`Save Selection As...` currently exports:

* the selected pixel rectangle from the loaded source image; and
* the image with its stored orientation baked to the saved output.

It does not yet bake the full per-view recipe. In particular, exposure, gamma,
offset, channel/color display choices, and OCIO display/view state are still
preview-only at export time.

`Export Selection As...` currently exports:

* the selected pixel rectangle from the current image;
* the image with its stored orientation baked to the saved output; and
* the active view recipe for exposure, gamma, offset, channel/color display,
  and OCIO display/view if OCIO is enabled.


Viewing, navigation, and inspection
===================================

The main viewing controls are centered in the `View` and `Tools` menus.

Common shortcuts:

* `Ctrl++`, `Ctrl+-`, `Ctrl+0` zoom in, zoom out, and return to 1:1.
* `Ctrl+.` re-centers the image.
* `F` fits the window to the image; `Alt+F` fits the image to the window.
* `Ctrl+F` toggles fullscreen mode.
* `C`, `R`, `G`, `B`, `A`, `1`, `L`, and `H` switch channel and color modes.
* `[`, `]`, `{`, `}`, `(`, and `)` adjust exposure and gamma.
* `Ctrl+I` opens the image information window.
* `P` opens the pixel closeup view.
* `Ctrl+A` toggles Area Sample.
* `Ctrl+Shift+A` selects the whole image; `Ctrl+D` clears the current
  selection.

The `Tools` menu also exposes slideshow, sort-order, and orientation actions.


Multiple views and image list
=============================

The current multi-view workflow is centered on shared loaded-image history and
per-window image panes.

Useful actions:

* `File -> Open Folder...`
  scans one directory and queues the supported image files it contains.
* `File -> New view from current image`
  duplicates the current image into a new `Image N` window.
* `View -> Image List`
  opens a dockable window showing the current loaded-image queue.

`Open Folder...` is currently a non-recursive scan. It uses the readable file
extensions reported by the OpenImageIO plugin registry as a cheap prefilter,
so large flat folders do not need to be probed by opening every file just to
build the queue.

The `Image List` window currently supports:

* single-click
  load the chosen image into the active image view;
* double-click
  open the chosen image in a new image view window.
* active-view marker
  `>` marks the image currently shown in the active image window;
* open-count badge
  `[N]` shows how many open image views currently display that image;
* inline close button
  rows that are visible in the active image view show a small `x` button;
* right-click context menu
  provides `Open in active view`, `Open in new view`,
  `Close in active view`, `Close in all views`, and
  `Remove from session`.

`Close` and `Remove` are intentionally different:

* `Close in active view`
  only clears that image from the active image window;
* `Close in all views`
  clears it from every currently open image window;
* `Remove from session`
  removes it from the shared loaded-image queue without touching the file on
  disk.

The main `Image` window remains the primary docked image pane. Additional
`Image N` windows are currently created docked into the main dockspace.
Undocking those image views is intentionally disabled in this first slice.

When more than one image is loaded into the queue, `Image List` becomes
visible automatically and defaults to a narrow docked pane on the right side
of the main image area. Its dock position and size may still be saved by Dear
ImGui layout persistence, but its open/closed visibility is not treated as a
persistent preference.

This is the first multi-view milestone. View windows already have independent
loaded images, zoom, scroll, selection state, and preview recipe state.
Exposure, gamma, offset, interpolation, channel/color display, and OCIO
display/view/image-color-space choices are now stored per window at runtime.

The current persistence model is still simpler than the runtime model:

* :program:`imiv` saves the primary/default view recipe in `imiv.inf`;
* it does not yet persist the full multi-window workspace or every secondary
  view's recipe across launches.

Multi-file drag and drop feeds the same shared loaded-image queue as startup
multi-open and `Open Folder...`. That means dropped files immediately appear
in `Image List` and participate in the same per-view open/close/remove
workflow.


Color management
================

OCIO controls are available from `View -> OCIO` and from the preferences
window.

The current implementation supports:

* enabling or disabling OCIO preview;
* choosing the image color space;
* choosing the display and view;
* selecting the OCIO config source from the saved preferences;
* resolving `auto` image color space from image metadata when possible.

If the requested OCIO configuration is unavailable, :program:`imiv` falls back
according to the current config-source rules and reports the resolved state in
automation/state-dump output.


Preferences and persistent state
================================

:program:`imiv` saves both Dear ImGui window layout data and application state
to an `imiv.inf` file. The file location is:

* Windows: `%APPDATA%/OpenImageIO/imiv/imiv.inf` (or `%LOCALAPPDATA%/...`)
* macOS: `~/Library/Application Support/OpenImageIO/imiv/imiv.inf`
* Linux and other Unix-like platforms:
  `"$XDG_CONFIG_HOME"/OpenImageIO/imiv/imiv.inf` or
  `~/.config/OpenImageIO/imiv/imiv.inf`

For automation or isolated local experiments, the base config directory may be
overridden with the `IMIV_CONFIG_HOME` environment variable. In that case,
:program:`imiv` stores its files under:

    `"$IMIV_CONFIG_HOME"/OpenImageIO/imiv/imiv.inf`

Saved state currently includes:

* Dear ImGui docking/window layout;
* viewer and preview defaults for the primary view;
* backend preference (`renderer_backend`);
* OCIO settings;
* recent images and sort mode.

Example::

    [ImivApp][State]
    renderer_backend=vulkan
    fit_image_to_window=1
    use_ocio=1
    ocio_display=default
    ocio_view=default
    recent_image=/shots/plate.0001.exr

The Preferences window exposes backend selection as equal-width buttons for
the compiled backends, plus ``Auto``. Changing the backend preference updates
the next-launch backend and shows a restart-required note. The active backend
for the current process does not change until the next launch.

Developer mode may also be controlled by the ``OIIO_DEVMODE`` environment
variable. Acceptable values include ``1``, ``0``, ``true``, ``false``, ``on``,
``off``, ``yes``, and ``no``. The command-line ``--devmode`` flag takes
precedence over the environment variable.


Current limitations
===================

At the time of this writing, notable differences from :program:`iv` include:

* Fullscreen behavior does not yet exactly match :program:`iv`.
* Opening the same file repeatedly does not currently create duplicate loaded
  image entries.
* Multi-view image windows have per-view preview controls at runtime, but
  :program:`imiv` does not yet persist a full multi-window workspace across
  launches.
* Some older :program:`iv` mouse modes are not yet a priority for the Dear
  ImGui port.

For the current backend matrix and development priorities, see the
:doc:`developer guide <imiv_dev>`.
