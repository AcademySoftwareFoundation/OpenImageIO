// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <string>
#include <string_view>

namespace Imiv {

bool
read_env_value(const char* name, std::string& out_value);
bool
parse_bool_string(std::string_view value, bool& out_value);
bool
parse_int_string(std::string_view value, int& out_value);
bool
parse_float_string(std::string_view value, float& out_value);
bool
env_flag_is_truthy(const char* name);
bool
env_read_bool_value(const char* name, bool& out_value);
bool
env_read_int_value(const char* name, int& out_value);
bool
env_read_float_value(const char* name, float& out_value);
int
env_int_value(const char* name, int fallback);
int
env_int_value_clamped(const char* name, int fallback, int min_value,
                      int max_value);
float
env_float_value(const char* name, float fallback, bool* found = nullptr);

}  // namespace Imiv
