// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <string>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

#define DBG if (0)

//
// Documentation on the PNM formats can be found at:
// http://netpbm.sourceforge.net/doc/pbm.html  (B&W)
// http://netpbm.sourceforge.net/doc/pgm.html  (grey)
// http://netpbm.sourceforge.net/doc/ppm.html  (color)
// http://netpbm.sourceforge.net/doc/pam.html  (base format)
//

enum PNMType { P1, P2, P3, P4, P5, P6, Pf, PF };



struct PNMBasicInfo {
    PNMType type;
    int width;
    int height;
};



class PNMInput final : public ImageInput {
public:
    PNMInput() { init(); }
    ~PNMInput() override { close(); }
    const char* format_name(void) const override { return "pnm"; }
    int supports(string_view feature) const override
    {
        return feature == "ioproxy";
    }
    bool open(const std::string& name, ImageSpec& newspec) override;
    bool open(const std::string& name, ImageSpec& spec,
              const ImageSpec& config) override;
    bool close() override;
    int current_subimage(void) const override { return 0; }
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;
    bool valid_file(Filesystem::IOProxy* ioproxy) const override;

private:
    PNMType m_pnm_type;
    int m_max_val;
    float m_scaling_factor;
    std::vector<char> m_file_contents;
    string_view m_remaining;
    string_view m_after_header;
    int m_y_next;
    bool m_pfm_flip;

    void init()
    {
        m_file_contents.shrink_to_fit();
        ioproxy_clear();
        m_y_next = 0;
    }

    bool read_file_scanline(void* data, int y);
    bool read_file_header();

    static string_view read_header_to_buffer(std::vector<char>& buffer,
                                             Filesystem::IOProxy* io);
    static string_view append_remainder_to_buffer(std::vector<char>& buffer,
                                                  Filesystem::IOProxy* io,
                                                  string_view remaining);

    template<typename T> bool nextVal(T& val);

    template<class T>
    bool ascii_to_raw(T* write, imagesize_t nvals, T max, bool invert = false);
};



// Obligatory material to make this a recognizable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageInput*
pnm_input_imageio_create()
{
    return new PNMInput;
}

OIIO_EXPORT int pnm_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
pnm_imageio_library_version()
{
    return nullptr;
}

OIIO_EXPORT const char* pnm_input_extensions[] = { "ppm", "pgm", "pbm",
                                                   "pnm", "pfm", nullptr };

OIIO_PLUGIN_EXPORTS_END


// 1KiB approximate limit on header size
static const imagesize_t max_pnm_header_size = 1024;

// 1GiB rough limit on file size to avoid loading arbitrarily
// large files into memory
static const imagesize_t max_pnm_file_size = 1024 * 1024 * 1024;



inline static void
skip_header_comments(string_view& header)
{
    while (header.size() && Strutil::parse_char(header, '#'))
        Strutil::parse_line(header);
}



template<typename T>
inline static bool
parse_next_header_value(string_view& header, T& val)
{
    skip_header_comments(header);
    return Strutil::parse_value(header, val);
}



template<class T>
inline void
invert(const T* read, T* write, imagesize_t nvals)
{
    for (imagesize_t i = 0; i < nvals; i++)
        write[i] = std::numeric_limits<T>::max() - read[i];
}



template<typename T>
bool
PNMInput::nextVal(T& val)
{
    return parse_next_header_value(m_remaining, val);
}



template<class T>
bool
PNMInput::ascii_to_raw(T* write, imagesize_t nvals, T max, bool invert)
{
    if (max) {
        for (imagesize_t i = 0; i < nvals; i++) {
            int tmp;
            if (!nextVal(tmp))
                return false;
            write[i] = std::min((int)max, tmp) * std::numeric_limits<T>::max()
                       / max;
        }
        if (invert)
            for (imagesize_t i = 0; i < nvals; i++)
                write[i] = std::numeric_limits<T>::max() - write[i];
    } else {
        for (imagesize_t i = 0; i < nvals; i++)
            write[i] = std::numeric_limits<T>::max();
    }
    return true;
}



template<class T>
inline void
raw_to_raw(const T* read, T* write, imagesize_t nvals, T max)
{
    if (max)
        for (imagesize_t i = 0; i < nvals; i++) {
            int tmp  = read[i];
            write[i] = std::min((int)max, tmp) * std::numeric_limits<T>::max()
                       / max;
        }
    else
        for (imagesize_t i = 0; i < nvals; i++)
            write[i] = std::numeric_limits<T>::max();
}



inline void
unpack(const unsigned char* read, unsigned char* write, imagesize_t size)
{
    imagesize_t w = 0, r = 0;
    unsigned char bit = 0x7, byte = 0;
    for (imagesize_t x = 0; x < size; x++) {
        if (bit == 0x7)
            byte = ~read[r++];
        write[w++] = 0 - ((byte & (1 << bit)) >> bit);  //assign expanded bit
        bit        = (bit - 1) & 0x7;                   // limit bit to [0; 8[
    }
}



inline void
unpack_floats(const unsigned char* read, float* write, imagesize_t numsamples,
              float scaling_factor)
{
    float* read_floats = (float*)read;

    if ((scaling_factor < 0 && bigendian())
        || (scaling_factor > 0 && littleendian())) {
        swap_endian(read_floats, numsamples);
    }

    float absfactor = fabs(scaling_factor);
    for (imagesize_t i = 0; i < numsamples; i++) {
        write[i] = absfactor * read_floats[i];
    }
}



static std::optional<PNMBasicInfo>
read_type_and_resolution(string_view& header)
{
    PNMType type;

    if (!Strutil::parse_char(header, 'P') || header.empty())
        return std::nullopt;

    switch (header.front()) {
    case '1': type = P1; break;
    case '2': type = P2; break;
    case '3': type = P3; break;
    case '4': type = P4; break;
    case '5': type = P5; break;
    case '6': type = P6; break;
    case 'f': type = Pf; break;
    case 'F': type = PF; break;
    default: return std::nullopt;
    }
    header.remove_prefix(1);

    //Size
    int width, height;
    if (!parse_next_header_value(header, width))
        return std::nullopt;
    if (!parse_next_header_value(header, height))
        return std::nullopt;

    return PNMBasicInfo { type, width, height };
}



bool
PNMInput::read_file_scanline(void* data, int y)
{
    DBG std::cerr << "PNMInput::read_file_scanline(" << y << ")\n";
    if (y < m_y_next) {
        // If being asked to backtrack to an earlier scanline, reset all the
        // way to the beginning, right after the header.
        m_remaining = m_after_header;
        m_y_next    = 0;
    }

    std::vector<unsigned char> buf;
    int nsamples = m_spec.width * m_spec.nchannels;
    bool good    = true;
    // If y is farther ahead, skip scanlines to get to it
    for (; good && m_y_next <= y; ++m_y_next) {
        // PFM files are bottom-to-top, so we need to seek to the right spot
        if (m_pnm_type == PF || m_pnm_type == Pf) {
            if (m_pfm_flip) {
                int file_scanline = m_spec.height - 1 - (y - m_spec.y);
                auto offset       = file_scanline * m_spec.scanline_bytes();
                m_remaining       = m_after_header.substr(offset);
            }
        }

        if ((m_pnm_type >= P4 && m_pnm_type <= P6) || m_pnm_type == PF
            || m_pnm_type == Pf) {
            int numbytes;
            if (m_pnm_type == P4)
                numbytes = (m_spec.width + 7) / 8;
            else if (m_pnm_type == PF || m_pnm_type == Pf)
                numbytes = m_spec.nchannels * 4 * m_spec.width;
            else
                numbytes = m_spec.scanline_bytes();
            if (size_t(numbytes) > m_remaining.size()) {
                errorfmt("Premature end of file");
                return false;
            }
            buf.assign(m_remaining.begin(), m_remaining.begin() + numbytes);

            m_remaining.remove_prefix(numbytes);
        }

        switch (m_pnm_type) {
        //Ascii
        case P1:
            good &= ascii_to_raw((unsigned char*)data, nsamples,
                                 (unsigned char)m_max_val, true);
            break;
        case P2:
        case P3:
            if (m_max_val > std::numeric_limits<unsigned char>::max())
                good &= ascii_to_raw((unsigned short*)data, nsamples,
                                     (unsigned short)m_max_val);
            else
                good &= ascii_to_raw((unsigned char*)data, nsamples,
                                     (unsigned char)m_max_val);
            break;
        //Raw
        case P4: unpack(&buf[0], (unsigned char*)data, nsamples); break;
        case P5:
        case P6:
            if (m_max_val > std::numeric_limits<unsigned char>::max()) {
                if (littleendian())
                    swap_endian((unsigned short*)&buf[0], nsamples);
                raw_to_raw((unsigned short*)&buf[0], (unsigned short*)data,
                           nsamples, (unsigned short)m_max_val);
            } else {
                raw_to_raw((unsigned char*)&buf[0], (unsigned char*)data,
                           nsamples, (unsigned char)m_max_val);
            }
            break;
        //Floating point
        case Pf:
        case PF:
            unpack_floats(&buf[0], (float*)data, nsamples, m_scaling_factor);
            break;
        default: return false;
        }
    }
    return good;
}



bool
PNMInput::read_file_header()
{
    int width, height;

    if (auto basic_info = read_type_and_resolution(m_remaining)) {
        m_pnm_type = basic_info->type;
        width      = basic_info->width;
        height     = basic_info->height;
    } else {
        return false;
    }

    if (m_pnm_type != PF && m_pnm_type != Pf) {
        // Max Val
        if (m_pnm_type != P1 && m_pnm_type != P4) {
            if (!nextVal(m_max_val))
                return false;
        } else
            m_max_val = 1;

        //Space before content
        if (!(m_remaining.size() && Strutil::isspace(m_remaining.front())))
            return false;
        m_remaining.remove_prefix(1);

        m_spec = ImageSpec(width, height,
                           (m_pnm_type == P3 || m_pnm_type == P6) ? 3 : 1,
                           (m_max_val > 255) ? TypeDesc::UINT16
                                             : TypeDesc::UINT8);
        m_spec.attribute("pnm:binary",
                         (m_pnm_type >= P1 && m_pnm_type <= P3) ? 0 : 1);
        int bps = int(ceilf(logf(m_max_val + 1) / logf(2)));
        if (bps < 8)
            m_spec.attribute("oiio:BitsPerSample", bps);
    } else {
        //Read scaling factor
        if (!nextVal(m_scaling_factor))
            return false;

        //Space before content
        if (!(m_remaining.size() && Strutil::isspace(m_remaining.front())))
            return false;
        m_remaining.remove_prefix(1);

        m_spec = ImageSpec(width, height, m_pnm_type == PF ? 3 : 1,
                           TypeDesc::FLOAT);
        m_spec.attribute("pnm:bigendian", m_scaling_factor < 0 ? 0 : 1);
        m_spec.attribute("pnm:binary", 1);
    }
    m_spec.set_colorspace("Rec709");
    return true;
}



// Read only enough of the file to contain the header (at momst 1KB) into buffer
string_view
PNMInput::read_header_to_buffer(std::vector<char>& buffer,
                                Filesystem::IOProxy* io)
{
    imagesize_t header_size = std::min(static_cast<imagesize_t>(io->size()),
                                       max_pnm_header_size);
    buffer.resize(header_size);
    io->pread(buffer.data(), header_size, 0);
    return string_view(buffer.data(), buffer.size());
}



// buffer contains at most the first 1K of the file. At this point, we know
// the file seems valid. Read the rest in, appending to what we have, and
// return the adjusted string_view of the contents.
string_view
PNMInput::append_remainder_to_buffer(std::vector<char>& buffer,
                                     Filesystem::IOProxy* io,
                                     string_view remaining)
{
    // Assume we've already read the header into buffer
    imagesize_t header_size    = buffer.size();
    imagesize_t full_size      = std::min(static_cast<imagesize_t>(io->size()),
                                          max_pnm_file_size);
    ptrdiff_t remaining_offset = remaining.data() - buffer.data();

    buffer.resize(full_size);
    io->pread(buffer.data() + header_size, full_size - header_size,
              header_size);

    string_view result { buffer.data(), buffer.size() };
    result.remove_prefix(remaining_offset);
    return result;
}



bool
PNMInput::valid_file(Filesystem::IOProxy* ioproxy) const
{
    DBG std::cout << "PNMInput::valid_file()\n";

    if (!ioproxy || ioproxy->mode() != Filesystem::IOProxy::Mode::Read)
        return false;

    std::vector<char> buffer;
    string_view header = read_header_to_buffer(buffer, ioproxy);

    int width, height;
    if (auto basic_info = read_type_and_resolution(header)) {
        width  = basic_info->width;
        height = basic_info->height;
    } else {
        return false;
    }

    // Per spec, width and height must both be positive integers.
    // No formal upper limit is placed on width/height, but for sanity,
    // assume dimensions should be no greater than 2^12
    if (width < 0 || 4096 < width)
        return false;
    if (height < 0 || 4096 < height)
        return false;

    DBG std::cout << "PNMInput::valid_file returned true\n";

    return true;
}



bool
PNMInput::open(const std::string& name, ImageSpec& newspec,
               const ImageSpec& config)
{
    ioproxy_retrieve_from_config(config);

    if (!open(name, newspec)) {
        errorfmt("Could not parse spec for file \"%s\"", name);
        return false;
    }

    m_pfm_flip = config.get_int_attribute("pnm:pfmflip", 1);

    return true;
}



bool
PNMInput::open(const std::string& name, ImageSpec& newspec)
{
    if (!ioproxy_use_or_open(name))
        return false;

    // Read the whole file's contents into m_file_contents
    Filesystem::IOProxy* m_io = ioproxy();
    m_remaining               = read_header_to_buffer(m_file_contents, m_io);
    m_pfm_flip                = false;

    if (!read_file_header())
        return false;

    if (!check_open(m_spec))  // check for apparently invalid values
        return false;

    m_remaining    = append_remainder_to_buffer(m_file_contents, m_io,
                                                m_remaining);
    m_after_header = m_remaining;
    newspec        = m_spec;

    return true;
}



bool
PNMInput::close()
{
    init();
    return true;
}



bool
PNMInput::read_native_scanline(int subimage, int miplevel, int y, int z,
                               void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;

    if (z)
        return false;
    if (!read_file_scanline(data, y))
        return false;
    return true;
}


OIIO_PLUGIN_NAMESPACE_END
