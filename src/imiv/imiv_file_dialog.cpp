// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_file_dialog.h"

#ifndef IMIV_HAS_NFD
#    define IMIV_HAS_NFD 0
#endif

#if IMIV_HAS_NFD
#    include <nfd.h>
#endif

namespace Imiv::FileDialog {

namespace {

    NativeDialogScopeHook g_native_dialog_scope_hook = nullptr;
    void* g_native_dialog_scope_user_data            = nullptr;
    constexpr const char* k_not_configured_message
        = "nativefiledialog integration is not configured";

    const char* env_value(const char* name)
    {
        const char* value = std::getenv(name);
        return (value != nullptr && value[0] != '\0') ? value : nullptr;
    }

    DialogReply cancel_reply()
    {
        return DialogReply { Result::Cancel, std::string(), {}, std::string() };
    }

    DialogReply unsupported_reply()
    {
        return DialogReply {
            Result::Unsupported, std::string(), {}, k_not_configured_message
        };
    }

    DialogReply okay_reply(const char* path)
    {
        DialogReply reply;
        reply.result = Result::Okay;
        if (path != nullptr && path[0] != '\0')
            reply.path = path;
        return reply;
    }

    const char* optional_c_str(const std::string& value)
    {
        return value.empty() ? nullptr : value.c_str();
    }

    struct NativeDialogScopeGuard {
        bool active = false;
        NativeDialogScopeGuard()
        {
            if (g_native_dialog_scope_hook != nullptr) {
                g_native_dialog_scope_hook(true,
                                           g_native_dialog_scope_user_data);
                active = true;
            }
        }
        ~NativeDialogScopeGuard()
        {
            if (active && g_native_dialog_scope_hook != nullptr)
                g_native_dialog_scope_hook(false,
                                           g_native_dialog_scope_user_data);
        }
    };

}  // namespace

void
set_native_dialog_scope_hook(NativeDialogScopeHook hook, void* user_data)
{
    g_native_dialog_scope_hook      = hook;
    g_native_dialog_scope_user_data = user_data;
}

#if IMIV_HAS_NFD

namespace {

    const nfdu8filteritem_t k_image_filter[] = {
        { "Image Files",
          "avif,bmp,dpx,exr,gif,hdr,heic,jpg,jpeg,jxl,png,tif,tiff,tx,webp" }
    };
    const nfdu8filteritem_t k_ocio_filter[]
        = { { "OCIO Config Files", "ocio,ocioz" } };

    struct NfdThreadGuard {
        bool initialized = false;
        NfdThreadGuard() { initialized = (NFD_Init() == NFD_OKAY); }
        ~NfdThreadGuard()
        {
            if (initialized)
                NFD_Quit();
        }
    };

    struct NfdDialogScope {
        NativeDialogScopeGuard dialog_scope_guard;
        NfdThreadGuard thread_guard;
        bool initialized() const { return thread_guard.initialized; }
    };

    DialogReply map_error(const char* fallback_message)
    {
        DialogReply reply;
        reply.result    = Result::Error;
        reply.message   = fallback_message;
        const char* err = NFD_GetError();
        if (err && err[0] != '\0')
            reply.message = err;
        return reply;
    }

    DialogReply path_reply(nfdresult_t result, nfdu8char_t* selected_path,
                           const char* fallback_message)
    {
        if (result == NFD_OKAY) {
            DialogReply reply = okay_reply(selected_path);
            NFD_FreePathU8(selected_path);
            return reply;
        }
        if (result == NFD_CANCEL)
            return cancel_reply();
        return map_error(fallback_message);
    }

}  // namespace



bool
available()
{
    return true;
}

DialogReply
open_image_files(const std::string& default_path)
{
    NfdDialogScope scope;
    if (!scope.initialized())
        return map_error("nativefiledialog initialization failed");

    nfdopendialogu8args_t args = {};
    args.filterList            = k_image_filter;
    args.filterCount           = 1;
    args.defaultPath           = optional_c_str(default_path);

    const nfdpathset_t* selected_paths = nullptr;
    nfdresult_t result = NFD_OpenDialogMultipleU8_With(&selected_paths, &args);
    if (result == NFD_OKAY) {
        DialogReply reply;
        reply.result                = Result::Okay;
        nfdpathsetsize_t path_count = 0;
        if (NFD_PathSet_GetCount(selected_paths, &path_count) == NFD_OKAY) {
            reply.paths.reserve(static_cast<size_t>(path_count));
            for (nfdpathsetsize_t i = 0; i < path_count; ++i) {
                nfdu8char_t* selected_path = nullptr;
                if (NFD_PathSet_GetPathU8(selected_paths, i, &selected_path)
                    != NFD_OKAY) {
                    continue;
                }
                if (selected_path != nullptr && selected_path[0] != '\0')
                    reply.paths.emplace_back(selected_path);
                NFD_PathSet_FreePathU8(selected_path);
            }
        }
        if (!reply.paths.empty())
            reply.path = reply.paths.front();
        NFD_PathSet_Free(selected_paths);
        return reply;
    }
    if (result == NFD_CANCEL)
        return cancel_reply();
    return map_error("nativefiledialog open dialog failed");
}

DialogReply
open_folder(const std::string& default_path)
{
    NfdDialogScope scope;
    if (!scope.initialized())
        return map_error("nativefiledialog initialization failed");

    nfdpickfolderu8args_t args = {};
    args.defaultPath           = optional_c_str(default_path);

    nfdu8char_t* selected_path = nullptr;
    nfdresult_t result         = NFD_PickFolderU8_With(&selected_path, &args);
    return path_reply(result, selected_path,
                      "nativefiledialog folder dialog failed");
}

DialogReply
open_ocio_config_file(const std::string& default_path)
{
    NfdDialogScope scope;
    if (!scope.initialized())
        return map_error("nativefiledialog initialization failed");

    nfdopendialogu8args_t args = {};
    args.filterList            = k_ocio_filter;
    args.filterCount           = 1;
    args.defaultPath           = optional_c_str(default_path);

    nfdu8char_t* selected_path = nullptr;
    nfdresult_t result         = NFD_OpenDialogU8_With(&selected_path, &args);
    return path_reply(result, selected_path,
                      "nativefiledialog OCIO config dialog failed");
}

DialogReply
save_image_file(const std::string& default_path,
                const std::string& default_name)
{
    if (const char* override_path = env_value("IMIV_TEST_SAVE_IMAGE_PATH")) {
        DialogReply reply;
        reply.result = Result::Okay;
        reply.path   = override_path;
        return reply;
    }

    NfdDialogScope scope;
    if (!scope.initialized())
        return map_error("nativefiledialog initialization failed");

    nfdsavedialogu8args_t args = {};
    args.filterList            = k_image_filter;
    args.filterCount           = 1;
    args.defaultPath           = optional_c_str(default_path);
    args.defaultName           = optional_c_str(default_name);

    nfdu8char_t* selected_path = nullptr;
    nfdresult_t result         = NFD_SaveDialogU8_With(&selected_path, &args);
    return path_reply(result, selected_path,
                      "nativefiledialog save dialog failed");
}

#else

bool
available()
{
    return false;
}

DialogReply
open_image_files(const std::string& default_path)
{
    (void)default_path;
    return unsupported_reply();
}

DialogReply
open_ocio_config_file(const std::string& default_path)
{
    (void)default_path;
    return unsupported_reply();
}

DialogReply
open_folder(const std::string& default_path)
{
    (void)default_path;
    return unsupported_reply();
}

DialogReply
save_image_file(const std::string& default_path,
                const std::string& default_name)
{
    (void)default_path;
    (void)default_name;
    if (const char* override_path = env_value("IMIV_TEST_SAVE_IMAGE_PATH")) {
        DialogReply reply;
        reply.result = Result::Okay;
        reply.path   = override_path;
        return reply;
    }
    return unsupported_reply();
}

#endif

}  // namespace Imiv::FileDialog
