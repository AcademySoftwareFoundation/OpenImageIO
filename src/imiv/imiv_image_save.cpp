// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_image_save.h"

#include "imiv_loaded_image.h"
#include "imiv_ocio.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>

using namespace OIIO;

namespace Imiv {
namespace {

    bool build_selection_source_image(const LoadedImage& image,
                                      const ViewerState& viewer,
                                      ImageBuf& result,
                                      std::string& error_message)
    {
        error_message.clear();
        if (!has_image_selection(viewer)) {
            error_message = "no active selection";
            return false;
        }

        ImageBuf source;
        if (!imagebuf_from_loaded_image(image, source, error_message))
            return false;

        const ROI selection_roi(viewer.selection_xbegin, viewer.selection_xend,
                                viewer.selection_ybegin, viewer.selection_yend,
                                0, 1, 0, image.nchannels);
        result = ImageBufAlgo::cut(source, selection_roi);
        if (result.has_error()) {
            error_message = result.geterror();
            if (error_message.empty())
                error_message = "failed to crop selected image region";
            return false;
        }

        result.specmod().attribute("Orientation", image.orientation);
        return true;
    }

    float selected_channel(float r, float g, float b, float a, int channel)
    {
        if (channel == 1)
            return r;
        if (channel == 2)
            return g;
        if (channel == 3)
            return b;
        if (channel == 4)
            return a;
        return r;
    }

    void apply_heatmap(float value, float& out_r, float& out_g, float& out_b)
    {
        const float t = std::clamp(value, 0.0f, 1.0f);
        if (t < 0.33f) {
            const float u = t / 0.33f;
            out_r         = 0.0f;
            out_g         = 0.9f * u;
            out_b         = 0.5f + (1.0f - 0.5f) * u;
            return;
        }
        if (t < 0.66f) {
            const float u = (t - 0.33f) / 0.33f;
            out_r         = u;
            out_g         = 0.9f + (1.0f - 0.9f) * u;
            out_b         = 1.0f - u;
            return;
        }
        const float u = (t - 0.66f) / 0.34f;
        out_r         = 1.0f;
        out_g         = 1.0f - u;
        out_b         = 0.0f;
    }

    void apply_window_recipe_to_rgba(std::vector<float>& rgba_pixels,
                                     const ViewRecipe& recipe,
                                     bool exposure_gamma_already_applied)
    {
        const size_t pixel_count = rgba_pixels.size() / 4;
        for (size_t pixel = 0; pixel < pixel_count; ++pixel) {
            float& r = rgba_pixels[pixel * 4 + 0];
            float& g = rgba_pixels[pixel * 4 + 1];
            float& b = rgba_pixels[pixel * 4 + 2];
            float& a = rgba_pixels[pixel * 4 + 3];

            r += recipe.offset;
            g += recipe.offset;
            b += recipe.offset;

            if (recipe.color_mode == 1) {
                a = 1.0f;
            } else if (recipe.color_mode == 2) {
                const float v = selected_channel(r, g, b, a,
                                                 recipe.current_channel);
                r             = v;
                g             = v;
                b             = v;
                a             = 1.0f;
            } else if (recipe.color_mode == 3) {
                const float y = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                r             = y;
                g             = y;
                b             = y;
                a             = 1.0f;
            } else if (recipe.color_mode == 4) {
                const float v = selected_channel(r, g, b, a,
                                                 recipe.current_channel);
                apply_heatmap(v, r, g, b);
                a = 1.0f;
            }

            if (recipe.current_channel > 0 && recipe.color_mode != 2
                && recipe.color_mode != 4) {
                const float v = selected_channel(r, g, b, a,
                                                 recipe.current_channel);
                r             = v;
                g             = v;
                b             = v;
                a             = 1.0f;
            }

            if (!exposure_gamma_already_applied) {
                const float exposure_scale = std::exp2(recipe.exposure);
                r = std::pow(std::max(r * exposure_scale, 0.0f),
                             1.0f / std::max(recipe.gamma, 0.01f));
                g = std::pow(std::max(g * exposure_scale, 0.0f),
                             1.0f / std::max(recipe.gamma, 0.01f));
                b = std::pow(std::max(b * exposure_scale, 0.0f),
                             1.0f / std::max(recipe.gamma, 0.01f));
            }
        }
    }

    bool build_view_export_rgba_image(const ImageBuf& source_image,
                                      const LoadedImage& image_metadata,
                                      const ViewRecipe& recipe,
                                      const PlaceholderUiState& ui_state,
                                      ImageBuf& output,
                                      std::string& error_message)
    {
        error_message.clear();
        ImageBuf oriented = source_image;
        const int orientation
            = source_image.spec().get_int_attribute("Orientation", 1);
        if (orientation != 1) {
            oriented = ImageBufAlgo::reorient(source_image);
            if (oriented.has_error()) {
                error_message = oriented.geterror();
                if (error_message.empty())
                    error_message = "failed to orient export image";
                return false;
            }
        }

        const int width     = oriented.spec().width;
        const int height    = oriented.spec().height;
        const int nchannels = oriented.nchannels();
        if (width <= 0 || height <= 0 || nchannels <= 0) {
            error_message = "window export source image is invalid";
            return false;
        }

        std::vector<float> src_pixels(static_cast<size_t>(width)
                                      * static_cast<size_t>(height)
                                      * static_cast<size_t>(nchannels));
        if (!oriented.get_pixels(ROI::All(), TypeFloat, src_pixels.data())) {
            error_message = oriented.geterror();
            if (error_message.empty())
                error_message = "failed to read source pixels for window export";
            return false;
        }

        std::vector<float> rgba_pixels(static_cast<size_t>(width)
                                           * static_cast<size_t>(height) * 4u,
                                       0.0f);
        const size_t pixel_count = static_cast<size_t>(width)
                                   * static_cast<size_t>(height);
        for (size_t pixel = 0; pixel < pixel_count; ++pixel) {
            const float* src
                = &src_pixels[pixel * static_cast<size_t>(nchannels)];
            float& r = rgba_pixels[pixel * 4 + 0];
            float& g = rgba_pixels[pixel * 4 + 1];
            float& b = rgba_pixels[pixel * 4 + 2];
            float& a = rgba_pixels[pixel * 4 + 3];
            if (nchannels == 1) {
                r = src[0];
                g = src[0];
                b = src[0];
                a = 1.0f;
            } else if (nchannels == 2) {
                r = src[0];
                g = src[0];
                b = src[0];
                a = src[1];
            } else {
                r = src[0];
                g = src[1];
                b = src[2];
                a = nchannels >= 4 ? src[3] : 1.0f;
            }
        }

        bool ocio_applied = false;
        if (recipe.use_ocio) {
            PlaceholderUiState export_ui_state = ui_state;
            apply_view_recipe_to_ui_state(recipe, export_ui_state);
            OCIO::ConstProcessorRcPtr processor;
            std::string resolved_display;
            std::string resolved_view;
            if (!build_ocio_cpu_display_processor(
                    export_ui_state, &image_metadata, recipe.exposure,
                    recipe.gamma, processor, resolved_display, resolved_view,
                    error_message)) {
                return false;
            }
            if (!processor) {
                error_message = "OCIO window export processor is unavailable";
                return false;
            }
            try {
                OCIO::ConstCPUProcessorRcPtr cpu_processor
                    = processor->getDefaultCPUProcessor();
                if (!cpu_processor) {
                    error_message
                        = "OCIO CPU processor is unavailable for window export";
                    return false;
                }
                OCIO::PackedImageDesc desc(rgba_pixels.data(), width, height,
                                           4);
                cpu_processor->apply(desc);
            } catch (const OCIO::Exception& e) {
                error_message = e.what();
                return false;
            }
            ocio_applied = true;
        }

        apply_window_recipe_to_rgba(rgba_pixels, recipe, ocio_applied);

        ImageSpec spec(width, height, 4, TypeFloat);
        spec.attribute("Orientation", 1);
        spec.channelnames = { "R", "G", "B", "A" };
        output.reset(spec);
        if (!output.set_pixels(ROI::All(), TypeFloat, rgba_pixels.data())) {
            error_message = output.geterror();
            if (error_message.empty())
                error_message = "failed to store window export pixels";
            return false;
        }
        return true;
    }

    bool build_window_export_rgba_image(const LoadedImage& image,
                                        const ViewRecipe& recipe,
                                        const PlaceholderUiState& ui_state,
                                        ImageBuf& output,
                                        std::string& error_message)
    {
        error_message.clear();

        ImageBuf source;
        if (!imagebuf_from_loaded_image(image, source, error_message))
            return false;

        return build_view_export_rgba_image(source, image, recipe, ui_state,
                                            output, error_message);
    }

}  // namespace

bool
save_selection_image(const LoadedImage& image, const ViewerState& viewer,
                     const std::string& path, std::string& error_message)
{
    error_message.clear();
    if (path.empty()) {
        error_message = "save path is empty";
        return false;
    }

    ImageBuf result;
    if (!build_selection_source_image(image, viewer, result, error_message))
        return false;

    const int orientation = result.spec().get_int_attribute("Orientation", 1);
    if (orientation != 1) {
        ImageBuf oriented = ImageBufAlgo::reorient(result);
        if (oriented.has_error()) {
            error_message = oriented.geterror();
            if (error_message.empty())
                error_message = "failed to orient selection export";
            return false;
        }
        result = std::move(oriented);
    }

    result.specmod().attribute("Orientation", 1);
    if (!result.write(path, result.spec().format)) {
        error_message = result.geterror();
        if (error_message.empty())
            error_message = "image write failed";
        return false;
    }
    return true;
}

bool
save_window_image(const LoadedImage& image, const ViewRecipe& recipe,
                  const PlaceholderUiState& ui_state, const std::string& path,
                  std::string& error_message)
{
    error_message.clear();
    if (path.empty()) {
        error_message = "save path is empty";
        return false;
    }

    ImageBuf output;
    if (!build_window_export_rgba_image(image, recipe, ui_state, output,
                                        error_message)) {
        return false;
    }

    if (!output.write(path, TypeFloat)) {
        error_message = output.geterror();
        if (error_message.empty())
            error_message = "image write failed";
        return false;
    }
    return true;
}

bool
save_export_selection_image(const LoadedImage& image, const ViewerState& viewer,
                            const PlaceholderUiState& ui_state,
                            const std::string& path, std::string& error_message)
{
    error_message.clear();
    if (path.empty()) {
        error_message = "save path is empty";
        return false;
    }

    ImageBuf selection;
    if (!build_selection_source_image(image, viewer, selection, error_message))
        return false;

    ImageBuf output;
    if (!build_view_export_rgba_image(selection, image, viewer.recipe, ui_state,
                                      output, error_message)) {
        return false;
    }

    if (!output.write(path, TypeFloat)) {
        error_message = output.geterror();
        if (error_message.empty())
            error_message = "image write failed";
        return false;
    }
    return true;
}

bool
save_loaded_image(const LoadedImage& image, const std::string& path,
                  std::string& error_message)
{
    error_message.clear();
    if (path.empty()) {
        error_message = "save path is empty";
        return false;
    }
    if (image.width <= 0 || image.height <= 0 || image.nchannels <= 0) {
        error_message = "no valid image is loaded";
        return false;
    }

    const TypeDesc format = upload_data_type_to_typedesc(image.type);
    if (format == TypeUnknown) {
        error_message = "unsupported source pixel type for save";
        return false;
    }

    const size_t width         = static_cast<size_t>(image.width);
    const size_t height        = static_cast<size_t>(image.height);
    const size_t channels      = static_cast<size_t>(image.nchannels);
    const size_t min_row_pitch = width * channels * image.channel_bytes;
    if (image.row_pitch_bytes < min_row_pitch) {
        error_message = "image row pitch is invalid";
        return false;
    }
    const size_t required_bytes = image.row_pitch_bytes * height;
    if (image.pixels.size() < required_bytes) {
        error_message = "image pixel buffer is incomplete";
        return false;
    }

    ImageSpec spec(image.width, image.height, image.nchannels, format);
    ImageBuf output(spec);

    const std::byte* begin = reinterpret_cast<const std::byte*>(
        image.pixels.data());
    const cspan<std::byte> byte_span(begin, image.pixels.size());
    const stride_t xstride = static_cast<stride_t>(image.nchannels
                                                   * image.channel_bytes);
    const stride_t ystride = static_cast<stride_t>(image.row_pitch_bytes);
    if (!output.set_pixels(ROI::All(), format, byte_span, begin, xstride,
                           ystride, AutoStride)) {
        error_message = output.geterror();
        if (error_message.empty())
            error_message = "failed to copy pixels into save buffer";
        return false;
    }

    if (!output.write(path, format)) {
        error_message = output.geterror();
        if (error_message.empty())
            error_message = "image write failed";
        return false;
    }
    return true;
}

}  // namespace Imiv
