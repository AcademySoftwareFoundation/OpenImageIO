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

}  // namespace



bool
available()
{
    return true;
}



DialogReply
open_image_file(const std::string& default_path)
{
    DialogReply reply = open_image_files(default_path);
    if (!reply.paths.empty())
        reply.path = reply.paths.front();
    return reply;
}



DialogReply
open_image_files(const std::string& default_path)
{
    NfdThreadGuard guard;
    if (!guard.initialized)
        return map_error("nativefiledialog initialization failed");

    nfdopendialogu8args_t args = {};
    args.filterList            = k_image_filter;
    args.filterCount           = 1;
    args.defaultPath = default_path.empty() ? nullptr : default_path.c_str();

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
        return DialogReply { Result::Cancel, std::string(), {}, std::string() };
    return map_error("nativefiledialog open dialog failed");
}

DialogReply
open_ocio_config_file(const std::string& default_path)
{
    NfdThreadGuard guard;
    if (!guard.initialized)
        return map_error("nativefiledialog initialization failed");

    nfdopendialogu8args_t args = {};
    args.filterList            = k_ocio_filter;
    args.filterCount           = 1;
    args.defaultPath = default_path.empty() ? nullptr : default_path.c_str();

    nfdu8char_t* selected_path = nullptr;
    nfdresult_t result         = NFD_OpenDialogU8_With(&selected_path, &args);
    if (result == NFD_OKAY) {
        DialogReply reply;
        reply.result = Result::Okay;
        if (selected_path != nullptr && selected_path[0] != '\0')
            reply.path = selected_path;
        NFD_FreePathU8(selected_path);
        return reply;
    }
    if (result == NFD_CANCEL)
        return DialogReply { Result::Cancel, std::string(), {}, std::string() };
    return map_error("nativefiledialog OCIO config dialog failed");
}



DialogReply
save_image_file(const std::string& default_path,
                const std::string& default_name)
{
    NfdThreadGuard guard;
    if (!guard.initialized)
        return map_error("nativefiledialog initialization failed");

    nfdsavedialogu8args_t args = {};
    args.filterList            = k_image_filter;
    args.filterCount           = 1;
    args.defaultPath = default_path.empty() ? nullptr : default_path.c_str();
    args.defaultName = default_name.empty() ? nullptr : default_name.c_str();

    nfdu8char_t* selected_path = nullptr;
    nfdresult_t result         = NFD_SaveDialogU8_With(&selected_path, &args);
    if (result == NFD_OKAY) {
        DialogReply reply;
        reply.result = Result::Okay;
        if (selected_path)
            reply.path = selected_path;
        NFD_FreePathU8(selected_path);
        return reply;
    }
    if (result == NFD_CANCEL)
        return DialogReply { Result::Cancel, std::string(), {}, std::string() };
    return map_error("nativefiledialog save dialog failed");
}

#else

bool
available()
{
    return false;
}



DialogReply
open_image_file(const std::string& default_path)
{
    (void)default_path;
    return DialogReply { Result::Unsupported, std::string(),
                         "nativefiledialog integration is not configured" };
}



DialogReply
open_image_files(const std::string& default_path)
{
    (void)default_path;
    return DialogReply { Result::Unsupported,
                         std::string(),
                         {},
                         "nativefiledialog integration is not configured" };
}

DialogReply
open_ocio_config_file(const std::string& default_path)
{
    (void)default_path;
    return DialogReply { Result::Unsupported,
                         std::string(),
                         {},
                         "nativefiledialog integration is not configured" };
}



DialogReply
save_image_file(const std::string& default_path,
                const std::string& default_name)
{
    (void)default_path;
    (void)default_name;
    return DialogReply { Result::Unsupported, std::string(),
                         "nativefiledialog integration is not configured" };
}

#endif

}  // namespace Imiv::FileDialog
