// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cerrno>

extern "C" {  // ffmpeg is a C api
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

// It's hard to figure out FFMPEG versions from what they give us, so
// record some of the milestones once and for all for easy reference.
#define USE_FFMPEG_2_6 (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(56, 26, 100))
#define USE_FFMPEG_2_7 (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(56, 41, 100))
#define USE_FFMPEG_2_8 (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(56, 60, 100))
#define USE_FFMPEG_3_0 (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 24, 100))
#define USE_FFMPEG_3_1 (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 100))
#define USE_FFMPEG_3_2 (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 64, 100))
#define USE_FFMPEG_3_3 (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 89, 100))
#define USE_FFMPEG_3_4 (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 107, 100))
#define USE_FFMPEG_4_0 (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(58, 18, 100))
#define USE_FFMPEG_4_1 (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(58, 35, 100))
#define USE_FFMPEG_4_2 (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(58, 54, 100))
#define USE_FFMPEG_4_3 (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(58, 91, 100))
#define USE_FFMPEG_4_4 (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(58, 134, 100))

#if !USE_FFMPEG_4_0
#    error "OIIO FFmpeg support requires FFmpeg >= 4.0"
#endif

#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}


static constexpr int ffmpeg_image_align = 64;

inline int
avpicture_fill(AVFrame* picture, uint8_t* ptr, enum AVPixelFormat pix_fmt,
               int width, int height)
{
    AVFrame* frame = reinterpret_cast<AVFrame*>(picture);
    return av_image_fill_arrays(frame->data, frame->linesize, ptr, pix_fmt,
                                width, height, ffmpeg_image_align);
}


#define stream_codec(ix) m_format_context->streams[(ix)]->codecpar


// avcodec_decode_video2 was deprecated.
// This now works by sending `avpkt` to the decoder, which buffers the
// decoded image in `avctx`. Then `avcodec_receive_frame` will copy the
// frame to `picture`.
inline int
receive_frame(AVCodecContext* avctx, AVFrame* picture, AVPacket* avpkt)
{
    int ret = avcodec_send_packet(avctx, avpkt);

    // AVERROR_EOF means the decoder is already draining: it accepts no more
    // input, but it may still have buffered frames to hand back, so keep
    // going and let avcodec_receive_frame() tell us when it is empty.
    if (ret < 0 && ret != AVERROR_EOF)
        return 0;

    return avcodec_receive_frame(avctx, picture) == 0;
}



#include <OpenImageIO/color.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <climits>
#include <cmath>
#include <iostream>
#include <mutex>

OIIO_PLUGIN_NAMESPACE_BEGIN


// Timestamps, frame rates and time bases all come from the file, so the
// arithmetic below can produce NaN, infinity, or values outside the integer
// range. Convert through these helpers to keep that out of undefined
// behavior. safe_int64's range is well under the type's limits so that a sum
// of two results can't overflow either.
static int64_t
safe_int64(double v)
{
    return std::isfinite(v) ? int64_t(clamp(v, -4.0e18, 4.0e18)) : 0;
}


static int
safe_int(double v)
{
    return std::isfinite(v) ? int(clamp(v, double(INT_MIN), double(INT_MAX)))
                            : 0;
}


class FFmpegInput final : public ImageInput {
public:
    FFmpegInput();
    ~FFmpegInput() override;
    const char* format_name(void) const override { return "FFmpeg movie"; }
    int supports(string_view feature) const override
    {
        return (feature == "multiimage");
    }
    bool valid_file(const std::string& name) const override;
    bool open(const std::string& name, ImageSpec& spec) override;
    bool close(void) override;
    int current_subimage(void) const override
    {
        lock_guard lock(*this);
        return m_subimage;
    }
    bool seek_subimage(int subimage, int miplevel) override;
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;
    void read_frame(int pos);
#if 0
    const char *metadata (const char * key);
    bool has_metadata (const char * key);
#endif
    bool seek(int pos);
    double fps() const;
    int64_t time_stamp(int pos) const;

private:
    std::string m_filename;
    int m_subimage;
    int64_t m_nsubimages;
    AVFormatContext* m_format_context = nullptr;
    AVCodecContext* m_codec_context   = nullptr;
    const AVCodec* m_codec            = nullptr;
    AVFrame* m_frame                  = nullptr;
    AVFrame* m_rgb_frame              = nullptr;
    size_t m_stride;  // scanline width in bytes, a.k.a. scanline stride
    AVPixelFormat m_decoded_pix_format;  // what the decoder promised at open
    AVPixelFormat m_dst_pix_format;
    SwsContext* m_sws_rgb_context = nullptr;
    AVRational m_frame_rate;
    std::vector<uint8_t> m_rgb_buffer;
    std::vector<int> m_video_indexes;
    int m_video_stream;
    int m_data_stream;
    int64_t m_frames;
    int m_last_decoded_pos;
    bool m_codec_cap_delay;
    bool m_read_frame;
    bool m_frame_valid;  // did the last read_frame() actually decode?
    int64_t m_start_time;

    // init to initialize state
    void init(void)
    {
        m_filename.clear();
        m_format_context     = nullptr;
        m_codec_context      = nullptr;
        m_codec              = nullptr;
        m_frame              = nullptr;
        m_rgb_frame          = nullptr;
        m_sws_rgb_context    = nullptr;
        m_stride             = 0;
        m_decoded_pix_format = AV_PIX_FMT_NONE;
        m_rgb_buffer.clear();
        m_video_indexes.clear();
        m_video_stream = -1;
        m_data_stream  = -1;
        m_frames       = 0;
        // -1, not 0, so that the first read_frame() always seeks: nothing
        // has been decoded yet, and open() leaves the demuxer wherever the
        // frame-count pass stopped.
        m_last_decoded_pos = -1;
        m_read_frame       = false;
        m_frame_valid      = false;
        m_codec_cap_delay  = false;
        m_subimage         = 0;
        m_start_time       = 0;
    }
};



// Obligatory material to make this a recognizable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT int ffmpeg_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
ffmpeg_imageio_library_version()
{
    return "FFMpeg " OIIO_FFMPEG_VERSION " (" LIBAVFORMAT_IDENT ")";
}

OIIO_EXPORT ImageInput*
ffmpeg_input_imageio_create()
{
    return new FFmpegInput;
}
// FFmpeg hints:
// AVI (Audio Video Interleaved)
// QuickTime / MOV
// raw MPEG-4 video
// MPEG-1 Systems / MPEG program stream
OIIO_EXPORT const char* ffmpeg_input_extensions[]
    = { "avi", "mov", "qt",  "mp4", "m4a", "3gp",
        "3g2", "mj2", "m4v", "mpg", "mkv", nullptr };


OIIO_PLUGIN_EXPORTS_END



FFmpegInput::FFmpegInput() { init(); }



FFmpegInput::~FFmpegInput() { close(); }



bool
FFmpegInput::valid_file(const std::string& name) const
{
    // Quick/naive test -- just make sure the extension is valid for one of
    // the supported file types supported by this reader.
    for (int i = 0; ffmpeg_input_extensions[i]; ++i)
        if (Strutil::iends_with(name, ffmpeg_input_extensions[i]))
            return true;
    return false;
}



bool
FFmpegInput::open(const std::string& name, ImageSpec& spec)
{
    // Temporary workaround: refuse to open a file whose name does not
    // indicate that it's a movie file. This avoids the problem that ffmpeg
    // is willing to open tiff and other files better handled by other
    // plugins. The better long-term solution is to replace av_register_all
    // with our own function that registers only the formats that we want
    // this reader to handle. At some point, we will institute that superior
    // approach, but in the mean time, this is a quick solution that 90%
    // does the job.
    bool valid_extension = false;
    for (int i = 0; ffmpeg_input_extensions[i]; ++i)
        if (Strutil::iends_with(name, ffmpeg_input_extensions[i])) {
            valid_extension = true;
            break;
        }
    if (!valid_extension) {
        errorfmt("\"{}\" could not open input", name);
        return false;
    }

    const char* file_name = name.c_str();
    av_log_set_level(AV_LOG_FATAL);
    if (avformat_open_input(&m_format_context, file_name, NULL, NULL) != 0) {
        // avformat_open_input allocs format_context
        errorfmt("\"{}\" could not open input", file_name);
        return false;
    }
    if (avformat_find_stream_info(m_format_context, NULL) < 0) {
        errorfmt("\"{}\" could not find stream info", file_name);
        return false;
    }
    m_video_stream = -1;
    for (unsigned int i = 0; i < m_format_context->nb_streams; i++) {
        if (stream_codec(i)->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (m_video_stream < 0) {
                m_video_stream = i;
            }
            m_video_indexes.push_back(i);  // needed for later use
            break;
        }
    }
    if (m_video_stream == -1) {
        errorfmt("\"{}\" could not find a valid videostream", file_name);
        return false;
    }
    for (unsigned int i = 0; i < m_format_context->nb_streams; i++) {
        if (stream_codec(i)->codec_type == AVMEDIA_TYPE_DATA) {
            if (m_data_stream < 0) {
                m_data_stream = i;
                break;
            }
        }
    }

    // codec context for videostream
    AVCodecParameters* par = stream_codec(m_video_stream);

    m_codec = avcodec_find_decoder(par->codec_id);
    if (!m_codec) {
        errorfmt("\"{}\" can't find decoder", file_name);
        return false;
    }

    m_codec_context = avcodec_alloc_context3(m_codec);
    if (!m_codec_context) {
        errorfmt("\"{}\" can't allocate decoder context", file_name);
        return false;
    }

    int ret;

    ret = avcodec_parameters_to_context(m_codec_context, par);
    if (ret < 0) {
        errorfmt("\"{}\" unsupported codec", file_name);
        return false;
    }

    if (avcodec_open2(m_codec_context, m_codec, NULL) < 0) {
        errorfmt("\"{}\" could not open codec", file_name);
        return false;
    }
    m_codec_cap_delay = (bool)(m_codec_context->codec->capabilities
                               & AV_CODEC_CAP_DELAY);

    AVStream* stream = m_format_context->streams[m_video_stream];
    m_frame_rate     = av_guess_frame_rate(m_format_context, stream, NULL);

    // nb_frames and start_time come from the file. Clamp them to a range the
    // rest of this reader can handle: subimage indices are int, and an
    // unset start_time (AV_NOPTS_VALUE == INT64_MIN) would blow up the frame
    // arithmetic in read_frame().
    m_frames     = OIIO::clamp(stream->nb_frames, int64_t(0), int64_t(INT_MAX));
    m_start_time = stream->start_time;
    if (m_start_time == int64_t(AV_NOPTS_VALUE))
        m_start_time = 0;
    if (!m_frames) {
        // The container did not say how many frames there are, so find the
        // timestamp of the first video packet and of the last one, and turn
        // the span into a frame count. Only video packets count: audio and
        // data streams have their own time base and often run past the end
        // of the video, so counting them invents subimages that no frame
        // will ever decode into.
        AVPacket* pkt = av_packet_alloc();
        if (!pkt) {
            errorfmt("\"{}\" could not allocate FFmpeg packet", file_name);
            close();
            return false;
        }
        seek(0);
        int64_t first_pts = 0;
        while (av_read_frame(m_format_context, pkt) >= 0) {
            bool is_video = (pkt->stream_index == m_video_stream);
            int64_t pts   = pkt->pts;
            av_packet_unref(pkt);  // seek() below reuses m_format_context
            if (is_video) {
                if (pts != int64_t(AV_NOPTS_VALUE))
                    first_pts = pts;
                break;
            }
        }
        int64_t max_pts = 0;
        seek(1 << 29);
        while (av_read_frame(m_format_context, pkt) >= 0) {
            bool is_video = (pkt->stream_index == m_video_stream);
            int64_t pts   = pkt->pts;
            av_packet_unref(pkt);  // always free before reusing the context
            if (!is_video || pts == int64_t(AV_NOPTS_VALUE))
                continue;
            // Do the difference in double: both timestamps are untrusted and
            // subtracting them as int64 can overflow.
            int64_t current_pts = safe_int64(av_q2d(stream->time_base)
                                             * (double(pts) - double(first_pts))
                                             * fps());
            if (current_pts > max_pts) {
                max_pts = current_pts + 1;
            }
        }
        av_packet_free(&pkt);
        seek(0);  // the pass above ran to the end of the file
        m_frames = std::min(max_pts, int64_t(INT_MAX));
    }
    m_frame     = av_frame_alloc();
    m_rgb_frame = av_frame_alloc();
    if (!m_frame || !m_rgb_frame) {
        errorfmt("\"{}\" could not allocate FFmpeg frame", file_name);
        close();
        return false;
    }

    m_decoded_pix_format = m_codec_context->pix_fmt;
    AVPixelFormat src_pix_format;
    switch (m_codec_context->pix_fmt) {  // deprecation warning for YUV formats
    case AV_PIX_FMT_YUVJ420P: src_pix_format = AV_PIX_FMT_YUV420P; break;
    case AV_PIX_FMT_YUVJ422P: src_pix_format = AV_PIX_FMT_YUV422P; break;
    case AV_PIX_FMT_YUVJ444P: src_pix_format = AV_PIX_FMT_YUV444P; break;
    case AV_PIX_FMT_YUVJ440P: src_pix_format = AV_PIX_FMT_YUV440P; break;
    default: src_pix_format = m_codec_context->pix_fmt; break;
    }

    // Assume by default that we're delivering RGB UINT8
    int nchannels     = 3;
    TypeDesc datatype = TypeUInt8;
    m_dst_pix_format  = AV_PIX_FMT_RGB24;
    // Look for formats that indicate we should save some different number
    // of channels or bit depth.
    switch (src_pix_format) {
    // support for 10-bit and 12-bit pix_fmts
    case AV_PIX_FMT_RGB48BE:
    case AV_PIX_FMT_RGB48LE:
    case AV_PIX_FMT_BGR48BE:
    case AV_PIX_FMT_BGR48LE:
    case AV_PIX_FMT_YUV420P9BE:
    case AV_PIX_FMT_YUV420P9LE:
    case AV_PIX_FMT_YUV422P9BE:
    case AV_PIX_FMT_YUV422P9LE:
    case AV_PIX_FMT_YUV444P9BE:
    case AV_PIX_FMT_YUV444P9LE:
    case AV_PIX_FMT_YUV420P10BE:
    case AV_PIX_FMT_YUV420P10LE:
    case AV_PIX_FMT_YUV422P10BE:
    case AV_PIX_FMT_YUV422P10LE:
    case AV_PIX_FMT_YUV444P10BE:
    case AV_PIX_FMT_YUV444P10LE:
    case AV_PIX_FMT_YUV420P12BE:
    case AV_PIX_FMT_YUV420P12LE:
    case AV_PIX_FMT_YUV422P12BE:
    case AV_PIX_FMT_YUV422P12LE:
    case AV_PIX_FMT_YUV444P12BE:
    case AV_PIX_FMT_YUV444P12LE:
    case AV_PIX_FMT_YUV420P14BE:
    case AV_PIX_FMT_YUV420P14LE:
    case AV_PIX_FMT_YUV422P14BE:
    case AV_PIX_FMT_YUV422P14LE:
    case AV_PIX_FMT_YUV444P14BE:
    case AV_PIX_FMT_YUV444P14LE:
    case AV_PIX_FMT_GBRP9BE:
    case AV_PIX_FMT_GBRP9LE:
    case AV_PIX_FMT_GBRP10BE:
    case AV_PIX_FMT_GBRP10LE:
    case AV_PIX_FMT_GBRP16BE:
    case AV_PIX_FMT_GBRP16LE:
    case AV_PIX_FMT_GBRP12BE:
    case AV_PIX_FMT_GBRP12LE:
    case AV_PIX_FMT_GBRP14BE:
    case AV_PIX_FMT_GBRP14LE:
    case AV_PIX_FMT_BAYER_BGGR16LE:
    case AV_PIX_FMT_BAYER_BGGR16BE:
    case AV_PIX_FMT_BAYER_RGGB16LE:
    case AV_PIX_FMT_BAYER_RGGB16BE:
    case AV_PIX_FMT_BAYER_GBRG16LE:
    case AV_PIX_FMT_BAYER_GBRG16BE:
    case AV_PIX_FMT_BAYER_GRBG16LE:
    case AV_PIX_FMT_BAYER_GRBG16BE:
    case AV_PIX_FMT_GBRAP10BE:
    case AV_PIX_FMT_GBRAP10LE:
    case AV_PIX_FMT_GBRAP12BE:
    case AV_PIX_FMT_GBRAP12LE:
    case AV_PIX_FMT_P016LE:
    case AV_PIX_FMT_P016BE:
        datatype = TypeUInt16;
        // Must use planar format because swscale does not handle
        // interleaved correctly for 16 bit.
        m_dst_pix_format = AV_PIX_FMT_GBRP16;
        break;
    // Grayscale 8 bit
    case AV_PIX_FMT_GRAY8:
    case AV_PIX_FMT_MONOWHITE:
    case AV_PIX_FMT_MONOBLACK:
        nchannels        = 1;
        datatype         = TypeUInt8;
        m_dst_pix_format = AV_PIX_FMT_GRAY8;
        break;
    // Grayscale 16 bit
    case AV_PIX_FMT_GRAY9BE:
    case AV_PIX_FMT_GRAY9LE:
    case AV_PIX_FMT_GRAY10BE:
    case AV_PIX_FMT_GRAY10LE:
    case AV_PIX_FMT_GRAY12BE:
    case AV_PIX_FMT_GRAY12LE:
    case AV_PIX_FMT_GRAY16BE:
    case AV_PIX_FMT_GRAY16LE:
        nchannels        = 1;
        datatype         = TypeUInt16;
        m_dst_pix_format = AV_PIX_FMT_GRAY16;
        break;
    // RGBA 8 bit
    case AV_PIX_FMT_YA8:  // YA, but promote to RGBA because who cares
    case AV_PIX_FMT_YUVA422P:
    case AV_PIX_FMT_YUVA444P:
    case AV_PIX_FMT_GBRAP:
        nchannels        = 4;
        datatype         = TypeUInt8;
        m_dst_pix_format = AV_PIX_FMT_RGBA;
        break;
    // RGBA 16 bit
    case AV_PIX_FMT_YA16:  // YA, but promote to RGBA
    case AV_PIX_FMT_YUVA420P9BE:
    case AV_PIX_FMT_YUVA420P9LE:
    case AV_PIX_FMT_YUVA422P9BE:
    case AV_PIX_FMT_YUVA422P9LE:
    case AV_PIX_FMT_YUVA444P9BE:
    case AV_PIX_FMT_YUVA444P9LE:
    case AV_PIX_FMT_YUVA420P10BE:
    case AV_PIX_FMT_YUVA420P10LE:
    case AV_PIX_FMT_YUVA422P10BE:
    case AV_PIX_FMT_YUVA422P10LE:
    case AV_PIX_FMT_YUVA444P10BE:
    case AV_PIX_FMT_YUVA444P10LE:
#if USE_FFMPEG_4_2
    case AV_PIX_FMT_YUVA422P12BE:
    case AV_PIX_FMT_YUVA422P12LE:
    case AV_PIX_FMT_YUVA444P12BE:
    case AV_PIX_FMT_YUVA444P12LE:
#endif
    case AV_PIX_FMT_YUVA420P16BE:
    case AV_PIX_FMT_YUVA420P16LE:
    case AV_PIX_FMT_YUVA422P16BE:
    case AV_PIX_FMT_YUVA422P16LE:
    case AV_PIX_FMT_YUVA444P16BE:
    case AV_PIX_FMT_YUVA444P16LE:
    case AV_PIX_FMT_GBRAP16:
        nchannels = 4;
        datatype  = TypeUInt16;
        // Must use planar format because swscale does not handle
        // interleaved correctly for 16 bit.
        m_dst_pix_format = AV_PIX_FMT_GBRAP16;
        break;
    // RGB float
    case AV_PIX_FMT_GBRPF32BE:
    case AV_PIX_FMT_GBRPF32LE:
        nchannels = 3;
        datatype  = TypeFloat;
        // Must use planar format as there is no interleaved 32 bit float.
        m_dst_pix_format = AV_PIX_FMT_GBRPF32;
        break;
    // RGBA float
    case AV_PIX_FMT_GBRAPF32BE:
    case AV_PIX_FMT_GBRAPF32LE:
        nchannels = 4;
        datatype  = TypeFloat;
        // Must use planar format as there is no interleaved 32 bit float.
        m_dst_pix_format = AV_PIX_FMT_GBRAPF32;
        break;

    // Everything else is regular 8 bit RGB
    default: break;
    }

    m_spec = ImageSpec(m_codec_context->width, m_codec_context->height,
                       nchannels, datatype);
    // The frame dimensions come from the container/codec headers, which are
    // untrusted. FFmpeg caps the pixel count at only about 2.7e8, which still
    // leaves room for a tiny file to declare a multi-GB frame.
    if (!check_open(m_spec, { 0, 1 << 16, 0, 1 << 16, 0, 1, 0, 4 })
        || !check_compression_ratio(m_spec, Filesystem::file_size(name))) {
        close();
        return false;
    }
    m_stride = (size_t)(m_spec.scanline_bytes());

    int rgb_buffer_size
        = av_image_get_buffer_size(m_dst_pix_format, m_codec_context->width,
                                   m_codec_context->height, ffmpeg_image_align);
    if (rgb_buffer_size <= 0) {
        errorfmt("\"{}\" invalid FFmpeg RGB buffer size", file_name);
        return false;
    }
    m_rgb_buffer.resize(static_cast<size_t>(rgb_buffer_size), 0);

    m_sws_rgb_context
        = sws_getContext(m_codec_context->width, m_codec_context->height,
                         src_pix_format, m_codec_context->width,
                         m_codec_context->height, m_dst_pix_format, SWS_AREA,
                         NULL, NULL, NULL);
    if (!m_sws_rgb_context) {
        errorfmt("\"{}\" could not create FFmpeg scaling context", file_name);
        return false;
    }

    AVDictionaryEntry* tag = NULL;
    while ((tag = av_dict_get(m_format_context->metadata, "", tag,
                              AV_DICT_IGNORE_SUFFIX))) {
        m_spec.attribute(tag->key, tag->value);
    }
    tag = NULL;
    if (m_data_stream >= 0) {
        while ((
            tag = av_dict_get(m_format_context->streams[m_data_stream]->metadata,
                              "", tag, AV_DICT_IGNORE_SUFFIX))) {
            if (strcmp(tag->key, "timecode") == 0) {
                m_spec.attribute("ffmpeg:TimeCode", tag->value);
                break;
            }
        }
    }
    tag = NULL;
    while (
        (tag = av_dict_get(m_format_context->streams[m_video_stream]->metadata,
                           "", tag, AV_DICT_IGNORE_SUFFIX))) {
        if (strcmp(tag->key, "timecode") == 0) {
            m_spec.attribute("ffmpeg:TimeCode", tag->value);
            break;
        }
    }
    int rat[2] = { m_frame_rate.num, m_frame_rate.den };
    m_spec.attribute("FramesPerSecond", TypeRational, &rat);
    m_spec.attribute("oiio:Movie", true);
    m_spec.attribute("oiio:subimages", int(m_frames));
    if (m_codec_context->bits_per_raw_sample) {
        m_spec.attribute("oiio:BitsPerSample",
                         m_codec_context->bits_per_raw_sample);
    } else {
        // If bits_per_raw_sample is not provided, the bit depth of the
        // luma channel is the closest equivalent to a single bit depth.
        const AVPixFmtDescriptor* pix_format_desc = av_pix_fmt_desc_get(
            src_pix_format);
        if (pix_format_desc && pix_format_desc->nb_components > 0) {
            m_spec.attribute("oiio:BitsPerSample",
                             pix_format_desc->comp[0].depth);
        }
    }
    m_spec.attribute("ffmpeg:codec_name", m_codec_context->codec->long_name);
    /* The ffmpeg enums are documented to match CICP values, except the color range. */
    const int cicp[4]
        = { m_codec_context->color_primaries, m_codec_context->color_trc,
            m_codec_context->colorspace,
            m_codec_context->color_range == AVCOL_RANGE_MPEG ? 0 : 1 };
    m_spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), cicp);
    const ColorConfig& colorconfig(ColorConfig::default_colorconfig());
    string_view interop_id = colorconfig.get_color_interop_id(cicp);
    if (!interop_id.empty())
        m_spec.attribute("oiio:ColorSpace", interop_id);

    m_nsubimages = m_frames;
    spec         = m_spec;
    m_filename   = name;
    return true;
}



bool
FFmpegInput::seek_subimage(int subimage, int miplevel)
{
    if (subimage < 0 || subimage >= m_nsubimages || miplevel > 0) {
        return false;
    }
    if (subimage == m_subimage) {
        return true;
    }
    m_subimage    = subimage;
    m_read_frame  = false;
    m_frame_valid = false;
    return true;
}

template<typename T, int nchannels>
static bool
read_planar_scanline(void* data, int y, int width, AVFrame* rgb_frame)
{
    static_assert(nchannels == 3 || nchannels == 4);

    const T* in_planes[nchannels];
    const int swizzle[4] = { 1, 2, 0, 3 };  // GBR to RGB
    for (int channel = 0; channel < nchannels; ++channel) {
        if (rgb_frame->data[channel] == nullptr) {
            return false;
        }
        in_planes[swizzle[channel]] = reinterpret_cast<const T*>(
            rgb_frame->data[channel] + y * rgb_frame->linesize[channel]);
    }

    T* out = static_cast<T*>(data);
    for (int x = 0; x < width; ++x) {
        for (int channel = 0; channel < nchannels; ++channel) {
            out[channel] = in_planes[channel][x];
        }
        out += nchannels;
    }
    return true;
}

bool
FFmpegInput::read_native_scanline(int subimage, int miplevel, int y, int /*z*/,
                                  void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;
    if (!m_read_frame) {
        read_frame(m_subimage);
    }
    if (!m_frame_valid) {
        // Nothing was decoded for this subimage. Without this check we would
        // hand back whatever the previously decoded frame left in the buffer.
        if (!has_error())
            errorfmt("Could not decode frame {} of \"{}\"", m_subimage,
                     m_filename);
        return false;
    }
    if (m_spec.format == TypeUInt8 || m_spec.nchannels == 1) {
        if (m_rgb_frame->data[0]) {
            memcpy(data, m_rgb_frame->data[0] + y * m_rgb_frame->linesize[0],
                   m_stride);
            return true;
        }
    } else if (m_spec.format == TypeUInt16) {
        if (m_spec.nchannels == 3) {
            if (read_planar_scanline<uint16_t, 3>(data, y, m_spec.width,
                                                  m_rgb_frame)) {
                return true;
            }
        } else if (m_spec.nchannels == 4) {
            if (read_planar_scanline<uint16_t, 4>(data, y, m_spec.width,
                                                  m_rgb_frame)) {
                return true;
            }
        }
    } else if (m_spec.format == TypeFloat) {
        if (m_spec.nchannels == 3) {
            if (read_planar_scanline<float, 3>(data, y, m_spec.width,
                                               m_rgb_frame)) {
                return true;
            }
        } else if (m_spec.nchannels == 4) {
            if (read_planar_scanline<float, 4>(data, y, m_spec.width,
                                               m_rgb_frame)) {
                return true;
            }
        }
    }
    errorfmt("Error reading frame");
    return false;
}



bool
FFmpegInput::close(void)
{
    if (m_codec_context)
        avcodec_free_context(&m_codec_context);
    if (m_format_context) {
        // Frees the context and everything it owns, and nulls the pointer.
        avformat_close_input(&m_format_context);
    }
    if (m_frame)
        av_frame_free(&m_frame);  // free after close input
    if (m_rgb_frame)
        av_frame_free(&m_rgb_frame);
    if (m_sws_rgb_context)
        sws_freeContext(m_sws_rgb_context);
    init();
    return true;
}



void
FFmpegInput::read_frame(int frame)
{
    if (m_last_decoded_pos + 1 != frame) {
        seek(frame);
    }
    m_read_frame  = true;
    m_frame_valid = false;
    // Allocate the packet rather than using a stack AVPacket: av_read_frame
    // is not guaranteed to initialize it on failure, and the flush path below
    // would then unref an uninitialized packet.
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        errorfmt("Could not allocate FFmpeg packet");
        return;
    }
    bool flushing = false;
    while (true) {
        int ret = av_read_frame(m_format_context, pkt);
        if (ret < 0) {
            if (!m_codec_cap_delay)
                break;
            // The codec buffers delayed frames, so at the end of the stream
            // keep going with flush packets (data == null, size == 0) to
            // collect them. The test below stops us as soon as the decoder
            // runs dry -- otherwise a stream that keeps returning the same
            // error would spin here forever.
            flushing = true;
            av_packet_unref(pkt);
            pkt->stream_index = m_video_stream;
        }
        if (pkt->stream_index == m_video_stream) {
            int finished = receive_frame(m_codec_context, m_frame, pkt);
            if (flushing && !finished)
                break;

            // m_frame->pts and m_start_time are both counts of time base
            // ticks; scale both to seconds before taking the difference.
            double time_base = av_q2d(
                m_format_context->streams[m_video_stream]->time_base);
            double pts = 0;
            if (static_cast<int64_t>(m_frame->pts) != int64_t(AV_NOPTS_VALUE)) {
                pts = time_base * double(m_frame->pts);
            }

            int current_frame = safe_int(
                (pts - time_base * double(m_start_time)) * fps() + 0.5);
            if (current_frame == frame && finished) {
                // A decoder may change frame geometry or pixel format
                // mid-stream, but the scaling context and RGB buffer were
                // sized from the header when we opened the file.
                if (m_frame->width != m_spec.width
                    || m_frame->height != m_spec.height
                    || m_frame->format != m_decoded_pix_format) {
                    errorfmt("\"{}\" frame {} does not match the {}x{} format "
                             "declared by the header",
                             m_filename, frame, m_spec.width, m_spec.height);
                    break;
                }
                // Use the spec dimensions, which are what m_rgb_buffer and
                // the scaling context were sized for -- the decoder can move
                // m_codec_context->width/height out from under us.
                int fill_ret = avpicture_fill(m_rgb_frame, &m_rgb_buffer[0],
                                              m_dst_pix_format, m_spec.width,
                                              m_spec.height);
                if (fill_ret < 0) {
                    errorfmt("Error filling FFmpeg RGB frame");
                    break;
                }
                int scale_ret = sws_scale(m_sws_rgb_context,
                                          static_cast<uint8_t const* const*>(
                                              m_frame->data),
                                          m_frame->linesize, 0, m_spec.height,
                                          m_rgb_frame->data,
                                          m_rgb_frame->linesize);
                if (scale_ret <= 0) {
                    errorfmt("Error converting FFmpeg frame");
                    break;
                }
                m_last_decoded_pos = current_frame;
                m_frame_valid      = true;
                break;
            }
        }
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
}



#if 0
const char *
FFmpegInput::metadata (const char * key)
{
    AVDictionaryEntry * entry = av_dict_get (m_format_context->metadata, key, NULL, 0);
    return entry ? av_strdup(entry->value) : NULL;
    // FIXME -- that looks suspiciously like a memory leak
}



bool
FFmpegInput::has_metadata (const char * key)
{
    return av_dict_get (m_format_context->metadata, key, NULL, 0); // is there a better to check exists?
}
#endif



bool
FFmpegInput::seek(int frame)
{
    int64_t offset = time_stamp(frame);
    int flags      = AVSEEK_FLAG_BACKWARD;
    avcodec_flush_buffers(m_codec_context);
    av_seek_frame(m_format_context, m_video_stream, offset, flags);
    return true;
}



int64_t
FFmpegInput::time_stamp(int frame) const
{
    // The result is in the video stream's time base, which is the unit
    // av_seek_frame() expects when seek() hands it m_video_stream.
    // A corrupt header can give us a zero or degenerate time base, which
    // would make the division below produce inf/NaN.
    double time_base = av_q2d(
        m_format_context->streams[m_video_stream]->time_base);
    double scale = fps() * time_base;
    if (!(scale > 0) || !(time_base > 0))
        return m_start_time;
    return safe_int64(static_cast<double>(frame) / scale) + m_start_time;
}



double
FFmpegInput::fps() const
{
    if (m_frame_rate.den) {
        return av_q2d(m_frame_rate);
    }
    return 1.0f;
}

OIIO_PLUGIN_NAMESPACE_END
