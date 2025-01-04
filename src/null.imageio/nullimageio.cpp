// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <cstdio>
#include <cstdlib>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>

#include "imageio_pvt.h"



OIIO_PLUGIN_NAMESPACE_BEGIN


// Null output just sits there like a lump and returns ok for everything.
class NullOutput final : public ImageOutput {
public:
    NullOutput() {}
    ~NullOutput() override {}
    const char* format_name(void) const override { return "null"; }
    int supports(string_view feature) const override
    {
        return feature != "rectangles";
    }
    bool open(const std::string& /*name*/, const ImageSpec& spec,
              OpenMode /*mode*/) override
    {
        m_spec = spec;
        return true;
    }
    bool close() override { return true; }
    bool write_scanline(int /*y*/, int /*z*/, TypeDesc /*format*/,
                        const void* /*data*/, stride_t /*xstride*/) override
    {
        return true;
    }
    bool write_tile(int /*x*/, int /*y*/, int /*z*/, TypeDesc /*format*/,
                    const void* /*data*/, stride_t /*xstride*/,
                    stride_t /*ystride*/, stride_t /*zstride*/) override
    {
        return true;
    }
};



// Null input emulates a file, but just returns black tiles.
// But we accept REST-like filename designations to set certain parameters,
// such as "myfile.null&RES=1920x1080&CHANNELS=3&TYPE=uint16"
class NullInput final : public ImageInput {
public:
    NullInput() { init(); }
    ~NullInput() override {}
    const char* format_name(void) const override { return "null"; }
    bool valid_file(const std::string& filename) const override;
    int supports(string_view /*feature*/) const override { return true; }
    bool open(const std::string& name, ImageSpec& newspec) override;
    bool open(const std::string& name, ImageSpec& newspec,
              const ImageSpec& config) override;
    bool close() override { return true; }
    int current_subimage(void) const override { return m_subimage; }
    int current_miplevel(void) const override { return m_miplevel; }
    bool seek_subimage(int subimage, int miplevel) override;
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;
    bool read_native_tile(int subimage, int miplevel, int x, int y, int z,
                          void* data) override;

private:
    std::string m_filename;        ///< Stash the filename
    int m_subimage;                ///< What subimage are we looking at?
    int m_miplevel;                ///< What miplevel are we looking at?
    bool m_mip;                    ///< MIP-mapped?
    std::vector<uint8_t> m_value;  ///< Pixel value (if not black)
    ImageSpec m_topspec;

    // Reset everything to initial state
    void init()
    {
        m_subimage = -1;
        m_miplevel = -1;
        m_mip      = false;
        m_value.clear();
    }
};



// Obligatory material to make this a recognizable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
null_output_imageio_create()
{
    return new NullOutput;
}

OIIO_EXPORT int null_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
null_imageio_library_version()
{
    return "null 1.0";
}

OIIO_EXPORT const char* null_output_extensions[] = { "null", "nul", nullptr };

OIIO_EXPORT ImageInput*
null_input_imageio_create()
{
    return new NullInput;
}

OIIO_EXPORT const char* null_input_extensions[] = { "null", "nul", nullptr };

OIIO_PLUGIN_EXPORTS_END



bool
NullInput::valid_file(const std::string& name) const
{
    std::map<std::string, std::string> args;
    std::string filename;
    if (!Strutil::get_rest_arguments(name, filename, args))
        return false;
    return Strutil::ends_with(filename, ".null")
           || Strutil::ends_with(filename, ".nul");
}



bool
NullInput::open(const std::string& name, ImageSpec& newspec)
{
    ImageSpec config;
    return open(name, newspec, config);
}



static void
parse_res(string_view res, int& x, int& y, int& z)
{
    if (Strutil::parse_int(res, x)) {
        if (Strutil::parse_char(res, 'x') && Strutil::parse_int(res, y)) {
            if (!(Strutil::parse_char(res, 'x') && Strutil::parse_int(res, z)))
                z = 1;
        } else {
            y = x;
            z = 1;
        }
    }
}



// Add the attribute -- figure out the type
void
parse_param(string_view paramname, string_view val, ImageSpec& spec)
{
    TypeDesc type;  // start out unknown

    // If the param string starts with a type name, that's what it is
    if (size_t typeportion = type.fromstring(paramname)) {
        paramname.remove_prefix(typeportion);
        Strutil::skip_whitespace(paramname);
    }
    // If the value string starts with a type name, that's what it is
    else if (size_t typeportion = type.fromstring(val)) {
        val.remove_prefix(typeportion);
        Strutil::skip_whitespace(val);
    }

    if (type.basetype == TypeDesc::UNKNOWN) {
        // If we didn't find a type name, try to guess
        if (val.size() >= 2 && val.front() == '\"' && val.back() == '\"') {
            // Surrounded by quotes? it's a string (strip off the quotes)
            val.remove_prefix(1);
            val.remove_suffix(1);
            type = TypeString;
        } else if (Strutil::string_is<int>(val)) {
            // Looks like an int, is an int
            type = TypeInt;
        } else if (Strutil::string_is<float>(val)) {
            // Looks like a float, is a float
            type = TypeFloat;
        } else {
            // Everything else is assumed a string
            type = TypeString;
        }
    }

    // Read the values and set the attribute
    int n = type.numelements() * type.aggregate;
    if (type.basetype == TypeDesc::INT) {
        std::vector<int> values(n);
        for (int i = 0; i < n; ++i) {
            Strutil::parse_int(val, values[i]);
            Strutil::parse_char(val, ',');  // optional
        }
        if (n > 0)
            spec.attribute(paramname, type, &values[0]);
    }
    if (type.basetype == TypeDesc::FLOAT) {
        std::vector<float> values(n);
        for (int i = 0; i < n; ++i) {
            Strutil::parse_float(val, values[i]);
            Strutil::parse_char(val, ',');  // optional
        }
        if (n > 0)
            spec.attribute(paramname, type, &values[0]);
    } else if (type.basetype == TypeDesc::STRING) {
        std::vector<ustring> values(n);
        for (int i = 0; i < n; ++i) {
            string_view v;
            Strutil::parse_string(val, v);
            Strutil::parse_char(val, ',');  // optional
            values[i] = v;
        }
        if (n > 0)
            spec.attribute(paramname, type, &values[0]);
    }
}



bool
NullInput::open(const std::string& name, ImageSpec& newspec,
                const ImageSpec& config)
{
    m_filename = name;
    m_subimage = -1;
    m_miplevel = -1;
    m_mip      = false;
    m_topspec  = config;

    // std::vector<std::pair<string_view,string_view> > args;
    // string_view filename = deconstruct_uri (name, &args);
    std::map<std::string, std::string> args;
    std::string filename;
    if (!Strutil::get_rest_arguments(name, filename, args))
        return false;
    if (filename.empty())
        return false;

    // To keep the "null" input reader from reading from ANY name, only
    // succeed if it ends in ".null" or ".nul" --OR-- if the config has a
    // special override "null:force" set to nonzero (that lets the caller
    // guarantee a null input even if the name has no extension, say).
    if (!Strutil::ends_with(filename, ".null")
        && !Strutil::ends_with(filename, ".nul")
        && config.get_int_attribute("null:force") == 0)
        return false;

    // Override the config with default resolution if it was not set
    if (m_topspec.width <= 0)
        m_topspec.width = 1024;
    if (m_topspec.height <= 0)
        m_topspec.height = 1024;
    if (m_topspec.depth <= 0)
        m_topspec.depth = 1;
    if (m_topspec.full_width <= 0)
        m_topspec.full_width = m_topspec.width;
    if (m_topspec.full_height <= 0)
        m_topspec.full_height = m_topspec.height;
    if (m_topspec.full_depth <= 0)
        m_topspec.full_depth = m_topspec.depth;
    if (m_topspec.nchannels <= 0)
        m_topspec.nchannels = 4;
    if (m_topspec.format == TypeUnknown)
        m_topspec.format = TypeFloat;

    m_filename = filename;
    std::vector<float> fvalue;

    for (const auto& a : args) {
        if (a.first == "RES") {
            parse_res(a.second, m_topspec.width, m_topspec.height,
                      m_topspec.depth);
            m_topspec.full_x      = m_topspec.x;
            m_topspec.full_y      = m_topspec.y;
            m_topspec.full_z      = m_topspec.z;
            m_topspec.full_width  = m_topspec.width;
            m_topspec.full_height = m_topspec.height;
            m_topspec.full_depth  = m_topspec.depth;
        } else if (a.first == "TILE" || a.first == "TILES") {
            parse_res(a.second, m_topspec.tile_width, m_topspec.tile_height,
                      m_topspec.tile_depth);
        } else if (a.first == "CHANNELS") {
            m_topspec.nchannels = Strutil::from_string<int>(a.second);
            m_topspec.default_channel_names();
        } else if (a.first == "MIP") {
            m_mip = Strutil::from_string<int>(a.second);
        } else if (a.first == "TEX") {
            if (Strutil::from_string<int>(a.second)) {
                if (!m_spec.tile_width) {
                    m_topspec.tile_width  = 64;
                    m_topspec.tile_height = 64;
                    m_topspec.tile_depth  = 1;
                }
                m_topspec.attribute("wrapmodes", "black,black");
                m_topspec.attribute("textureformat", "Plain Texture");
                m_mip = true;
            }
        } else if (a.first == "TYPE") {
            m_topspec.set_format(TypeDesc(a.second));
        } else if (a.first == "PIXEL") {
            Strutil::extract_from_list_string(fvalue, a.second);
            fvalue.resize(m_topspec.nchannels);
        } else if (a.first.size() && a.second.size()) {
            parse_param(a.first, a.second, m_topspec);
        }
    }

    m_value.resize(m_topspec.pixel_bytes());  // default fills with 0's
    if (fvalue.size()) {
        // Convert float to the native type
        fvalue.resize(m_topspec.nchannels, 0.0f);
        convert_pixel_values(TypeFloat, fvalue.data(), m_topspec.format,
                             m_value.data(), m_topspec.nchannels);
    }

    bool ok = seek_subimage(0, 0);
    newspec = spec();
    return ok;
}



bool
NullInput::seek_subimage(int subimage, int miplevel)
{
    if (subimage == current_subimage() && miplevel == current_miplevel()) {
        return true;
    }

    if (subimage != 0)
        return false;  // We only make one subimage
    m_subimage = subimage;

    if (miplevel > 0 && !m_mip)
        return false;  // Asked for MIP levels but we aren't making them

    m_spec = m_topspec;
    for (m_miplevel = 0; m_miplevel < miplevel; ++m_miplevel) {
        if (m_spec.width == 1 && m_spec.height == 1 && m_spec.depth == 1)
            return false;  // Asked for more MIP levels than were available
        m_spec.width       = std::max(1, m_spec.width / 2);
        m_spec.height      = std::max(1, m_spec.height / 2);
        m_spec.depth       = std::max(1, m_spec.depth / 2);
        m_spec.full_width  = m_spec.width;
        m_spec.full_height = m_spec.height;
        m_spec.full_depth  = m_spec.depth;
    }
    return true;
}



bool
NullInput::read_native_scanline(int /*subimage*/, int /*miplevel*/, int /*y*/,
                                int /*z*/, void* data)
{
    size_t s = m_spec.pixel_bytes();
    OIIO_DASSERT(m_value.size() == s);
    for (int x = 0; x < m_spec.width; ++x)
        memcpy((char*)data + s * x, m_value.data(), s);
    return true;
}



bool
NullInput::read_native_tile(int /*subimage*/, int /*miplevel*/, int /*x*/,
                            int /*y*/, int /*z*/, void* data)
{
    size_t s = m_spec.pixel_bytes();
    OIIO_DASSERT(m_value.size() == s);
    for (size_t x = 0, e = m_spec.tile_pixels(); x < e; ++x)
        memcpy((char*)data + s * x, m_value.data(), s);
    return true;
}


OIIO_PLUGIN_NAMESPACE_END
