/*
  Copyright 2014 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/

extern "C" { // ffmpeg is a C api
#include <errno.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}


#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc  avcodec_alloc_frame
#define av_frame_free   avcodec_free_frame
#endif

#include <boost/thread/once.hpp>

#include "OpenImageIO/imageio.h"

OIIO_PLUGIN_NAMESPACE_BEGIN


class FFmpegInput : public ImageInput {
public:
    FFmpegInput ();
    virtual ~FFmpegInput();
    virtual const char *format_name (void) const { return "FFmpeg movie"; }
    virtual bool open (const std::string &name, ImageSpec &spec);
    virtual bool close (void);
    virtual int current_subimage (void) const { return m_subimage; }
    virtual bool seek_subimage (int subimage, int miplevel, ImageSpec &newspec);
    virtual bool read_native_scanline (int y, int z, void *data);
    void read_frame(int pos);
#if 0
    const char *metadata (const char * key);
    bool has_metadata (const char * key);
#endif
    bool seek (int pos);
    double fps() const;
    int64_t time_stamp(int pos) const;
private:
    std::string m_filename;
    int m_subimage;
    int m_nsubimages;
    AVFormatContext * m_format_context;
    AVCodecContext * m_codec_context;
    AVCodec *m_codec;
    AVFrame *m_frame;
    AVFrame *m_rgb_frame;
    SwsContext *m_sws_rgb_context;
    AVRational m_frame_rate;
    std::vector<uint8_t> m_rgb_buffer;
    std::vector<int> m_video_indexes;
    int m_video_stream;
    int m_frames;
    int m_last_search_pos;
    int m_last_decoded_pos;
    bool m_offset_time;
    bool m_read_frame;
    // init to initialize state
    void init (void) {
        m_filename.clear ();
        m_format_context = 0;
        m_codec_context = 0;
        m_codec = 0;
        m_frame = 0;
        m_rgb_frame = 0;
        m_sws_rgb_context = 0;
        m_rgb_buffer.clear();
        m_video_indexes.clear();
        m_video_stream = -1;
        m_frames = 0;
        m_last_search_pos = 0;
        m_last_decoded_pos = 0;
        m_offset_time = true;
        m_read_frame = false;
    }
};



// Obligatory material to make this a recognizeable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

    OIIO_EXPORT int ffmpeg_imageio_version = OIIO_PLUGIN_VERSION;
    OIIO_EXPORT ImageInput *ffmpeg_input_imageio_create () {
        return new FFmpegInput;
    }
    // FFmpeg hints:
    // AVI (Audio Video Interleaved)
    // QuickTime / MOV
    // raw MPEG-4 video
    // MPEG-1 Systems / MPEG program stream
    OIIO_EXPORT const char *ffmpeg_input_extensions[] = {
        "avi", "mov", "qt", "mp4", "m4a", "3gp", "3g2", "mj2", "m4v", "mpg", NULL
    };
    

OIIO_PLUGIN_EXPORTS_END



FFmpegInput::FFmpegInput ()
{
    init();
}



FFmpegInput::~FFmpegInput()
{
    close();
}



bool
FFmpegInput::open (const std::string &name, ImageSpec &spec)
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
        if (Strutil::ends_with (name, ffmpeg_input_extensions[i])) {
            valid_extension = true;
            break;
        }
    if (! valid_extension) {
        error ("\"%s\" could not open input", name);
        return false;
    }

    static boost::once_flag init_flag = BOOST_ONCE_INIT;
    boost::call_once (&av_register_all, init_flag);
    const char *file_name = name.c_str();
    av_log_set_level (AV_LOG_FATAL);
    if (avformat_open_input (&m_format_context, file_name, NULL, NULL) != 0) // avformat_open_input allocs format_context
    {
        error ("\"%s\" could not open input", file_name);
        return false;
    }
    if (avformat_find_stream_info (m_format_context, NULL) < 0)
    {
        error ("\"%s\" could not find stream info", file_name);
        return false;
    }
    m_video_stream = -1;
    for (unsigned int i=0; i<m_format_context->nb_streams; i++) {
        if (m_format_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (m_video_stream < 0) {
                m_video_stream=i;
            }
            m_video_indexes.push_back (i); // needed for later use
            break;
        }
    }  
    if (m_video_stream == -1) {
        error ("\"%s\" could not find a valid videostream", file_name);
        return false;
    }
    m_codec_context = m_format_context->streams[m_video_stream]->codec; // codec context for videostream
    m_codec = avcodec_find_decoder (m_codec_context->codec_id);
    if (!m_codec) {
        error ("\"%s\" unsupported codec", file_name);
        return false;
    }
    if (avcodec_open2 (m_codec_context, m_codec, NULL) < 0) {
        error ("\"%s\" could not open codec", file_name);
        return false;
    }
    if (!strcmp (m_codec_context->codec->name, "mjpeg") ||
        !strcmp (m_codec_context->codec->name, "dvvideo")) {
        m_offset_time = false;
    }
    AVStream *stream = m_format_context->streams[m_video_stream];
    if (stream->r_frame_rate.num != 0 && stream->r_frame_rate.den != 0) {
        m_frame_rate = stream->r_frame_rate;
    }
    if (static_cast<int64_t>(m_format_context->duration) != int64_t(AV_NOPTS_VALUE)) {
        m_frames = static_cast<uint64_t> ((fps() * static_cast<double>(m_format_context->duration) / 
                                                   static_cast<uint64_t>(AV_TIME_BASE)));
    } else {
        m_frames = 1 << 29;
    }
    AVPacket pkt;
    if (!m_frames) {
        seek (0);
        av_init_packet (&pkt);
        av_read_frame (m_format_context, &pkt);
        uint64_t first_pts = pkt.pts;
        uint64_t max_pts = first_pts;
        seek (1 << 29);
        av_init_packet (&pkt);
        while (stream && av_read_frame (m_format_context, &pkt) >= 0) {
            uint64_t current_pts = static_cast<uint64_t> (av_q2d(stream->time_base) * (pkt.pts - first_pts) * fps());
            if (current_pts > max_pts) {
                max_pts = current_pts;
            }
        }
        m_frames = max_pts;
    }
    m_frame = av_frame_alloc();
    m_rgb_frame = av_frame_alloc();
    m_rgb_buffer.resize(
        avpicture_get_size (PIX_FMT_RGB24,
        m_codec_context->width,
        m_codec_context->height),
        0
    );
    AVPixelFormat pixFormat;
    switch (m_codec_context->pix_fmt) { // deprecation warning for YUV formats
        case AV_PIX_FMT_YUVJ420P:
            pixFormat = AV_PIX_FMT_YUV420P;
            break;
        case AV_PIX_FMT_YUVJ422P:
            pixFormat = AV_PIX_FMT_YUV422P;
            break;
        case AV_PIX_FMT_YUVJ444P:
            pixFormat = AV_PIX_FMT_YUV444P;
            break;
        case AV_PIX_FMT_YUVJ440P:
            pixFormat = AV_PIX_FMT_YUV440P;
        default:
            pixFormat = m_codec_context->pix_fmt;
            break;
    }
    m_sws_rgb_context = sws_getContext(
        m_codec_context->width,
        m_codec_context->height,
        pixFormat,
        m_codec_context->width,
        m_codec_context->height,
        PIX_FMT_RGB24,
        SWS_AREA,
        NULL,
        NULL,
        NULL
    );
    m_spec = ImageSpec (m_codec_context->width, m_codec_context->height, 3, TypeDesc::UINT8);
    AVDictionaryEntry *tag = NULL;
    while ((tag = av_dict_get (m_format_context->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        m_spec.attribute (tag->key, tag->value);
    }
    m_spec.attribute ("FramesPerSecond", m_frame_rate.num / static_cast<float> (m_frame_rate.den));
    m_spec.attribute ("oiio:Movie", true);
    m_nsubimages = m_frames;
    spec = m_spec;
    return true;
}



bool
FFmpegInput::seek_subimage (int subimage, int miplevel, ImageSpec &newspec)
{
    if (subimage < 0 || subimage >= m_nsubimages || miplevel > 0) {
        return false;
    }
    if (subimage == m_subimage) {
        newspec = m_spec;
        return true;
    }
    newspec = m_spec;
    m_subimage = subimage;
    m_read_frame = false;
    return true;
}



bool
FFmpegInput::read_native_scanline (int y, int z, void *data)
{
    if (!m_read_frame) {
        read_frame (m_subimage);
    }
    memcpy (data, m_rgb_frame->data[0] + y * m_rgb_frame->linesize[0], m_spec.width*3);
    return true;
}



bool
FFmpegInput::close (void)
{
    avcodec_close (m_codec_context);
    avformat_close_input (&m_format_context);
    av_free (m_format_context); // will free m_codec and m_codec_context
    av_frame_free (&m_frame); // free after close input
    av_frame_free (&m_rgb_frame);
    sws_freeContext (m_sws_rgb_context);
    init ();
    return true;
}



void
FFmpegInput::read_frame(int pos)
{
    if (m_last_decoded_pos + 1 != m_subimage) {
        seek (0);
        seek (m_subimage);
    }
    AVPacket pkt;
    int finished = 0;
    while (av_read_frame (m_format_context, &pkt) >=0) {
        if (pkt.stream_index == m_video_stream) {
            double pts = 0;
            if (static_cast<int64_t>(pkt.dts) != int64_t(AV_NOPTS_VALUE)) {
                pts = av_q2d (m_format_context->streams[m_video_stream]->time_base) * pkt.dts;
            }
            int current_pos = int(pts * fps() + 0.5f);
            if (current_pos == m_last_search_pos) {
                current_pos = m_last_search_pos + 1;
            }
            m_last_search_pos = current_pos;
            if (static_cast<int64_t>(m_format_context->start_time) != int64_t(AV_NOPTS_VALUE)) {
                current_pos -= static_cast<int> (m_format_context->start_time * fps() / AV_TIME_BASE);
            }
            if (current_pos >= m_subimage) {
                avcodec_decode_video2 (m_codec_context, m_frame, &finished, &pkt);
            }
            if(finished)
            {
                avpicture_fill
                (
                    reinterpret_cast<AVPicture*>(m_rgb_frame),
                    &m_rgb_buffer[0],
                    PIX_FMT_RGB24,
                    m_codec_context->width,
                    m_codec_context->height
                );
                sws_scale
                (
                    m_sws_rgb_context,
                    static_cast<uint8_t const * const *> (m_frame->data),
                    m_frame->linesize,
                    0,
                    m_codec_context->height,
                    m_rgb_frame->data,
                    m_rgb_frame->linesize
                );
                m_last_decoded_pos = m_last_search_pos; 
                av_free_packet (&pkt);
                break;
            }
        }
        av_free_packet (&pkt);
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
FFmpegInput::seek (int pos)
{
    int64_t offset = time_stamp (pos);
    if (m_offset_time) {
        offset -= AV_TIME_BASE;
        if (offset < m_format_context->start_time) {
            offset = 0;
        }
    }
    avcodec_flush_buffers (m_codec_context);
    if (av_seek_frame (m_format_context, -1, offset, AVSEEK_FLAG_BACKWARD) < 0) {
        return false;
    }
    return true;
}



int64_t
FFmpegInput::time_stamp(int pos) const
{
    int64_t timestamp = static_cast<int64_t>((static_cast<double> (pos) / fps()) * AV_TIME_BASE);
    if (static_cast<int64_t>(m_format_context->start_time) != int64_t(AV_NOPTS_VALUE)) {
        timestamp += m_format_context->start_time;
    }
    return timestamp;
}



double
FFmpegInput::fps() const
{
    if (m_frame_rate.den) {
        return m_frame_rate.num / static_cast<double> (m_frame_rate.den);
    }
    return 1.0f;
}

OIIO_PLUGIN_NAMESPACE_END
