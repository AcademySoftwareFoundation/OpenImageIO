// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_parse.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

#include <OpenImageIO/strutil.h>

namespace Imiv {

bool
read_env_value(const char* name, std::string& out_value)
{
    out_value.clear();
    if (name == nullptr || name[0] == '\0')
        return false;
#if defined(_WIN32)
    char* value       = nullptr;
    size_t value_size = 0;
    errno_t err       = _dupenv_s(&value, &value_size, name);
    if (err != 0 || value == nullptr || value[0] == '\0') {
        if (value != nullptr)
            std::free(value);
        return false;
    }
    out_value.assign(value);
    std::free(value);
#else
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0')
        return false;
    out_value.assign(value);
#endif
    return true;
}

bool
parse_bool_string(std::string_view value, bool& out_value)
{
    const std::string_view trimmed = OIIO::Strutil::strip(value);
    if (trimmed.empty())
        return false;
    if (trimmed == "1" || OIIO::Strutil::iequals(trimmed, "true")
        || OIIO::Strutil::iequals(trimmed, "yes")
        || OIIO::Strutil::iequals(trimmed, "on")) {
        out_value = true;
        return true;
    }
    if (trimmed == "0" || OIIO::Strutil::iequals(trimmed, "false")
        || OIIO::Strutil::iequals(trimmed, "no")
        || OIIO::Strutil::iequals(trimmed, "off")) {
        out_value = false;
        return true;
    }
    return false;
}

bool
parse_int_string(std::string_view value, int& out_value)
{
    const std::string trimmed = std::string(OIIO::Strutil::strip(value));
    if (trimmed.empty())
        return false;
    char* end   = nullptr;
    long parsed = std::strtol(trimmed.c_str(), &end, 10);
    if (end == trimmed.c_str() || *end != '\0')
        return false;
    if (parsed < static_cast<long>(std::numeric_limits<int>::min())
        || parsed > static_cast<long>(std::numeric_limits<int>::max())) {
        return false;
    }
    out_value = static_cast<int>(parsed);
    return true;
}

bool
parse_float_string(std::string_view value, float& out_value)
{
    const std::string trimmed = std::string(OIIO::Strutil::strip(value));
    if (trimmed.empty())
        return false;
    char* end    = nullptr;
    float parsed = std::strtof(trimmed.c_str(), &end);
    if (end == trimmed.c_str() || *end != '\0')
        return false;
    out_value = parsed;
    return true;
}

bool
env_flag_is_truthy(const char* name)
{
    bool out_value = false;
    return env_read_bool_value(name, out_value) && out_value;
}

bool
env_read_bool_value(const char* name, bool& out_value)
{
    std::string env_raw;
    return read_env_value(name, env_raw)
           && parse_bool_string(env_raw, out_value);
}

bool
env_read_int_value(const char* name, int& out_value)
{
    std::string env_raw;
    return read_env_value(name, env_raw)
           && parse_int_string(env_raw, out_value);
}

bool
env_read_float_value(const char* name, float& out_value)
{
    std::string env_raw;
    return read_env_value(name, env_raw)
           && parse_float_string(env_raw, out_value);
}

int
env_int_value(const char* name, int fallback)
{
    int out_value = 0;
    return env_read_int_value(name, out_value) ? out_value : fallback;
}

int
env_int_value_clamped(const char* name, int fallback, int min_value,
                      int max_value)
{
    const int value = env_int_value(name, fallback);
    if (min_value > max_value)
        return value;
    return std::clamp(value, min_value, max_value);
}

float
env_float_value(const char* name, float fallback, bool* found)
{
    if (found != nullptr)
        *found = false;
    float out_value = 0.0f;
    if (!env_read_float_value(name, out_value))
        return fallback;
    if (found != nullptr)
        *found = true;
    return out_value;
}

}  // namespace Imiv
