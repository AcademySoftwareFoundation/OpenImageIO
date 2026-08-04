// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// APV (Advanced Professional Video) reader, using the OpenAPV library.
//
// https://github.com/openapv/openapv
//
// APV is an all-intra professional video codec: every frame is coded
// independently, so a raw .apv bitstream is naturally addressable as a
// sequence of individual images. We expose one subimage per access unit
// (AU), decoding its primary frame. Auxiliary frames within an AU
// (preview, depth, alpha, non-primary) are not currently exposed.

#include <cstring>
#include <memory>
#include <vector>

#include <OpenImageIO/color.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>

#include "apv_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace apv_pvt;

class ApvInput final : public ImageInput {
public:
    ApvInput() { init(); }
    ~ApvInput() override { close(); }
    const char* format_name(void) const override { return "apv"; }
    int supports(string_view feature) const override
    {
        return feature == "ioproxy";
    }
    bool valid_file(Filesystem::IOProxy* ioproxy) const override;
    bool open(const std::string& name, ImageSpec& newspec) override;
    bool open(const std::string& name, ImageSpec& newspec,
              const ImageSpec& config) override;
    int current_subimage(void) const override
    {
        lock_guard lock(*this);
        return m_subimage;
    }
    bool seek_subimage(int subimage, int miplevel) override;
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;
    bool close() override;

private:
    struct AuIndexEntry {
        int64_t offset;  // file offset of the AU payload (past the size field)
        uint32_t size;   // byte size of the AU payload
    };

    std::string m_filename;
    std::vector<AuIndexEntry> m_au_index;
    oapvd_t m_decoder;
    int m_subimage;                   // Current subimage (== AU) index
    std::vector<uint16_t> m_pixels;   // Decoded, converted RGB(A)/Y pixels
    std::vector<unsigned char> m_au;  // Raw bytes of the current AU

    void init()
    {
        ioproxy_clear();
        m_filename.clear();
        m_au_index.clear();
        m_decoder  = nullptr;
        m_subimage = -1;
        m_pixels.clear();
        m_au.clear();
    }

    // Read the 4-byte big-endian AU size fields to build the AU index.
    bool index_aus(Filesystem::IOProxy* io);
    // Decode AU number `subimage` and fill m_spec and m_pixels.
    bool decode_au(int subimage);
};



// Obligatory material to make this a recognizable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageInput*
apv_input_imageio_create()
{
    return new ApvInput;
}

OIIO_EXPORT int apv_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
apv_imageio_library_version()
{
    return "OpenAPV " OIIO_STRINGIZE(OAPV_VER_APISET) "." OIIO_STRINGIZE(
        OAPV_VER_MAJOR) "." OIIO_STRINGIZE(OAPV_VER_MINOR) "." OIIO_STRINGIZE(OAPV_VER_PATCH);
}

OIIO_EXPORT const char* apv_input_extensions[] = { "apv", nullptr };

OIIO_PLUGIN_EXPORTS_END



bool
ApvInput::valid_file(Filesystem::IOProxy* ioproxy) const
{
    // A raw APV bitstream starts with a 4-byte AU size followed by the
    // 4-byte AU signature "aPv1".
    if (!ioproxy || ioproxy->mode() != Filesystem::IOProxy::Read)
        return false;
    unsigned char header[8];
    if (ioproxy->pread(header, sizeof(header), 0) != sizeof(header))
        return false;
    return memcmp(header + 4, apv_signature, sizeof(apv_signature)) == 0;
}



bool
ApvInput::open(const std::string& name, ImageSpec& newspec,
               const ImageSpec& config)
{
    ioproxy_retrieve_from_config(config);
    return open(name, newspec);
}



bool
ApvInput::open(const std::string& name, ImageSpec& newspec)
{
    m_filename = name;
    if (!ioproxy_use_or_open(name))
        return false;
    Filesystem::IOProxy* io = ioproxy();
    if (!valid_file(io)) {
        errorfmt("\"{}\" is not an APV bitstream", name);
        close();
        return false;
    }
    if (!index_aus(io)) {
        close();
        return false;
    }

    oapvd_cdesc_t cdesc;
    memset(&cdesc, 0, sizeof(cdesc));
    cdesc.threads = OAPV_CDESC_THREADS_AUTO;
    int err       = OAPV_OK;
    m_decoder     = oapvd_create(&cdesc, &err);
    if (m_decoder == nullptr) {
        errorfmt("Could not create APV decoder (error {})", err);
        close();
        return false;
    }

    if (!seek_subimage(0, 0)) {
        close();
        return false;
    }
    newspec = m_spec;
    return true;
}



bool
ApvInput::index_aus(Filesystem::IOProxy* io)
{
    int64_t pos        = 0;
    const int64_t size = io->size();
    while (pos + 4 <= size) {
        unsigned char szbuf[4];
        if (io->pread(szbuf, 4, pos) != 4) {
            errorfmt("Truncated AU size field at byte {}", pos);
            return false;
        }
        uint32_t au_size = (uint32_t(szbuf[0]) << 24)
                           | (uint32_t(szbuf[1]) << 16)
                           | (uint32_t(szbuf[2]) << 8) | uint32_t(szbuf[3]);
        pos += 4;
        if (au_size == 0 || pos + au_size > size) {
            errorfmt("Corrupt AU size {} at byte {}", au_size, pos - 4);
            return false;
        }
        m_au_index.push_back({ pos, au_size });
        pos += au_size;
    }
    if (m_au_index.empty()) {
        errorfmt("No access units found");
        return false;
    }
    return true;
}



bool
ApvInput::seek_subimage(int subimage, int miplevel)
{
    if (miplevel != 0 || subimage < 0 || subimage >= (int)m_au_index.size())
        return false;
    if (subimage == m_subimage)
        return true;
    if (!decode_au(subimage))
        return false;
    m_subimage = subimage;
    return true;
}



bool
ApvInput::decode_au(int subimage)
{
    Filesystem::IOProxy* io = ioproxy();
    const AuIndexEntry& au  = m_au_index[subimage];
    m_au.resize(au.size);
    if (io->pread(m_au.data(), au.size, au.offset) != au.size) {
        errorfmt("Could not read AU {}", subimage);
        return false;
    }

    oapv_au_info_t aui;
    if (oapvd_info(m_au.data(), (int)au.size, &aui) != OAPV_OK) {
        errorfmt("Could not parse AU {}", subimage);
        return false;
    }
    if (aui.num_frms <= 0 || aui.num_frms > OAPV_MAX_NUM_FRAMES) {
        errorfmt("AU {} has invalid frame count {}", subimage, aui.num_frms);
        return false;
    }

    // Find the primary frame in this AU.
    int primary = -1;
    for (int i = 0; i < aui.num_frms; i++) {
        if (aui.frm_info[i].pbu_type == OAPV_PBU_TYPE_PRIMARY_FRAME) {
            primary = i;
            break;
        }
    }
    if (primary < 0)
        primary = 0;  // fall back to the first frame
    const oapv_frm_info_t& finfo = aui.frm_info[primary];

    const int fmt      = OAPV_CS_GET_FORMAT(finfo.cs);
    const int bits     = OAPV_CS_GET_BIT_DEPTH(finfo.cs);
    const int channels = channels_for_format(fmt);
    if (channels == 0 || bits < 10 || bits > 16) {
        errorfmt("Unsupported APV color format ({}) or bit depth ({})", fmt,
                 bits);
        return false;
    }

    // Decode the AU. The decoder requires caller-allocated buffers for
    // every frame in the AU, not just the one we want.
    oapv_frms_t ofrms;
    memset(&ofrms, 0, sizeof(ofrms));
    ofrms.num_frms = aui.num_frms;
    for (int i = 0; i < aui.num_frms; i++) {
        ofrms.frm[i].imgb = imgb_create(aui.frm_info[i].w, aui.frm_info[i].h,
                                        aui.frm_info[i].cs);
        if (!ofrms.frm[i].imgb) {
            for (int j = 0; j < i; j++)
                ofrms.frm[j].imgb->release(ofrms.frm[j].imgb);
            errorfmt("Could not allocate frame buffers for AU {}", subimage);
            return false;
        }
    }
    oapv_bitb_t bitb;
    memset(&bitb, 0, sizeof(bitb));
    bitb.addr  = m_au.data();
    bitb.bsize = (int)au.size;
    bitb.ssize = (int)au.size;
    oapvd_stat_t stat;
    memset(&stat, 0, sizeof(stat));
    int ret = oapvd_decode(m_decoder, &bitb, &ofrms, nullptr, &stat);
    if (ret != OAPV_OK) {
        for (int i = 0; i < ofrms.num_frms; i++)
            ofrms.frm[i].imgb->release(ofrms.frm[i].imgb);
        errorfmt("APV decode failed for AU {} (error {})", subimage, ret);
        return false;
    }

    // Build the spec for this subimage.
    m_spec = ImageSpec(finfo.w, finfo.h, channels, TypeDesc::UINT16);
    m_spec.attribute("oiio:BitsPerSample", bits);
    m_spec.attribute("apv:profile", finfo.profile_idc);
    m_spec.attribute("apv:level", finfo.level_idc);
    // N.B. Take the color description from the decode's frame info, not
    // from oapvd_info(): OpenAPV's lightweight header probe misparses the
    // color description fields (verified against its own reference
    // encoder; the full decoder parses them correctly).
    const oapv_frm_info_t& dinfo = stat.aui.frm_info[primary];
    float kr = 0.2126f, kb = 0.0722f;  // default BT.709
    bool full_range = false;
    if (dinfo.color_description_present_flag) {
        matrix_luma_coefficients(dinfo.matrix_coefficients, kr, kb);
        full_range = dinfo.full_range_flag;
        // The pixels we return are RGB (matrix 0) and full range after
        // conversion; the primaries and transfer carry over unchanged.
        const int cicp[4] = { dinfo.color_primaries,
                              dinfo.transfer_characteristics, 0 /* RGB */,
                              1 /* full range */ };
        m_spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), cicp);
        const ColorConfig& colorconfig(ColorConfig::default_colorconfig());
        string_view interop_id = colorconfig.get_color_interop_id(cicp);
        if (!interop_id.empty())
            m_spec.attribute("oiio:ColorSpace", interop_id);
        m_spec.attribute("apv:matrix_coefficients",
                         (int)dinfo.matrix_coefficients);
        m_spec.attribute("apv:full_range_flag", dinfo.full_range_flag);
    }

    // Convert the primary frame to interleaved RGB(A)/Y uint16 pixels.
    const oapv_imgb_t* imgb = ofrms.frm[primary].imgb;
    const int w = finfo.w, h = finfo.h;
    const float maxval = float((1 << bits) - 1);
    const float lo     = full_range ? 0.0f : float(16 << (bits - 8));
    const float yrange = full_range ? maxval : float(219 << (bits - 8));
    const float crange = full_range ? maxval : float(224 << (bits - 8));
    const float chalf  = float(1 << (bits - 1));
    const float kg     = 1.0f - kr - kb;
    const int subx     = (fmt == OAPV_CF_YCBCR420 || fmt == OAPV_CF_YCBCR422);
    const int suby     = (fmt == OAPV_CF_YCBCR420);

    m_pixels.resize(size_t(w) * h * channels);
    auto plane = [&](int p) {
        return reinterpret_cast<const uint16_t*>(imgb->a[p]);
    };
    auto pstride = [&](int p) { return imgb->s[p] / 2; };

    for (int y = 0; y < h; y++) {
        uint16_t* dst        = m_pixels.data() + size_t(y) * w * channels;
        const uint16_t* yrow = plane(0) + size_t(y) * pstride(0);
        if (channels == 1) {
            for (int x = 0; x < w; x++) {
                float Y = (yrow[x] - lo) / yrange;
                dst[x]  = uint16_t(clamp(Y, 0.0f, 1.0f) * 65535.0f + 0.5f);
            }
            continue;
        }
        const int cy          = suby ? (y >> 1) : y;
        const uint16_t* cbrow = plane(1) + size_t(cy) * pstride(1);
        const uint16_t* crrow = plane(2) + size_t(cy) * pstride(2);
        const uint16_t* arow = channels == 4 ? plane(3) + size_t(y) * pstride(3)
                                             : nullptr;
        const int cw         = imgb->w[1];
        for (int x = 0; x < w; x++) {
            float cb, cr;
            if (subx) {
                // Simple co-sited linear interpolation of chroma.
                // ponytail: nearest+average; proper resampling filters can
                // come later if quality demands.
                int cx = x >> 1;
                if (x & 1) {
                    int cx1 = std::min(cx + 1, cw - 1);
                    cb      = 0.5f * (cbrow[cx] + cbrow[cx1]);
                    cr      = 0.5f * (crrow[cx] + crrow[cx1]);
                } else {
                    cb = cbrow[cx];
                    cr = crrow[cx];
                }
            } else {
                cb = cbrow[x];
                cr = crrow[x];
            }
            float Y      = (yrow[x] - lo) / yrange;
            float pb     = (cb - chalf) / crange;
            float pr     = (cr - chalf) / crange;
            float r      = Y + 2.0f * (1.0f - kr) * pr;
            float b      = Y + 2.0f * (1.0f - kb) * pb;
            float g      = (Y - kr * r - kb * b) / kg;
            uint16_t* px = dst + size_t(x) * channels;
            px[0]        = uint16_t(clamp(r, 0.0f, 1.0f) * 65535.0f + 0.5f);
            px[1]        = uint16_t(clamp(g, 0.0f, 1.0f) * 65535.0f + 0.5f);
            px[2]        = uint16_t(clamp(b, 0.0f, 1.0f) * 65535.0f + 0.5f);
            if (arow)  // alpha is carried full range
                px[3] = scale_to_16bits(arow[x], bits);
        }
    }

    for (int i = 0; i < ofrms.num_frms; i++)
        ofrms.frm[i].imgb->release(ofrms.frm[i].imgb);
    return true;
}



bool
ApvInput::read_native_scanline(int subimage, int miplevel, int y, int /*z*/,
                               void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;
    if (y < 0 || y >= m_spec.height)
        return false;
    size_t scanline_bytes = size_t(m_spec.width) * m_spec.nchannels
                            * sizeof(uint16_t);
    memcpy(data, m_pixels.data() + size_t(y) * m_spec.width * m_spec.nchannels,
           scanline_bytes);
    return true;
}



bool
ApvInput::close()
{
    if (m_decoder) {
        oapvd_delete(m_decoder);
        m_decoder = nullptr;
    }
    init();
    return true;
}

OIIO_PLUGIN_NAMESPACE_END
