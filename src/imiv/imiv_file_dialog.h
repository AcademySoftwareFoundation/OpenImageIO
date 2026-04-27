// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <string>
#include <vector>

namespace Imiv::FileDialog {

enum class Result { Okay = 0, Cancel, Error, Unsupported };

struct DialogReply {
    Result result = Result::Unsupported;
    std::string path;
    std::vector<std::string> paths;
    std::string message;
};

using NativeDialogScopeHook = void (*)(bool begin_scope, void* user_data);

bool
available();
void
set_native_dialog_scope_hook(NativeDialogScopeHook hook, void* user_data);
DialogReply
open_image_files(const std::string& default_path);
DialogReply
open_folder(const std::string& default_path);
DialogReply
open_ocio_config_file(const std::string& default_path);
DialogReply
save_image_file(const std::string& default_path,
                const std::string& default_name);

}  // namespace Imiv::FileDialog
