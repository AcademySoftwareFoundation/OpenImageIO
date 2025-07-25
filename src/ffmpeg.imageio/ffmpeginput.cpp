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
}


inline int
avpicture_fill(AVFrame* picture, uint8_t* ptr, enum AVPixelFormat pix_fmt,
               int width, int height)
{
    AVFrame* frame = reinterpret_cast<AVFrame*>(picture);
    return av_image_fill_arrays(frame->data, frame->linesize, ptr, pix_fmt,
                                width, height, 1);
}


#define stream_codec(ix) m_format_context->streams[(ix)]->codecpar


// avcodec_decode_video2 was deprecated.
// This now works by sending `avpkt` to the decoder, which buffers the
// decoded image in `avctx`. Then `avcodec_receive_frame` will copy the
// frame to `picture`.
inline int
receive_frame(AVCodecContext* avctx, AVFrame* picture, AVPacket* avpkt)
{
    int ret;

    ret = avcodec_send_packet(avctx, avpkt);

    if (ret < 0)
        return 0;

    ret = avcodec_receive_frame(avctx, picture);

    if (ret < 0)
        return 0;

    return 1;
}



#include <OpenImageIO/imageio.h>
#include <iostream>
#include <mutex>

OIIO_PLUGIN_NAMESPACE_BEGIN


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
    AVPixelFormat m_dst_pix_format;
    SwsContext* m_sws_rgb_context = nullptr;
    AVRational m_frame_rate;
    std::vector<uint8_t> m_rgb_buffer;
    std::vector<int> m_video_indexes;
    int m_video_stream;
    int m_data_stream;
    int64_t m_frames;
    int m_last_search_pos;
    int m_last_decoded_pos;
    bool m_offset_time;
    bool m_codec_cap_delay;
    bool m_read_frame;
    int64_t m_start_time;

    // init to initialize state
    void init(void)
    {
        m_filename.clear();
        m_format_context  = nullptr;
        m_codec_context   = nullptr;
        m_codec           = nullptr;
        m_frame           = nullptr;
        m_rgb_frame       = nullptr;
        m_sws_rgb_context = nullptr;
        m_stride          = 0;
        m_rgb_buffer.clear();
        m_video_indexes.clear();
        m_video_stream     = -1;
        m_data_stream      = -1;
        m_frames           = 0;
        m_last_search_pos  = 0;
        m_last_decoded_pos = 0;
        m_offset_time      = true;
        m_read_frame       = false;
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
OIIO_EXPORT const char* ffmpeg_input_extensions[] = {
    "avi", "mov", "qt", "mp4", "m4a", "3gp", "3g2", "mj2", "m4v", "mpg", nullptr
};


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
    if (!strcmp(m_codec_context->codec->name, "mjpeg")
        || !strcmp(m_codec_context->codec->name, "dvvideo")) {
        m_offset_time = false;
    }
    m_codec_cap_delay = (bool)(m_codec_context->codec->capabilities
                               & AV_CODEC_CAP_DELAY);

    AVStream* stream = m_format_context->streams[m_video_stream];
    m_frame_rate     = av_guess_frame_rate(m_format_context, stream, NULL);

    m_frames     = stream->nb_frames;
    m_start_time = stream->start_time;
    if (!m_frames) {
        seek(0);
        AVPacket pkt;
        av_init_packet(&pkt);
        av_read_frame(m_format_context, &pkt);
        int64_t first_pts = pkt.pts;
        int64_t max_pts   = 0;
        av_packet_unref(&pkt);  //because seek(int) uses m_format_context
        seek(1 << 29);
        av_init_packet(&pkt);  //Is this needed?
        while (stream && av_read_frame(m_format_context, &pkt) >= 0) {
            int64_t current_pts = static_cast<int64_t>(
                av_q2d(stream->time_base) * (pkt.pts - first_pts) * fps());
            if (current_pts > max_pts) {
                max_pts = current_pts + 1;
            }
            av_packet_unref(&pkt);  //Always free before format_context usage
        }
        m_frames = max_pts;
    }
    m_frame     = av_frame_alloc();
    m_rgb_frame = av_frame_alloc();

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
        datatype         = TypeUInt16;
        m_dst_pix_format = AV_PIX_FMT_RGB48;
        break;
    // Grayscale 8 bit
    case AV_PIX_FMT_GRAY8:
    case AV_PIX_FMT_MONOWHITE:
    case AV_PIX_FMT_MONOBLACK:
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
        nchannels        = 4;
        datatype         = TypeUInt16;
        m_dst_pix_format = AV_PIX_FMT_RGBA64;
        break;
    // RGB float
    case AV_PIX_FMT_GBRPF32BE:
    case AV_PIX_FMT_GBRPF32LE:
        nchannels        = 3;
        datatype         = TypeFloat;
        m_dst_pix_format = AV_PIX_FMT_RGB48;  // ? AV_PIX_FMT_GBRPF32
        // FIXME: They don't have a type for RGB float, only GBR float.
        // Yuck. Punt for now and save as uint16 RGB. If people care, we
        // can return and ask for GBR float and swap order.
        break;
    // RGBA float
    case AV_PIX_FMT_GBRAPF32BE:
    case AV_PIX_FMT_GBRAPF32LE:
        nchannels        = 4;
        datatype         = TypeFloat;
        m_dst_pix_format = AV_PIX_FMT_RGBA64;  // ? AV_PIX_FMT_GBRAPF32
        // FIXME: They don't have a type for RGBA float, only GBRA float.
        // Yuck. Punt for now and save as uint16 RGB. If people care, we
        // can return and ask for GBRA float and swap order.
        break;

    // Everything else is regular 8 bit RGB
    default: break;
    }

    m_spec   = ImageSpec(m_codec_context->width, m_codec_context->height,
                         nchannels, datatype);
    m_stride = (size_t)(m_spec.scanline_bytes());

    m_rgb_buffer.resize(av_image_get_buffer_size(m_dst_pix_format,
                                                 m_codec_context->width,
                                                 m_codec_context->height, 1),
                        0);

    m_sws_rgb_context
        = sws_getContext(m_codec_context->width, m_codec_context->height,
                         src_pix_format, m_codec_context->width,
                         m_codec_context->height, m_dst_pix_format, SWS_AREA,
                         NULL, NULL, NULL);

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
    m_spec.attribute("oiio:BitsPerSample",
                     m_codec_context->bits_per_raw_sample);
    m_spec.attribute("ffmpeg:codec_name", m_codec_context->codec->long_name);
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
    m_subimage   = subimage;
    m_read_frame = false;
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
    if (m_rgb_frame->data[0]) {
        memcpy(data, m_rgb_frame->data[0] + y * m_rgb_frame->linesize[0],
               m_stride);
        return true;
    } else {
        errorfmt("Error reading frame");
        return false;
    }
}



bool
FFmpegInput::close(void)
{
    if (m_codec_context)
        avcodec_free_context(&m_codec_context);
    if (m_format_context) {
        avformat_close_input(&m_format_context);
        av_free(m_format_context);  // will free m_codec and m_codec_context
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
    AVPacket pkt;
    int finished = 0;
    int ret      = 0;
    while ((ret = av_read_frame(m_format_context, &pkt)) == 0
           || m_codec_cap_delay) {
        if (ret == AVERROR_EOF) {
            break;
        }
        if (pkt.stream_index == m_video_stream) {
            if (ret < 0 && m_codec_cap_delay) {
                pkt.data = NULL;
                pkt.size = 0;
            }

            finished = receive_frame(m_codec_context, m_frame, &pkt);

            double pts = 0;
            if (static_cast<int64_t>(m_frame->pts) != int64_t(AV_NOPTS_VALUE)) {
                pts = av_q2d(
                          m_format_context->streams[m_video_stream]->time_base)
                      * m_frame->pts;
            }

            int current_frame = int((pts - m_start_time) * fps() + 0.5f);  //???
            //current_frame =   m_frame->display_picture_number;
            m_last_search_pos = current_frame;

            if (current_frame == frame && finished) {
                avpicture_fill(m_rgb_frame, &m_rgb_buffer[0], m_dst_pix_format,
                               m_codec_context->width, m_codec_context->height);
                sws_scale(m_sws_rgb_context,
                          static_cast<uint8_t const* const*>(m_frame->data),
                          m_frame->linesize, 0, m_codec_context->height,
                          m_rgb_frame->data, m_rgb_frame->linesize);
                m_last_decoded_pos = current_frame;
                av_packet_unref(&pkt);
                break;
            }
        }
        av_packet_unref(&pkt);
    }
    m_read_frame = true;
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
    av_seek_frame(m_format_context, -1, offset, flags);
    return true;
}



int64_t
FFmpegInput::time_stamp(int frame) const
{
    int64_t timestamp = static_cast<int64_t>(
        (static_cast<double>(frame)
         / (fps()
            * av_q2d(m_format_context->streams[m_video_stream]->time_base))));
    if (static_cast<int64_t>(m_format_context->start_time)
        != int64_t(AV_NOPTS_VALUE)) {
        timestamp += static_cast<int64_t>(
            static_cast<double>(m_format_context->start_time) * AV_TIME_BASE
            / av_q2d(m_format_context->streams[m_video_stream]->time_base));
    }
    return timestamp;
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
