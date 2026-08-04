// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// APV (Advanced Professional Video) writer, using the OpenAPV library.
//
// Each subimage is encoded as one access unit (AU) containing a single
// primary frame, so appending subimages produces a valid multi-frame APV
// bitstream.

#include <cstring>
#include <memory>
#include <vector>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>

#include "apv_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace apv_pvt;

class ApvOutput final : public ImageOutput {
public:
    ApvOutput() { init(); }
    ~ApvOutput() override { close(); }
    const char* format_name(void) const override { return "apv"; }
    int supports(string_view feature) const override
    {
        return (feature == "alpha" || feature == "multiimage"
                || feature == "appendsubimage" || feature == "ioproxy");
    }
    bool open(const std::string& name, const ImageSpec& spec,
              OpenMode mode = Create) override;
    bool write_scanline(int y, int z, TypeDesc format, const void* data,
                        stride_t xstride) override;
    bool write_scanlines(int ybegin, int yend, int z, TypeDesc format,
                         const void* data, stride_t xstride,
                         stride_t ystride) override;
    bool close() override;

private:
    std::string m_filename;
    std::vector<float> m_staging;  // Interleaved float pixels for one frame
    int m_color_format;            // OAPV_CF_* for the frame being written
    int m_bits;                    // encoded bit depth
    int m_profile_idc;
    bool m_dirty;  // any scanlines written since open?

    void init()
    {
        ioproxy_clear();
        m_filename.clear();
        m_staging.clear();
        m_color_format = OAPV_CF_UNKNOWN;
        m_bits         = 0;
        m_profile_idc  = 0;
        m_dirty        = false;
    }

    bool setup_profile();
    bool finalize_subimage();
};



// Obligatory material to make this a recognizable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
apv_output_imageio_create()
{ return new ApvOutput; }

OIIO_EXPORT const char* apv_output_extensions[] = { "apv", nullptr };

OIIO_PLUGIN_EXPORTS_END



// Map a profile name to (profile_idc, color format, bit depth).
// The supported profile names match OpenAPV's own option strings.
struct ApvProfileDef {
    const char* name;
    int idc;
    int color_format;
    int bits;
};
static const ApvProfileDef apv_profiles[] = {
    { "422-10", OAPV_PROFILE_422_10, OAPV_CF_YCBCR422, 10 },
    { "422-12", OAPV_PROFILE_422_12, OAPV_CF_YCBCR422, 12 },
    { "444-10", OAPV_PROFILE_444_10, OAPV_CF_YCBCR444, 10 },
    { "444-12", OAPV_PROFILE_444_12, OAPV_CF_YCBCR444, 12 },
    { "4444-10", OAPV_PROFILE_4444_10, OAPV_CF_YCBCR4444, 10 },
    { "4444-12", OAPV_PROFILE_4444_12, OAPV_CF_YCBCR4444, 12 },
    { "400-10", OAPV_PROFILE_400_10, OAPV_CF_YCBCR400, 10 },
};



bool
ApvOutput::setup_profile()
{
    string_view profile = m_spec.get_string_attribute("apv:profile", "");
    if (profile.empty()) {
        if (m_spec.nchannels == 1)
            profile = "400-10";
        else if (m_spec.nchannels == 4)
            profile = "4444-10";
        else
            profile = "422-10";
    }
    for (const auto& p : apv_profiles) {
        if (profile == p.name) {
            m_profile_idc  = p.idc;
            m_color_format = p.color_format;
            m_bits         = p.bits;
            const int want = channels_for_format(p.color_format);
            if (m_spec.nchannels != want) {
                errorfmt(
                    "APV profile \"{}\" requires {} channels, but image has {}",
                    profile, want, m_spec.nchannels);
                return false;
            }
            return true;
        }
    }
    errorfmt("Unknown APV profile \"{}\"", profile);
    return false;
}



bool
ApvOutput::open(const std::string& name, const ImageSpec& userspec,
                OpenMode mode)
{
    if (mode == AppendMIPLevel) {
        errorfmt("APV does not support MIP levels");
        return false;
    }
    if (mode == AppendSubimage) {
        if (!finalize_subimage())
            return false;
        m_spec = userspec;
        if (!setup_profile())
            return false;
        m_staging.assign(size_t(m_spec.width) * m_spec.height
                             * m_spec.nchannels,
                         0.0f);
        m_dirty = false;
        return true;
    }

    m_filename = name;
    m_spec     = userspec;
    if (!check_open(mode, userspec, { 0, 1 << 20, 0, 1 << 20, 0, 1, 0, 4 }))
        return false;
    if (m_spec.depth > 1) {
        errorfmt("APV does not support volume images");
        return false;
    }
    if (!setup_profile())
        return false;
    if (!ioproxy_use_or_open(name))
        return false;
    m_staging.assign(size_t(m_spec.width) * m_spec.height * m_spec.nchannels,
                     0.0f);
    m_dirty = false;
    return true;
}



bool
ApvOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                          stride_t xstride)
{ return write_scanlines(y, y + 1, z, format, data, xstride, AutoStride); }



bool
ApvOutput::write_scanlines(int ybegin, int yend, int z, TypeDesc format,
                           const void* data, stride_t xstride, stride_t ystride)
{
    if (z != 0 || ybegin < 0 || yend > m_spec.height) {
        errorfmt("Scanline range [{}, {}) out of bounds", ybegin, yend);
        return false;
    }
    stride_t pixelsize = stride_t(m_spec.nchannels * format.size());
    if (xstride == AutoStride)
        xstride = pixelsize;
    if (ystride == AutoStride)
        ystride = xstride * m_spec.width;
    for (int y = ybegin; y < yend; y++) {
        const char* src = (const char*)data + (y - ybegin) * ystride;
        float* dst      = m_staging.data()
                          + size_t(y) * m_spec.width * m_spec.nchannels;
        OIIO::convert_image(m_spec.nchannels, m_spec.width, 1, 1, src, format,
                            xstride, AutoStride, AutoStride, dst,
                            TypeDesc::FLOAT, AutoStride, AutoStride,
                            AutoStride);
    }
    m_dirty = true;
    return true;
}



bool
ApvOutput::finalize_subimage()
{
    if (!m_dirty)
        return true;

    const int w        = m_spec.width;
    const int h        = m_spec.height;
    const int channels = m_spec.nchannels;
    const int bits     = m_bits;
    const int cs       = OAPV_CS_SET(m_color_format, bits, 0);

    // Color handling: we always encode with BT.709 matrix coefficients.
    // If the spec carries a CICP attribute, pass its primaries/transfer
    // through to the bitstream's color description and honor its range
    // flag; otherwise emit no color description and use limited range.
    float kr = 0.2126f, kb = 0.0722f;
    int color_present = 0, color_primaries = 2, color_transfer = 2;
    int full_range = 0;
    if (const ParamValue* p = m_spec.find_attribute("CICP")) {
        if (p->type() == TypeDesc(TypeDesc::INT, 4)) {
            const int* cicp = (const int*)p->data();
            color_present   = 1;
            color_primaries = cicp[0];
            color_transfer  = cicp[1];
            full_range      = cicp[3] ? 1 : 0;
        }
    }
    const float kg     = 1.0f - kr - kb;
    const float maxval = float((1 << bits) - 1);
    const float lo     = full_range ? 0.0f : float(16 << (bits - 8));
    const float yrange = full_range ? maxval : float(219 << (bits - 8));
    const float crange = full_range ? maxval : float(224 << (bits - 8));
    const float chalf  = float(1 << (bits - 1));

    // Allocate and fill the input frame.
    oapv_imgb_t* imgb = imgb_create(w, h, cs);
    if (!imgb) {
        errorfmt("Could not allocate APV frame buffer");
        return false;
    }
    auto plane = [&](int p) { return reinterpret_cast<uint16_t*>(imgb->a[p]); };
    auto pstride = [&](int p) { return imgb->s[p] / 2; };
    auto quant   = [&](float v, float scale, float offset) -> uint16_t {
        float q = offset + v * scale;
        return uint16_t(clamp(q, 0.0f, maxval) + 0.5f);
    };

    const bool subx = (m_color_format == OAPV_CF_YCBCR422);
    for (int y = 0; y < h; y++) {
        const float* src = m_staging.data() + size_t(y) * w * channels;
        uint16_t* yrow   = plane(0) + size_t(y) * pstride(0);
        if (channels == 1) {
            for (int x = 0; x < w; x++)
                yrow[x] = quant(clamp(src[x], 0.0f, 1.0f), yrange, lo);
            continue;
        }
        uint16_t* cbrow = plane(1) + size_t(y) * pstride(1);
        uint16_t* crrow = plane(2) + size_t(y) * pstride(2);
        uint16_t* arow  = channels == 4 ? plane(3) + size_t(y) * pstride(3)
                                        : nullptr;
        const int cw    = imgb->w[1];
        for (int cx = 0; cx < cw; cx++) {
            // For 4:2:2, box-filter each horizontal pair of pixels.
            int x0   = subx ? std::min(2 * cx, w - 1) : cx;
            int x1   = subx ? std::min(2 * cx + 1, w - 1) : cx;
            float r  = 0.5f * (src[x0 * channels + 0] + src[x1 * channels + 0]);
            float g  = 0.5f * (src[x0 * channels + 1] + src[x1 * channels + 1]);
            float b  = 0.5f * (src[x0 * channels + 2] + src[x1 * channels + 2]);
            float Y  = kr * r + kg * g + kb * b;
            float pb = (b - Y) / (2.0f * (1.0f - kb));
            float pr = (r - Y) / (2.0f * (1.0f - kr));
            cbrow[cx] = quant(pb, crange, chalf);
            crrow[cx] = quant(pr, crange, chalf);
        }
        for (int x = 0; x < w; x++) {
            float r = src[x * channels + 0];
            float g = src[x * channels + 1];
            float b = src[x * channels + 2];
            float Y = kr * r + kg * g + kb * b;
            yrow[x] = quant(clamp(Y, 0.0f, 1.0f), yrange, lo);
            if (arow)
                arow[x] = uint16_t(
                    clamp(src[x * channels + 3], 0.0f, 1.0f) * maxval + 0.5f);
        }
    }

    // Set up the encoder.
    oapve_cdesc_t cdesc;
    memset(&cdesc, 0, sizeof(cdesc));
    cdesc.max_bs_buf_size = w * h * channels * 4 + (1 << 20);
    cdesc.max_num_frms    = 1;
    cdesc.threads         = OAPV_CDESC_THREADS_AUTO;
    oapve_param_t& param  = cdesc.param[0];
    oapve_param_default(&param);
    param.profile_idc = m_profile_idc;
    param.w           = w;
    param.h           = h;
    int fps[2]        = { 30, 1 };
    if (const ParamValue* p = m_spec.find_attribute("FramesPerSecond",
                                                    TypeRational)) {
        const int* r = (const int*)p->data();
        if (r[0] > 0 && r[1] > 0) {
            fps[0] = r[0];
            fps[1] = r[1];
        }
    }
    param.fps_num = fps[0];
    param.fps_den = fps[1];
    int qp        = m_spec.get_int_attribute("apv:qp", -1);
    if (qp >= 0)
        param.qp = (unsigned char)qp;
    int bitrate = m_spec.get_int_attribute("apv:bitrate", 0);
    if (bitrate > 0) {
        param.bitrate = bitrate;
        param.rc_type = OAPV_RC_ABR;
    }
    string_view preset = m_spec.get_string_attribute("apv:preset", "");
    if (!preset.empty()) {
        for (const oapv_dict_str_int_t* d = oapv_param_opts_preset; d->key[0];
             d++) {
            if (preset == d->key) {
                param.preset = d->val;
                break;
            }
        }
    }
    param.color_description_present_flag = color_present;
    if (color_present) {
        param.color_primaries          = (unsigned char)color_primaries;
        param.transfer_characteristics = (unsigned char)color_transfer;
        param.matrix_coefficients      = 1;  // BT.709, what we encoded with
        param.full_range_flag          = full_range;
    }

    int err         = OAPV_OK;
    oapve_t encoder = oapve_create(&cdesc, &err);
    if (encoder == nullptr) {
        imgb->release(imgb);
        errorfmt("Could not create APV encoder (error {})", err);
        return false;
    }

    std::vector<unsigned char> bsbuf(cdesc.max_bs_buf_size);
    oapv_bitb_t bitb;
    memset(&bitb, 0, sizeof(bitb));
    bitb.addr  = bsbuf.data();
    bitb.bsize = (int)bsbuf.size();

    oapv_frms_t ifrms, rfrms;
    memset(&ifrms, 0, sizeof(ifrms));
    memset(&rfrms, 0, sizeof(rfrms));
    ifrms.num_frms        = 1;
    ifrms.frm[0].imgb     = imgb;
    ifrms.frm[0].pbu_type = OAPV_PBU_TYPE_PRIMARY_FRAME;
    ifrms.frm[0].group_id = 1;
    oapve_stat_t stat;
    memset(&stat, 0, sizeof(stat));

    int ret = oapve_encode(encoder, &ifrms, nullptr, &bitb, &stat, &rfrms);
    oapve_delete(encoder);
    imgb->release(imgb);
    if (ret != OAPV_OK || stat.write <= 0) {
        errorfmt("APV encode failed (error {})", ret);
        return false;
    }

    // The encoder's output already carries the raw bitstream framing
    // ([4-byte big-endian AU size][AU]), so write it verbatim.
    if (!iowrite(bsbuf.data(), stat.write))
        return false;
    m_dirty = false;
    return true;
}



bool
ApvOutput::close()
{
    if (!ioproxy_opened())  // already closed
        return true;
    bool ok = finalize_subimage();
    init();
    return ok;
}

OIIO_PLUGIN_NAMESPACE_END
