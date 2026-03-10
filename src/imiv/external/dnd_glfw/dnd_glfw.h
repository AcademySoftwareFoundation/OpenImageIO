// Lightweight GLFW-based drag-and-drop helper
// Header-only; C-style callbacks with minimal C++17 usage.

#pragma once

#include <GLFW/glfw3.h>

#include <vector>
#include <string>

#if defined(_WIN32)
#    include <windows.h>
#    include <ole2.h>
#    include <shellapi.h>
#    if !defined(GLFW_EXPOSE_NATIVE_WIN32)
#        define GLFW_EXPOSE_NATIVE_WIN32
#    endif
#    include <GLFW/glfw3native.h>
#endif

#if defined(__APPLE__) && !defined(_WIN32)
#    if !defined(GLFW_EXPOSE_NATIVE_COCOA)
#        define GLFW_EXPOSE_NATIVE_COCOA
#    endif
#    include <GLFW/glfw3native.h>
#endif

namespace dnd_glfw {

enum class PayloadKind {
    Unknown = 0,
    Files   = 1
};

struct DragEvent {
    double x   = 0.0;
    double y   = 0.0;
    PayloadKind kind = PayloadKind::Unknown;
};

struct DropEvent {
    double x   = 0.0;
    double y   = 0.0;
    PayloadKind kind = PayloadKind::Unknown;
    std::vector<std::string> paths;
};

struct Callbacks {
    void (*dragEnter)(GLFWwindow* window, const DragEvent& event, void* userData) = nullptr;
    void (*dragOver)(GLFWwindow* window, const DragEvent& event, void* userData)  = nullptr;
    void (*dragLeave)(GLFWwindow* window, void* userData)                         = nullptr;
    void (*drop)(GLFWwindow* window, const DropEvent& event, void* userData)      = nullptr;
    void (*dragCancel)(GLFWwindow* window, void* userData)                        = nullptr;
};

namespace detail {

struct WindowState {
    GLFWwindow* window      = nullptr;
    Callbacks callbacks     = {};
    void* userData          = nullptr;
    GLFWdropfun prevDropCb  = nullptr;
    bool dropCallbackInstalled = false;

#if defined(_WIN32)
    HWND hwnd               = nullptr;
    IDropTarget* dropTarget = nullptr;
#endif

#if defined(__APPLE__) && !defined(_WIN32)
    void* cocoaView         = nullptr;
#endif
};

inline std::vector<WindowState>&
windowStates()
{
    static std::vector<WindowState> states;
    return states;
}

inline WindowState*
findState(GLFWwindow* window)
{
    auto& states = windowStates();
    for (auto& s : states) {
        if (s.window == window) {
            return &s;
        }
    }
    return nullptr;
}

inline WindowState*
addState(GLFWwindow* window)
{
    auto& states = windowStates();
    states.push_back(WindowState {});
    WindowState& s = states.back();
    s.window       = window;
    return &s;
}

inline void
removeState(GLFWwindow* window)
{
    auto& states = windowStates();
    for (std::size_t i = 0; i < states.size(); ++i) {
        if (states[i].window == window) {
            states[i] = states.back();
            states.pop_back();
            break;
        }
    }
}

inline void
dispatchDragEnter(WindowState* state, double x, double y, PayloadKind kind)
{
    if (!state || !state->callbacks.dragEnter) {
        return;
    }
    DragEvent ev {};
    ev.x    = x;
    ev.y    = y;
    ev.kind = kind;
    state->callbacks.dragEnter(state->window, ev, state->userData);
}

inline void
dispatchDragOver(WindowState* state, double x, double y, PayloadKind kind)
{
    if (!state || !state->callbacks.dragOver) {
        return;
    }
    DragEvent ev {};
    ev.x    = x;
    ev.y    = y;
    ev.kind = kind;
    state->callbacks.dragOver(state->window, ev, state->userData);
}

inline void
dispatchDragLeave(WindowState* state)
{
    if (!state || !state->callbacks.dragLeave) {
        return;
    }
    state->callbacks.dragLeave(state->window, state->userData);
}

inline void
dispatchDragCancel(WindowState* state)
{
    if (!state || !state->callbacks.dragCancel) {
        return;
    }
    state->callbacks.dragCancel(state->window, state->userData);
}

inline void
dispatchDrop(WindowState* state, const DropEvent& ev)
{
    if (!state || !state->callbacks.drop) {
        return;
    }
    state->callbacks.drop(state->window, ev, state->userData);
}

inline void
glfwDropCallback(GLFWwindow* window, int count, const char** paths);

#if defined(__APPLE__) && !defined(_WIN32)
void platformInitMac(WindowState* state, GLFWwindow* window);
void platformShutdownMac(WindowState* state, GLFWwindow* window);
#endif

#if defined(_WIN32)

struct DropTarget : public IDropTarget {
    LONG refCount;
    WindowState* state;

    explicit DropTarget(WindowState* s)
        : refCount(1)
        , state(s)
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
    {
        if (!ppvObject) {
            return E_INVALIDARG;
        }
        if (riid == IID_IUnknown || riid == IID_IDropTarget) {
            *ppvObject = static_cast<IDropTarget*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return static_cast<ULONG>(InterlockedIncrement(&refCount));
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        LONG count = InterlockedDecrement(&refCount);
        if (count == 0) {
            delete this;
            return 0;
        }
        return static_cast<ULONG>(count);
    }

    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* dataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override
    {
        (void)grfKeyState;

        if (!pdwEffect) {
            return E_INVALIDARG;
        }

        if (!state) {
            *pdwEffect = DROPEFFECT_NONE;
            return S_OK;
        }

        FORMATETC fmt {};
        fmt.cfFormat = CF_HDROP;
        fmt.ptd      = nullptr;
        fmt.dwAspect = DVASPECT_CONTENT;
        fmt.lindex   = -1;
        fmt.tymed    = TYMED_HGLOBAL;

        if (!dataObj || FAILED(dataObj->QueryGetData(&fmt))) {
            *pdwEffect = DROPEFFECT_NONE;
            return S_OK;
        }

        POINT screenPoint;
        screenPoint.x = static_cast<LONG>(pt.x);
        screenPoint.y = static_cast<LONG>(pt.y);

        POINT clientPoint = screenPoint;
        if (state->hwnd) {
            ScreenToClient(state->hwnd, &clientPoint);
        }

        double x = static_cast<double>(clientPoint.x);
        double y = static_cast<double>(clientPoint.y);
        dispatchDragEnter(state, x, y, PayloadKind::Files);

        *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override
    {
        (void)grfKeyState;

        if (!pdwEffect) {
            return E_INVALIDARG;
        }

        if (!state) {
            *pdwEffect = DROPEFFECT_NONE;
            return S_OK;
        }

        POINT screenPoint;
        screenPoint.x = static_cast<LONG>(pt.x);
        screenPoint.y = static_cast<LONG>(pt.y);

        POINT clientPoint = screenPoint;
        if (state->hwnd) {
            ScreenToClient(state->hwnd, &clientPoint);
        }

        double x = static_cast<double>(clientPoint.x);
        double y = static_cast<double>(clientPoint.y);
        dispatchDragOver(state, x, y, PayloadKind::Files);

        *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DragLeave() override
    {
        if (state) {
            dispatchDragLeave(state);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Drop(IDataObject* dataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override
    {
        (void)grfKeyState;
        (void)pt;

        if (pdwEffect) {
            *pdwEffect = DROPEFFECT_NONE;
        }

        if (!state || !dataObj) {
            return S_OK;
        }

        FORMATETC fmt {};
        fmt.cfFormat = CF_HDROP;
        fmt.ptd      = nullptr;
        fmt.dwAspect = DVASPECT_CONTENT;
        fmt.lindex   = -1;
        fmt.tymed    = TYMED_HGLOBAL;

        STGMEDIUM medium {};
        if (FAILED(dataObj->GetData(&fmt, &medium))) {
            dispatchDragCancel(state);
            return S_OK;
        }

        HDROP hdrop = static_cast<HDROP>(GlobalLock(medium.hGlobal));
        if (!hdrop) {
            ReleaseStgMedium(&medium);
            dispatchDragCancel(state);
            return S_OK;
        }

        UINT fileCount = DragQueryFileW(hdrop, 0xFFFFFFFFu, nullptr, 0);
        dnd_glfw::DropEvent ev;
        ev.kind = PayloadKind::Files;

        POINT clientPt {};
        clientPt.x = static_cast<LONG>(pt.x);
        clientPt.y = static_cast<LONG>(pt.y);
        if (state->hwnd) {
            ScreenToClient(state->hwnd, &clientPt);
        }
        ev.x = static_cast<double>(clientPt.x);
        ev.y = static_cast<double>(clientPt.y);

        ev.paths.reserve(fileCount);
        for (UINT i = 0; i < fileCount; ++i) {
            UINT len = DragQueryFileW(hdrop, i, nullptr, 0);
            if (len == 0) {
                continue;
            }
            std::wstring wpath;
            wpath.resize(len);
            if (DragQueryFileW(hdrop, i, &wpath[0], len + 1) == 0) {
                continue;
            }
            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), static_cast<int>(wpath.size()), nullptr, 0,
                                              nullptr, nullptr);
            if (utf8Len <= 0) {
                continue;
            }
            std::string path;
            path.resize(utf8Len);
            WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), static_cast<int>(wpath.size()), &path[0], utf8Len,
                                nullptr, nullptr);
            ev.paths.emplace_back(std::move(path));
        }

        GlobalUnlock(medium.hGlobal);
        ReleaseStgMedium(&medium);

        if (!ev.paths.empty()) {
            dispatchDrop(state, ev);
            if (pdwEffect) {
                *pdwEffect = DROPEFFECT_COPY;
            }
        } else {
            dispatchDragCancel(state);
        }

        dispatchDragLeave(state);
        return S_OK;
    }
};

#endif // _WIN32

inline void
glfwDropCallback(GLFWwindow* window, int count, const char** paths)
{
    WindowState* state = findState(window);

    DropEvent ev {};
    ev.kind = PayloadKind::Files;

    double x = 0.0;
    double y = 0.0;
    if (window) {
        glfwGetCursorPos(window, &x, &y);
    }
    ev.x = x;
    ev.y = y;

    if (count > 0 && paths) {
        ev.paths.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i) {
            if (paths[i]) {
                ev.paths.emplace_back(paths[i]);
            }
        }
    }

    if (state) {
        dispatchDrop(state, ev);
        if (state->prevDropCb) {
            state->prevDropCb(window, count, paths);
        }
    } else {
        // No state; forward to GLFW's previously installed callback if any (unlikely).
        // There is no stored previous callback here, so just ignore.
        (void)window;
        (void)count;
        (void)paths;
    }
}

} // namespace detail

inline bool
init(GLFWwindow* window, const Callbacks& callbacks, void* userData)
{
    if (!window) {
        return false;
    }

    detail::WindowState* state = detail::findState(window);
    if (!state) {
        state = detail::addState(window);
    }

    state->callbacks = callbacks;
    state->userData  = userData;

#if defined(__APPLE__) && !defined(_WIN32)
    state->prevDropCb          = nullptr;
    state->dropCallbackInstalled = false;
#elif defined(_WIN32)
    // On Windows we rely on the OLE IDropTarget path for drops.
    state->prevDropCb          = nullptr;
    state->dropCallbackInstalled = false;
#else
    state->prevDropCb          = glfwSetDropCallback(window, &detail::glfwDropCallback);
    state->dropCallbackInstalled = true;
#endif

#if defined(_WIN32)
    if (!state->dropTarget) {
        state->hwnd = glfwGetWin32Window(window);
        if (state->hwnd) {
            HRESULT hrInit = OleInitialize(nullptr);
            (void)hrInit;

            auto* target = new detail::DropTarget(state);
            HRESULT hr   = RegisterDragDrop(state->hwnd, target);
            if (SUCCEEDED(hr)) {
                state->dropTarget = target;
            } else {
                target->Release();
            }
        }
    }
#endif

#if defined(__APPLE__) && !defined(_WIN32)
    detail::platformInitMac(state, window);
#endif

    return true;
}

inline void
shutdown(GLFWwindow* window)
{
    if (!window) {
        return;
    }

    detail::WindowState* state = detail::findState(window);
    if (!state) {
        return;
    }

#if defined(_WIN32)
    if (state->hwnd && state->dropTarget) {
        RevokeDragDrop(state->hwnd);
        state->dropTarget->Release();
        state->dropTarget = nullptr;
        state->hwnd       = nullptr;
    }
#endif

#if defined(__APPLE__) && !defined(_WIN32)
    detail::platformShutdownMac(state, window);
#endif

    if (state->dropCallbackInstalled) {
        glfwSetDropCallback(window, state->prevDropCb);
    }
    detail::removeState(window);
}

} // namespace dnd_glfw
