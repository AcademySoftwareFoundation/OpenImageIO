// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <vector>
#include <iostream>

#include "ojph_arg.h"
#include <ojph_mem.h>
#include <ojph_file.h>
#include <ojph_codestream.h>
#include <ojph_params.h>
#include <ojph_message.h>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>


OIIO_PLUGIN_NAMESPACE_BEGIN



struct size_list_interpreter : public ojph::cli_interpreter::arg_inter_base
{
  size_list_interpreter(const int max_num_elements, int& num_elements,
                        ojph::size* list)
  : max_num_eles(max_num_elements), sizelist(list), num_eles(num_elements)
  {}

  virtual void operate(const char *str)
  {
    const char *next_char = str;
    num_eles = 0;
    do
    {
      if (num_eles)
      {
        if (*next_char != ',') //separate sizes by a comma
          throw "sizes in a sizes list must be separated by a comma";
        next_char++;
      }

      char *endptr;
      sizelist[num_eles].w = (ojph::ui32)strtoul(next_char, &endptr, 10);
      if (endptr == next_char)
        throw "size number is improperly formatted";
      next_char = endptr;
      if (*next_char != ',')
        throw "size must have a "","" between the two numbers";
      next_char++;
      sizelist[num_eles].h = (ojph::ui32)strtoul(next_char, &endptr, 10);
      if (endptr == next_char)
        throw "number is improperly formatted";
      next_char = endptr;


      ++num_eles;
    }
    while (*next_char == ',' && num_eles < max_num_eles);
    if (num_eles < max_num_eles)
    {
      if (*next_char)
        throw "size elements must separated by a "",""";
    }
    else if (*next_char)
        throw "there are too many elements in the size list";
  }

  const int max_num_eles;
  ojph::size* sizelist;
  int& num_eles;
};


class JphOutput final : public ImageOutput {
public:
    JphOutput() { init(); }
    ~JphOutput() override { close(); }
    const char* format_name(void) const override { return "jph"; }
    int supports(string_view feature) const override
    {
        return feature == "alpha" || feature == "ioproxy" || feature == "tiles";
        // FIXME: we should support Exif/IPTC, but currently don't.
    }
    bool open(const std::string& name, const ImageSpec& spec,
              OpenMode mode = Create) override;
    bool close() override;
    bool write_scanline(int y, int z, TypeDesc format, const void* data,
                        stride_t xstride) override;
    bool write_tile(int x, int y, int z, TypeDesc format, const void* data,
                    stride_t xstride, stride_t ystride,
                    stride_t zstride) override;

private:
    std::string m_filename;
    // opj_cparameters_t m_compression_parameters;
    ojph::j2c_outfile* m_image;
    ojph::codestream* m_stream;
    unsigned int m_dither;
    bool m_convert_alpha;  //< Do we deassociate alpha?
    int output_depth;
    std::vector<unsigned char> m_tilebuffer;
    std::vector<unsigned char> m_scratch;

    void init(void)
    {
        m_image         = NULL;
        m_stream        = NULL;
        m_convert_alpha = true;
        ioproxy_clear();
    }

    ojph::j2c_outfile* create_jph_image();

    // void init_components(opj_image_cmptparm_t* components, int precision);

    void destroy_stream()
    {
        if (m_stream) {
            // opj_stream_destroy(m_stream);
            delete m_stream;
            m_stream = NULL;
        }
    }

    bool save_image();

    template<typename T> void write_scanline(int y, int z, const void* data);

};

// Obligatory material to make this a recognizable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
openjph_output_imageio_create()
{
    return new JphOutput;
}

OIIO_EXPORT int openjph_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
openjph_imageio_library_version()
{
    return ustring::fmtformat("OpenJph {}.{}.{}", OPENJPH_VERSION_MAJOR, OPENJPH_VERSION_MINOR, OPENJPH_VERSION_PATCH).c_str();
}


OIIO_EXPORT const char* openjph_output_extensions[] = { "j2c", 
                                                         nullptr };

OIIO_PLUGIN_EXPORTS_END


bool
JphOutput::open(const std::string& name, const ImageSpec& spec,
                     OpenMode mode)
{
    if (!check_open(mode, spec, { 0, 1 << 20, 0, 1 << 20, 0, 1, 0, 4 },
                    uint64_t(OpenChecks::Disallow2Channel)))
        return false;

    m_filename = name;

    m_dither        = (m_spec.format == TypeDesc::UINT8)
                          ? m_spec.get_int_attribute("oiio:dither", 0)
                          : 0;

    m_convert_alpha = m_spec.alpha_channel != -1;

    // If user asked for tiles -- which this format doesn't support, emulate
    // it by buffering the whole image.
    if (m_spec.tile_width && m_spec.tile_height)
        m_tilebuffer.resize(m_spec.image_bytes());

    m_image = create_jph_image();


    return true;
}



template<class T>
static void
deassociateAlpha(T* data, int size, int channels, int alpha_channel,
                 float gamma)
{
    unsigned int max = std::numeric_limits<T>::max();
    if (gamma == 1) {
        for (int x = 0; x < size; ++x, data += channels)
            if (data[alpha_channel])
                for (int c = 0; c < channels; c++)
                    if (c != alpha_channel) {
                        unsigned int f = data[c];
                        f              = (f * max) / data[alpha_channel];
                        data[c]        = (T)std::min(max, f);
                    }
    } else {
        for (int x = 0; x < size; ++x, data += channels)
            if (data[alpha_channel]) {
                // See associateAlpha() for an explanation.
                float alpha_deassociate = pow((float)max / data[alpha_channel],
                                              gamma);
                for (int c = 0; c < channels; c++)
                    if (c != alpha_channel)
                        data[c] = static_cast<T>(std::min(
                            max, (unsigned int)(data[c] * alpha_deassociate)));
            }
    }
}



bool
JphOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                               stride_t xstride)
{
    y -= m_spec.y;
    if (y > m_spec.height) {
        errorfmt("Attempt to write too many scanlines to {}", m_filename);
        return false;
    }

    m_spec.auto_stride(xstride, format, spec().nchannels);
    const void* origdata = data;
    data = to_native_scanline(format, data, xstride, m_scratch, m_dither, y, z);
    if (data == origdata) {
        m_scratch.assign((unsigned char*)data,
                         (unsigned char*)data + m_spec.scanline_bytes());
        data = &m_scratch[0];
    }

    // JPEG-2000 specifically dictates unassociated (un-"premultiplied") alpha
    // Disabling this for now, since we really need a file format like jph that
    // can record whether the RGB values have had a premult applied.
    /* if (m_convert_alpha) {
        if (m_spec.format == TypeDesc::UINT16)
            deassociateAlpha((unsigned short*)data, m_spec.width,
                             m_spec.nchannels, m_spec.alpha_channel, 2.2f);
        else
            deassociateAlpha((unsigned char*)data, m_spec.width,
                             m_spec.nchannels, m_spec.alpha_channel, 2.2f);
    } */

    if (m_spec.format == TypeDesc::UINT8)
        write_scanline<uint8_t>(y, z, data);
    else
        write_scanline<uint16_t>(y, z, data);
    if (y == m_spec.height - 1)
        save_image();

    return true;
}



bool
JphOutput::write_tile(int x, int y, int z, TypeDesc format,
                           const void* data, stride_t xstride, stride_t ystride,
                           stride_t zstride)
{
    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer(x, y, z, format, data, xstride, ystride,
                                     zstride, &m_tilebuffer[0]);
}



bool
JphOutput::close()
{
    if (!m_stream) {  // Already closed
        return true;
    }

    bool ok = true;
    if (m_spec.tile_width) {
        // We've been emulating tiles; now dump as scanlines.
        OIIO_ASSERT(m_tilebuffer.size());
        ok &= write_scanlines(m_spec.y, m_spec.y + m_spec.height, 0,
                              m_spec.format, &m_tilebuffer[0]);
        std::vector<unsigned char>().swap(m_tilebuffer);
    }

    if (m_image) {
        m_image->close();
        delete m_image;
        m_image = NULL;
    }
    destroy_stream();
    init();
    return ok;
}



bool
JphOutput::save_image()
{


    // m_stream = opj_stream_default_create(false /* is_input */);
    m_stream->flush();
    m_stream->close();
    destroy_stream();

    return true;
    if (!m_stream) {
        errorfmt("Failed write jph::save_image");
        return false;
    }

    return true;
}


ojph::j2c_outfile*
JphOutput::create_jph_image()
{
    m_stream = new ojph::codestream;
    ojph::param_siz siz = m_stream->access_siz();
    siz.set_image_extent(ojph::point(m_spec.width,
          m_spec.height));
    

    // TODO
    /*
    OPJ_COLOR_SPACE color_space = OPJ_CLRSPC_SRGB;
    if (m_spec.nchannels == 1)
        color_space = OPJ_CLRSPC_GRAY;
    */

    int precision          = 16;
    const ParamValue* prec = m_spec.find_attribute("oiio:BitsPerSample",
                                                   TypeDesc::INT);
    bool is_signed = false;

    if (prec)
        precision = *(int*)prec->data();
    else if (m_spec.format == TypeDesc::UINT8
             || m_spec.format == TypeDesc::INT8)
        precision = 8;
    
    switch (m_spec.format.basetype) {
    case TypeDesc::INT8:
    case TypeDesc::UINT8:
        precision = 8;
        is_signed = false;
        break;
    case TypeDesc::FLOAT:
        precision = 32;
        is_signed = true;
        break;
    case TypeDesc::HALF:
        is_signed = true;
        break;
    case TypeDesc::DOUBLE:
            throw "OpenJPH::Write Double is not currently supported.";
    default:
        break;
    }
    
    output_depth = m_spec.get_int_attribute("jph:bit_depth", precision);

    siz.set_num_components(m_spec.nchannels);
    ojph::point subsample(1,1); // Default subsample
    for (ojph::ui32 c = 0; c < m_spec.nchannels; ++c)
          siz.set_component(c, subsample, output_depth, is_signed);

    ojph::size tile_size(0, 0);
    ojph::point tile_offset(0, 0);
    ojph::point image_offset(0, 0);
    siz.set_image_offset(image_offset);
    siz.set_tile_size(tile_size);
    siz.set_tile_offset(tile_offset);
    ojph::param_cod cod = m_stream->access_cod();

    std::string block_args = m_spec.get_string_attribute("jph:block_size", "64,64");
    std::stringstream ss(block_args);
    char comma;
    int block_size_x, block_size_y;
    ss >> block_size_x >> comma >> block_size_y;

    cod.set_block_dims(block_size_x, block_size_y);
    cod.set_color_transform(true);
    
    int num_precincts = -1;
    const int max_precinct_sizes = 33; //maximum number of decompositions is 32
    ojph::size precinct_size[max_precinct_sizes];
    std::string precinct_size_args = m_spec.get_string_attribute("jph:precincts", "undef");
    if (precinct_size_args != "undef"){
        size_list_interpreter sizelist(max_precinct_sizes, num_precincts,
                                    precinct_size);
        sizelist.operate(precinct_size_args.c_str());

        if (num_precincts != -1)
            cod.set_precinct_size(num_precincts, precinct_size);
    }

    std::string progression_order = m_spec.get_string_attribute("jph:prog_order", "RPCL");

    cod.set_progression_order(progression_order.c_str());

    cod.set_reversible(true);
    const ParamValue *compressionparams = m_spec.find_attribute("compression", TypeString);
    if (compressionparams){
        std::string compressionparms = compressionparams->get_string();
        // Sadly cannot use decode_compression_metadata since we are asking for a float param to be returned.
        auto comp_and_value = Strutil::splitsv(compressionparms, ":");
        if (comp_and_value.size() >= 2){
            string_view comp = comp_and_value[0];
            if (comp == "qstep"){
                float quantization_step = Strutil::stof(comp_and_value[1]);
                cod.set_reversible(false);
                m_stream->access_qcd().set_irrev_quant(quantization_step);
            }
        }
    }

    cod.set_num_decomposition(m_spec.get_int_attribute("jph:num_decomps", 5));
    m_stream->set_planar(false);
    //m_image = opj_image_create(m_spec.nchannels, &component_params[0],
    //                           color_space);

    // Floating point support
    if (m_spec.format.basetype == TypeDesc::HALF || m_spec.format.basetype == TypeDesc::FLOAT){
        // If we are treating the J2H file format as floating point
        // We need to enable the NLT type3 and the file needs to be signed, we only support half and float
        // not double (yet).
        ojph::param_nlt nlt = m_stream->access_nlt();
        nlt.set_type3_transformation(65535, true);
    }


    m_image = new ojph::j2c_outfile;
    //ojph::j2c_outfile j2c_file;
    m_image->open(m_filename.c_str());
    m_stream->write_headers(m_image); //, "test comment", 1);

    return m_image;
}


template<typename T>
void
JphOutput::write_scanline(int y, int /*z*/, const void* data)
{
    int bits                  = sizeof(T) * 8;
    const T* scanline         = static_cast<const T*>(data);
    ojph::ui32 next_comp = 0;
    ojph::line_buf* cur_line = m_stream->exchange(NULL, next_comp);
    for (int c = 0; c < m_spec.nchannels; ++c) {
        assert(c == next_comp);
        for (int i = 0, j = c; i < m_spec.width; i++) {
            unsigned int val = scanline[j];
            j += m_spec.nchannels;
            if (bits != output_depth)
                val = bit_range_convert(val, bits, output_depth);
            cur_line->i32[i] = val;
        }
        cur_line = m_stream->exchange (cur_line, next_comp);
    }

}


OIIO_PLUGIN_NAMESPACE_END
