# dnd_glfw – GLFW Drag-and-Drop Helper

`dnd_glfw` is a very small helper around GLFW that exposes a simple, C‑style drag‑and‑drop callback interface. It is designed for applications that already use GLFW for their main window (e.g. Dear ImGui with the GLFW/OpenGL backend) and want better control over file drops and drag‑hover overlays.

The implementation is intentionally minimal and C‑style C++: no heavy abstractions, only basic C++17 containers and RAII where needed.

## Supported platforms

- **Windows**
  - Uses the GLFW window handle (`HWND`) and registers an OLE `IDropTarget`.
  - Provides real OS‑level events for file drags:
    - `dragEnter`, `dragOver`, `dragLeave`, `dragCancel`.
  - File paths are still obtained via GLFW’s existing drop callback semantics and forwarded to your `drop` handler.

- **macOS**
  - Uses Cocoa drag‑and‑drop via an `NSView<NSDraggingDestination>` overlay on the GLFW window’s content view.
  - Provides OS‑level `dragEntered`, `draggingUpdated`, `draggingExited`, and `performDragOperation` mapped to:
    - `dragEnter`, `dragOver`, `dragLeave`, `dragCancel`, `drop`.
  - File paths are read from the pasteboard and passed to your `drop` handler as UTF‑8 strings.

- **Linux / X11 / Wayland**
  - Relies on GLFW’s internal Xdnd / DnD handling.
  - Wraps `glfwSetDropCallback` and exposes file paths and cursor position via the `drop` callback.
  - OS‑level drag‑enter / drag‑hover / drag‑leave for external file drags are **not** available via GLFW’s public API, so `dragEnter` / `dragOver` / `dragLeave` / `dragCancel` are not fired for system file drags on Linux.

In practice this means:

- You get full overlay‑style drag feedback on Windows and macOS.
- On Linux you still get correct file drops with a consistent API, but no OS‑level “drag in progress” notifications.

## API overview

Header: `dnd_glfw/dnd_glfw.h`

```cpp
namespace dnd_glfw {

enum class PayloadKind {
    Unknown = 0,
    Files   = 1
};

struct DragEvent {
    double x;
    double y;
    PayloadKind kind;
};

struct DropEvent {
    double x;
    double y;
    PayloadKind kind;
    std::vector<std::string> paths; // UTF‑8 file paths
};

struct Callbacks {
    void (*dragEnter)(GLFWwindow*, const DragEvent&, void* userData);
    void (*dragOver)(GLFWwindow*, const DragEvent&, void* userData);
    void (*dragLeave)(GLFWwindow*, void* userData);
    void (*drop)(GLFWwindow*, const DropEvent&, void* userData);
    void (*dragCancel)(GLFWwindow*, void* userData);
};

bool init(GLFWwindow* window, const Callbacks& callbacks, void* userData);
void shutdown(GLFWwindow* window);

} // namespace dnd_glfw
```

All callbacks are optional; set a pointer to `nullptr` to ignore that event type.

## Typical usage

1. **Create your GLFW window as usual** and make it current.
2. **Register drag‑and‑drop callbacks** once, after the window exists:

```cpp
static void onDragEnter(GLFWwindow* window, const dnd_glfw::DragEvent& e, void* user)
{
    (void)window;
    AppState* app = static_cast<AppState*>(user);
    if (!app) return;

    if (e.kind == dnd_glfw::PayloadKind::Files) {
        app->dragOverlayActive = true;
    }
}

static void onDrop(GLFWwindow* window, const dnd_glfw::DropEvent& e, void* user)
{
    (void)window;
    AppState* app = static_cast<AppState*>(user);
    if (!app) return;

    std::lock_guard<std::mutex> lock(app->dropMutex);
    app->pendingDrops = e.paths;
    app->dragOverlayActive = false;
}

static void onDragLeave(GLFWwindow* window, void* user)
{
    (void)window;
    AppState* app = static_cast<AppState*>(user);
    if (!app) return;
    app->dragOverlayActive = false;
}

// ...

dnd_glfw::Callbacks cbs{};
cbs.dragEnter  = &onDragEnter;
cbs.dragOver   = nullptr;      // not needed for simple overlay
cbs.dragLeave  = &onDragLeave;
cbs.drop       = &onDrop;
cbs.dragCancel = &onDragLeave; // treat cancel as leave

dnd_glfw::init(window, cbs, &app);
```

3. **Use your own flag to drive an overlay** in ImGui (or another GUI):

```cpp
if (app.dragOverlayActive) {
    // Draw a full‑window dim background and center text
}
```

4. **On shutdown**, before destroying the GLFW window:

```cpp
dnd_glfw::shutdown(window);
glfwDestroyWindow(window);
```

## Integration details

- **GLFW drop callback chaining**
  - On Windows and Linux, `dnd_glfw::init` installs an internal `glfwSetDropCallback` handler and remembers any previously registered callback.
  - When a drop occurs, the internal handler:
    - Builds a `DropEvent` (cursor position, `PayloadKind::Files`, file paths).
    - Calls your `Callbacks::drop` (if provided).
    - Forwards the event to the previous GLFW drop callback (if any).
  - On macOS, GLFW’s own DnD is handled internally; `dnd_glfw` uses Cocoa APIs instead and does **not** override `glfwSetDropCallback`.

- **Windows / OLE**
  - Uses `glfwGetWin32Window(window)` to get the `HWND` and registers an `IDropTarget` implementation.
  - Only CF_HDROP (file paths) is accepted; other payloads are ignored.
  - `DragEnter` / `DragOver` use client‑area coordinates (converted from screen) for the `DragEvent::x`/`y` fields.
  - The actual file paths are still delivered via GLFW’s drop events to keep behavior consistent with GLFW/X11 and Cocoa.

- **macOS / Cocoa**
  - Obtains the native `NSWindow*` for the GLFW window with `glfwGetCocoaWindow`.
  - Adds a transparent `NSView` subclass (`DndGlfwDragView`) above the content view and registers it for file URL types.
  - Uses the system pasteboard to extract file URLs and forwards them as UTF‑8 paths in `DropEvent::paths`.

- **Linux**
  - Depends on GLFW’s own X11/Wayland drag‑and‑drop implementation.
  - Only the `drop` callback is meaningful for OS‑level file drags; hover/enter/leave for external drags are not exposed by GLFW and therefore not emitted by `dnd_glfw`.

## CMake integration

The top‑level `CMakeLists.txt` for this project adds the helper as a static library:

```cmake
if(APPLE)
    add_library(dnd_glfw STATIC
        dnd_glfw_macos.mm
    )
else()
    add_library(dnd_glfw STATIC
        dnd_glfw_dummy.cpp
    )
endif()

target_include_directories(dnd_glfw
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(APPLE)
    target_link_libraries(dnd_glfw PUBLIC "-framework Cocoa")
endif()
```

To use it from another target (like the GUI):

```cmake
add_subdirectory(dnd_glfw)

target_link_libraries(meshrepair_gui
    PRIVATE
        dnd_glfw
        # other libs: glfw, OpenGL, ImGui, etc.
)

if(WIN32)
    target_link_libraries(meshrepair_gui PRIVATE ole32)
endif()
```

The macOS Cocoa framework and Windows `ole32` import library are wired automatically via these targets.

## Notes and limitations

- Only a small subset of drag‑and‑drop is implemented: **file paths** dragged from the OS (Explorer/Finder/desktop/file manager) into your GLFW window.
- No attempt is made to support arbitrary MIME types or cross‑process data beyond file URLs / paths.
- On Linux, GLFW owns the Xdnd handling; `dnd_glfw` intentionally does **not** duplicate Xdnd parsing to avoid conflicting with GLFW’s internal event loop.
- The internal state is stored in a small global vector keyed by `GLFWwindow*`. It assumes a small number of windows (typically one) and is not optimized for hundreds of windows. This matches the intended usage for a single ImGui main window. 
