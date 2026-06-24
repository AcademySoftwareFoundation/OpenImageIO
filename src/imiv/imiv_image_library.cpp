// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_image_library.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>

using namespace OIIO;

namespace Imiv {
namespace {

    constexpr size_t k_max_recent_images = 16;

    std::unordered_set<std::string> build_readable_image_extensions()
    {
        std::unordered_set<std::string> result;
        const auto extension_map = get_extension_map();
        const auto input_formats
            = Strutil::splits(get_string_attribute("input_format_list"), ",");
        std::unordered_set<std::string> readable_formats;
        readable_formats.reserve(input_formats.size());
        for (std::string format_name : input_formats) {
            Strutil::to_lower(format_name);
            if (!format_name.empty())
                readable_formats.insert(std::move(format_name));
        }

        for (const auto& entry : extension_map) {
            std::string format_name = entry.first;
            Strutil::to_lower(format_name);
            if (readable_formats.find(format_name) == readable_formats.end())
                continue;
            for (std::string one_ext : entry.second) {
                Strutil::to_lower(one_ext);
                if (one_ext.empty())
                    continue;
                if (one_ext.front() != '.')
                    one_ext.insert(one_ext.begin(), '.');
                result.insert(std::move(one_ext));
            }
        }
        return result;
    }

    const std::unordered_set<std::string>& readable_image_extensions()
    {
        static const std::unordered_set<std::string> readable_extensions
            = build_readable_image_extensions();
        return readable_extensions;
    }

    bool has_supported_image_extension(const std::filesystem::path& path)
    {
        std::string ext = path.extension().string();
        Strutil::to_lower(ext);
        if (ext.empty())
            return false;
        return readable_image_extensions().find(ext)
               != readable_image_extensions().end();
    }

    bool datetime_to_time_t(std::string_view datetime, std::time_t& out_time)
    {
        int year  = 0;
        int month = 0;
        int day   = 0;
        int hour  = 0;
        int min   = 0;
        int sec   = 0;
        if (!Strutil::scan_datetime(datetime, year, month, day, hour, min, sec))
            return false;

        std::tm tm_value = {};
        tm_value.tm_sec  = sec;
        tm_value.tm_min  = min;
        tm_value.tm_hour = hour;
        tm_value.tm_mday = day;
        tm_value.tm_mon  = month - 1;
        tm_value.tm_year = year - 1900;
        out_time         = std::mktime(&tm_value);
        return out_time != static_cast<std::time_t>(-1);
    }

    bool file_last_write_time(const std::string& path, std::time_t& out_time)
    {
        std::error_code ec;
        const auto file_time = std::filesystem::last_write_time(path, ec);
        if (ec)
            return false;
        const auto system_time
            = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                file_time - std::filesystem::file_time_type::clock::now()
                + std::chrono::system_clock::now());
        out_time = std::chrono::system_clock::to_time_t(system_time);
        return true;
    }

    bool image_datetime(const std::string& path, std::time_t& out_time)
    {
        auto input = ImageInput::open(path);
        if (!input)
            return false;
        const ImageSpec spec = input->spec();
        input->close();

        const std::string datetime = spec.get_string_attribute("DateTime");
        if (datetime.empty())
            return false;
        return datetime_to_time_t(datetime, out_time);
    }

    std::string normalize_path_for_viewer_list(const std::string& path)
    {
        if (path.empty())
            return std::string();
        std::filesystem::path p(path);
        std::error_code ec;
        if (!p.is_absolute()) {
            const std::filesystem::path abs = std::filesystem::absolute(p, ec);
            if (!ec)
                p = abs;
        }
        return p.lexically_normal().string();
    }

    int find_path_index(const std::vector<std::string>& paths,
                        const std::string& path)
    {
        if (path.empty())
            return -1;
        const auto it = std::find(paths.begin(), paths.end(), path);
        if (it == paths.end())
            return -1;
        return static_cast<int>(std::distance(paths.begin(), it));
    }

    std::string filename_key(const std::string& path)
    {
        return std::filesystem::path(path).filename().string();
    }

    std::string path_key(const std::string& path)
    {
        return std::filesystem::path(path).lexically_normal().string();
    }

    bool sort_path_by_name(const std::string& a, const std::string& b)
    {
        const std::string a_name = filename_key(a);
        const std::string b_name = filename_key(b);
        if (a_name == b_name)
            return path_key(a) < path_key(b);
        return a_name < b_name;
    }

    bool sort_path_by_path(const std::string& a, const std::string& b)
    {
        return path_key(a) < path_key(b);
    }

    bool sort_path_by_image_date(const std::string& a, const std::string& b)
    {
        std::time_t a_time = {};
        std::time_t b_time = {};
        const bool a_ok    = image_datetime(a, a_time)
                          || file_last_write_time(a, a_time);
        const bool b_ok = image_datetime(b, b_time)
                          || file_last_write_time(b, b_time);
        if (a_ok != b_ok)
            return a_ok;
        if (!a_ok && !b_ok)
            return filename_key(a) < filename_key(b);
        if (a_time == b_time)
            return filename_key(a) < filename_key(b);
        return a_time < b_time;
    }

    bool sort_path_by_file_date(const std::string& a, const std::string& b)
    {
        std::time_t a_time = {};
        std::time_t b_time = {};
        const bool a_ok    = file_last_write_time(a, a_time);
        const bool b_ok    = file_last_write_time(b, b_time);
        if (a_ok != b_ok)
            return a_ok;
        if (!a_ok && !b_ok)
            return filename_key(a) < filename_key(b);
        if (a_time == b_time)
            return filename_key(a) < filename_key(b);
        return a_time < b_time;
    }

    void sort_image_path_list(std::vector<std::string>& paths,
                              ImageSortMode sort_mode, bool sort_reverse)
    {
        if (paths.empty())
            return;

        switch (sort_mode) {
        case ImageSortMode::ByName:
            std::sort(paths.begin(), paths.end(), sort_path_by_name);
            break;
        case ImageSortMode::ByPath:
            std::sort(paths.begin(), paths.end(), sort_path_by_path);
            break;
        case ImageSortMode::ByImageDate:
            std::sort(paths.begin(), paths.end(), sort_path_by_image_date);
            break;
        case ImageSortMode::ByFileDate:
            std::sort(paths.begin(), paths.end(), sort_path_by_file_date);
            break;
        }

        if (sort_reverse)
            std::reverse(paths.begin(), paths.end());
    }

}  // namespace

bool
collect_directory_image_paths(const std::string& directory_path,
                              ImageSortMode sort_mode, bool sort_reverse,
                              std::vector<std::string>& out_paths,
                              std::string& error_message)
{
    out_paths.clear();
    error_message.clear();

    std::error_code ec;
    std::filesystem::path dir(directory_path);
    if (dir.empty()) {
        error_message = "directory path is empty";
        return false;
    }
    if (!std::filesystem::exists(dir, ec) || ec) {
        error_message = Strutil::fmt::format("directory '{}' does not exist",
                                             dir.string());
        return false;
    }
    if (!std::filesystem::is_directory(dir, ec) || ec) {
        error_message = Strutil::fmt::format("'{}' is not a directory",
                                             dir.string());
        return false;
    }

    std::filesystem::directory_iterator it(
        dir, std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) {
        error_message = Strutil::fmt::format("failed to scan '{}': {}",
                                             dir.string(), ec.message());
        return false;
    }

    for (; it != std::filesystem::directory_iterator(); it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        const std::filesystem::directory_entry& entry = *it;
        if (!entry.is_regular_file(ec)) {
            ec.clear();
            continue;
        }
        if (!has_supported_image_extension(entry.path()))
            continue;
        out_paths.push_back(
            normalize_path_for_viewer_list(entry.path().string()));
    }

    sort_image_path_list(out_paths, sort_mode, sort_reverse);
    return true;
}

bool
add_loaded_image_path(ImageLibraryState& library, const std::string& path,
                      int* out_index)
{
    if (out_index != nullptr)
        *out_index = -1;
    const std::string normalized = normalize_path_for_viewer_list(path);
    if (normalized.empty())
        return false;

    int index = find_path_index(library.loaded_image_paths, normalized);
    if (index < 0) {
        library.loaded_image_paths.push_back(normalized);
        index = static_cast<int>(library.loaded_image_paths.size()) - 1;
    }
    if (out_index != nullptr)
        *out_index = index;
    return index >= 0;
}

bool
append_loaded_image_paths(ImageLibraryState& library,
                          const std::vector<std::string>& paths,
                          int* out_first_added_index)
{
    if (out_first_added_index != nullptr)
        *out_first_added_index = -1;

    bool added_any = false;
    std::string first_added_path;
    for (const std::string& path : paths) {
        const std::string normalized = normalize_path_for_viewer_list(path);
        if (normalized.empty())
            continue;
        if (find_path_index(library.loaded_image_paths, normalized) >= 0)
            continue;
        library.loaded_image_paths.push_back(normalized);
        if (first_added_path.empty())
            first_added_path = normalized;
        added_any = true;
    }

    if (!added_any)
        return false;

    if (out_first_added_index != nullptr && !first_added_path.empty()) {
        *out_first_added_index = find_path_index(library.loaded_image_paths,
                                                 first_added_path);
    }
    return true;
}

bool
remove_loaded_image_path(ImageLibraryState& library, ViewerState* viewer,
                         const std::string& path)
{
    const std::string normalized = normalize_path_for_viewer_list(path);
    const int remove_index       = find_path_index(library.loaded_image_paths,
                                                   normalized);
    if (remove_index < 0)
        return false;

    library.loaded_image_paths.erase(library.loaded_image_paths.begin()
                                     + remove_index);
    if (viewer != nullptr) {
        if (viewer->current_path_index == remove_index) {
            viewer->current_path_index = -1;
        } else if (viewer->current_path_index > remove_index) {
            --viewer->current_path_index;
        }

        if (viewer->last_path_index == remove_index) {
            viewer->last_path_index = -1;
        } else if (viewer->last_path_index > remove_index) {
            --viewer->last_path_index;
        }
    }
    return true;
}

bool
set_current_loaded_image_path(const ImageLibraryState& library,
                              ViewerState& viewer, const std::string& path)
{
    const std::string normalized = normalize_path_for_viewer_list(path);
    const int new_index          = find_path_index(library.loaded_image_paths,
                                                   normalized);
    if (new_index < 0)
        return false;
    if (viewer.current_path_index >= 0
        && viewer.current_path_index != new_index) {
        viewer.last_path_index = viewer.current_path_index;
    }
    viewer.current_path_index = new_index;
    return true;
}

bool
pick_loaded_image_path(const ImageLibraryState& library,
                       const ViewerState& viewer, int delta,
                       std::string& out_path)
{
    out_path.clear();
    if (library.loaded_image_paths.empty() || viewer.current_path_index < 0)
        return false;
    const int count = static_cast<int>(library.loaded_image_paths.size());
    int index       = viewer.current_path_index + delta;
    while (index < 0)
        index += count;
    index %= count;
    out_path = library.loaded_image_paths[static_cast<size_t>(index)];
    return !out_path.empty();
}

void
sort_loaded_image_paths(ImageLibraryState& library,
                        const std::vector<ViewerState*>& viewers)
{
    std::vector<std::string> current_paths;
    std::vector<std::string> last_paths;
    current_paths.reserve(viewers.size());
    last_paths.reserve(viewers.size());
    for (const ViewerState* viewer : viewers) {
        current_paths.emplace_back(viewer != nullptr ? viewer->image.path
                                                     : std::string());
        const std::string last_path
            = (viewer != nullptr && viewer->last_path_index >= 0
               && viewer->last_path_index
                      < static_cast<int>(library.loaded_image_paths.size()))
                  ? library.loaded_image_paths[static_cast<size_t>(
                      viewer->last_path_index)]
                  : std::string();
        last_paths.emplace_back(last_path);
    }

    sort_image_path_list(library.loaded_image_paths, library.sort_mode,
                         library.sort_reverse);
    for (size_t i = 0, e = viewers.size(); i < e; ++i) {
        ViewerState* viewer = viewers[i];
        if (viewer == nullptr)
            continue;
        viewer->current_path_index
            = find_path_index(library.loaded_image_paths,
                              normalize_path_for_viewer_list(current_paths[i]));
        viewer->last_path_index
            = find_path_index(library.loaded_image_paths,
                              normalize_path_for_viewer_list(last_paths[i]));
    }
}

void
sync_viewer_library_state(ViewerState& viewer, const ImageLibraryState& library)
{
    const std::string current_path = viewer.image.path;
    const std::string last_path
        = (viewer.last_path_index >= 0
           && viewer.last_path_index
                  < static_cast<int>(viewer.loaded_image_paths.size()))
              ? viewer.loaded_image_paths[static_cast<size_t>(
                  viewer.last_path_index)]
              : std::string();
    viewer.loaded_image_paths = library.loaded_image_paths;
    viewer.recent_images      = library.recent_images;
    viewer.sort_mode          = library.sort_mode;
    viewer.sort_reverse       = library.sort_reverse;
    viewer.current_path_index
        = find_path_index(viewer.loaded_image_paths,
                          normalize_path_for_viewer_list(current_path));
    viewer.last_path_index
        = find_path_index(viewer.loaded_image_paths,
                          normalize_path_for_viewer_list(last_path));
}

void
add_recent_image_path(ImageLibraryState& library, const std::string& path)
{
    const std::string normalized = normalize_path_for_viewer_list(path);
    if (normalized.empty())
        return;

    auto it = std::remove(library.recent_images.begin(),
                          library.recent_images.end(), normalized);
    library.recent_images.erase(it, library.recent_images.end());
    library.recent_images.insert(library.recent_images.begin(), normalized);
    if (library.recent_images.size() > k_max_recent_images)
        library.recent_images.resize(k_max_recent_images);
}

}  // namespace Imiv
