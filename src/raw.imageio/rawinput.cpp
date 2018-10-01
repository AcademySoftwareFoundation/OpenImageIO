/*
  Copyright 2013 Larry Gritz and the other authors and contributors.
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

#include <algorithm>
#include <iostream>
#include <ctime>       /* time_t, struct tm, gmtime */

#include <OpenImageIO/platform.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/strutil.h>

#if OIIO_GNUC_VERSION >= 80000
// fix gcc8 warnings in libraw headers: use of auto_ptr
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <libraw/libraw.h>
#include <libraw/libraw_version.h>


// This plugin utilises LibRaw:
//   http://www.libraw.org/
// Documentation:
//   http://www.libraw.org/docs
// Example raw images from many camera models:
//   https://www.rawsamples.ch


OIIO_PLUGIN_NAMESPACE_BEGIN

class RawInput final : public ImageInput {
public:
    RawInput () {}
    virtual ~RawInput() { close(); }
    virtual const char * format_name (void) const { return "raw"; }
    virtual int supports (string_view feature) const {
        return (feature == "exif"
             /* not yet? || feature == "iptc"*/);
    }
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool open (const std::string &name, ImageSpec &newspec,
                       const ImageSpec &config);
    virtual bool close();
    virtual bool read_native_scanline (int y, int z, void *data);

private:
    bool process();
    bool m_process = true;
    bool m_unpacked = false;
    std::unique_ptr<LibRaw> m_processor;
    libraw_processed_image_t *m_image = nullptr;
    std::string m_filename;
    ImageSpec m_config;  // save config requests

    bool do_unpack();
    void read_tiff_metadata (const std::string &filename);

    // Do the actual open. It expects m_filename and m_config to be set.
    bool open_raw (bool unpack, const std::string &name,
                   const ImageSpec &config);
};



// Export version number and create function symbols
OIIO_PLUGIN_EXPORTS_BEGIN

    OIIO_EXPORT int raw_imageio_version = OIIO_PLUGIN_VERSION;
    OIIO_EXPORT const char* raw_imageio_library_version () {
        return ustring::format("libraw %s", libraw_version()).c_str();
    }
    OIIO_EXPORT ImageInput *raw_input_imageio_create () {
        return new RawInput;
    }
    OIIO_EXPORT const char *raw_input_extensions[] = {
        "bay", "bmq", "cr2", "crw", "cs1", "dc2", "dcr", "dng",
        "erf", "fff", "hdr", "k25", "kdc", "mdc", "mos", "mrw",
        "nef", "orf", "pef", "pxn", "raf", "raw", "rdc", "sr2",
        "srf", "x3f", "arw", "3fr", "cine", "ia", "kc2", "mef",
        "nrw", "qtk", "rw2", "sti", "rwl", "srw", "drf", "dsc",
        "ptx", "cap", "iiq", "rwz", NULL
    };

OIIO_PLUGIN_EXPORTS_END



bool
RawInput::open (const std::string &name, ImageSpec &newspec)
{
    // If user doesn't want to provide any config, just use an empty spec.
    ImageSpec config;
    return open(name, newspec, config);
}



bool
RawInput::open (const std::string &name, ImageSpec &newspec,
                const ImageSpec &config)
{
    m_filename = name;
    m_config = config;

    // For a fresh open, we are concerned with just reading all the
    // meatadata quickly, because maybe that's all that will be needed. So
    // call open_raw passing unpack=false. This will not read the pixels! We
    // will need to close and re-open with unpack=true if and when we need
    // the actual pixel values.
    bool ok = open_raw (false, m_filename, m_config);
    if (ok)
        newspec = m_spec;
    return ok;
}



bool
RawInput::open_raw (bool unpack, const std::string &name,
                    const ImageSpec &config)
{
    // std::cout << "open_raw " << name << " unpack=" << unpack << "\n";
    ASSERT (! m_processor);
    m_processor.reset (new LibRaw);

    // open the image
    m_filename = name;
    int ret;
    if ( (ret = m_processor->open_file(name.c_str()) ) != LIBRAW_SUCCESS) {
        error ("Could not open file \"%s\", %s", m_filename, libraw_strerror(ret));
        return false;
    }

    ASSERT (!m_unpacked);
    if (unpack) {
        if ( (ret = m_processor->unpack() ) != LIBRAW_SUCCESS) {
            error ("Could not unpack \"%s\", %s", m_filename, libraw_strerror(ret));
            return false;
        }
    }
    m_processor->adjust_sizes_info_only();

    // Set file information
    m_spec = ImageSpec(m_processor->imgdata.sizes.iwidth,
                       m_processor->imgdata.sizes.iheight,
                       3, // LibRaw should only give us 3 channels
                       TypeDesc::UINT16);

    // Output 16 bit images
    m_processor->imgdata.params.output_bps = 16;

    // Set the gamma curve to Linear
    m_spec.attribute("oiio:ColorSpace","Linear");
    m_processor->imgdata.params.gamm[0] = 1.0;
    m_processor->imgdata.params.gamm[1] = 1.0;

    // Disable exposure correction (unless config "raw:auto_bright" == 1)
    m_processor->imgdata.params.no_auto_bright =
        ! config.get_int_attribute("raw:auto_bright", 0);
    // Use camera white balance if "raw:use_camera_wb" is not 0
    m_processor->imgdata.params.use_camera_wb =
        config.get_int_attribute("raw:use_camera_wb", 1);
    // Turn off maximum threshold value (unless set to non-zero)
    m_processor->imgdata.params.adjust_maximum_thr =
        config.get_float_attribute("raw:adjust_maximum_thr", 0.0f);
    // Set camera maximum value if "raw:user_sat" is not 0
    m_processor->imgdata.params.user_sat =
        config.get_int_attribute("raw:user_sat", 0);

    // Use embedded color profile. Values mean:
    // 0: do not use embedded color profile
    // 1 (default): use embedded color profile (if present) for DNG files
    //    (always), for other files only if use_camera_wb is set.
    // 3: use embedded color data (if present) regardless of white
    //    balance setting.
    m_processor->imgdata.params.use_camera_matrix =
        config.get_int_attribute("raw:use_camera_matrix", 1);


    // Check to see if the user has explicitly set the output colorspace primaries
    std::string cs = config.get_string_attribute ("raw:ColorSpace", "sRGB");
    if (cs.size()) {
        static const char *colorspaces[] = { "raw",
                                             "sRGB",
                                             "Adobe",
                                             "Wide",
                                             "ProPhoto",
                                             "XYZ",
#if LIBRAW_VERSION >= LIBRAW_MAKE_VERSION(0,18,0)
                                             "ACES",
#endif
                                             NULL
                                             };

        size_t c;
        for (c=0; colorspaces[c]; c++)
            if (Strutil::iequals (cs, colorspaces[c]))
                break;
        if (colorspaces[c])
            m_processor->imgdata.params.output_color = c;
        else {
#if LIBRAW_VERSION < LIBRAW_MAKE_VERSION(0,18,0)
            if (cs == "ACES")
                error ("raw:ColorSpace value of \"ACES\" is not supported by libRaw %d.%d.%d",
                       LIBRAW_MAJOR_VERSION, LIBRAW_MINOR_VERSION,
                       LIBRAW_PATCH_VERSION);
            else
#endif
            error("raw:ColorSpace set to unknown value");
            return false;
        }
        // Set the attribute in the output spec
        m_spec.attribute("raw:ColorSpace", cs);
    } else {
        // By default we use sRGB primaries for simplicity
        m_processor->imgdata.params.output_color = 1;
        m_spec.attribute("raw:ColorSpace", "sRGB");
    }

    // Exposure adjustment
    float exposure = config.get_float_attribute ("raw:Exposure", -1.0f);
    if (exposure >= 0.0f) {
        if (exposure < 0.25f || exposure > 8.0f) {
            error("raw:Exposure invalid value. range 0.25f - 8.0f");
            return false;
        }
        m_processor->imgdata.params.exp_correc = 1; // enable exposure correction
        m_processor->imgdata.params.exp_shift = exposure; // set exposure correction
        // Set the attribute in the output spec
        m_spec.attribute ("raw:Exposure", exposure);
    }

    // Highlight adjustment
    int highlight_mode = config.get_int_attribute("raw:HighlightMode", 0);
    if (highlight_mode != 0)
    {
        if (highlight_mode < 0 || highlight_mode > 9) {
            error("raw:HighlightMode invalid value. range 0-9");
            return false;
        }
        m_processor->imgdata.params.highlight = highlight_mode;
        m_spec.attribute ("raw:HighlightMode", highlight_mode);
    }

    // Interpolation quality
    // note: LibRaw must be compiled with demosaic pack GPL2 to use demosaic
    // algorithms 5-9. It must be compiled with demosaic pack GPL3 for
    // algorithm 10 (AMAzE). If either of these packs are not included, it
    // will silently use option 3 - AHD.
    std::string demosaic = config.get_string_attribute ("raw:Demosaic");
    if (demosaic.size()) {
        static const char *demosaic_algs[] = { "linear",
                                               "VNG",
                                               "PPG",
                                               "AHD",
                                               "DCB",
                                               "AHD-Mod",
                                               "AFD",
                                               "VCD",
                                               "Mixed",
                                               "LMMSE",
                                               "AMaZE",
#if LIBRAW_VERSION >= LIBRAW_MAKE_VERSION(0,16,0)
                                               "DHT",
                                               "AAHD",
#endif
                                               // Future demosaicing algorithms should go here
                                               NULL
                                               };
        size_t d;
        for (d=0; demosaic_algs[d]; d++)
            if (Strutil::iequals (demosaic, demosaic_algs[d]))
                break;
        if (demosaic_algs[d])
            m_processor->imgdata.params.user_qual = d;
        else if (Strutil::iequals (demosaic, "none")) {
#ifdef LIBRAW_DECODER_FLATFIELD
            // See if we can access the Bayer patterned data for this raw file
            libraw_decoder_info_t decoder_info;
            m_processor->get_decoder_info(&decoder_info);
            if (!(decoder_info.decoder_flags & LIBRAW_DECODER_FLATFIELD)) {
                error("Unable to extract unbayered data from file \"%s\"", name.c_str());
                return false;
            }

#endif
            // User has selected no demosaicing, so no processing needs to be done
            m_process = false;

            // This will read back a single, bayered channel
            m_spec.nchannels = 1;
            m_spec.channelnames.clear();
            m_spec.channelnames.emplace_back("Y");

            // Also, any previously set demosaicing options are void, so remove them
            m_spec.erase_attribute("oiio:Colorspace");
            m_spec.erase_attribute("raw:Colorspace");
            m_spec.erase_attribute("raw:Exposure");
        }
        else {
            error("raw:Demosaic set to unknown value");
            return false;
        }
        // Set the attribute in the output spec
        m_spec.attribute("raw:Demosaic", demosaic);
    } else {
        m_processor->imgdata.params.user_qual = 3;
        m_spec.attribute("raw:Demosaic", "AHD");
    }

    // Metadata

    const libraw_image_sizes_t &sizes (m_processor->imgdata.sizes);
    m_spec.attribute ("PixelAspectRatio", (float)sizes.pixel_aspect);
    // FIXME: sizes. top_margin, left_margin, raw_pitch, mask?

    const libraw_iparams_t &idata (m_processor->imgdata.idata);
    if (idata.make[0])
        m_spec.attribute ("Make", idata.make);
    if (idata.model[0])
        m_spec.attribute ("Model", idata.model);
    // FIXME: idata. dng_version, is_foveon, colors, filters, cdesc

    const libraw_colordata_t &color (m_processor->imgdata.color);
    m_spec.attribute("Exif:Flash", (int) color.flash_used);
    if (color.model2[0])
        m_spec.attribute ("Software", color.model2);

    // FIXME -- all sorts of things in this struct

    const libraw_imgother_t &other (m_processor->imgdata.other);
    m_spec.attribute ("Exif:ISOSpeedRatings", (int) other.iso_speed);
    m_spec.attribute ("ExposureTime", other.shutter);
    m_spec.attribute ("Exif:ShutterSpeedValue", -log2f(other.shutter));
    m_spec.attribute ("FNumber", other.aperture);
    m_spec.attribute ("Exif:ApertureValue", 2.0f * log2f(other.aperture));
    m_spec.attribute ("Exif:FocalLength", other.focal_len);
    struct tm * m_tm = localtime(&m_processor->imgdata.other.timestamp);
    char datetime[20];
    strftime (datetime, 20, "%Y-%m-%d %H:%M:%S", m_tm);
    m_spec.attribute ("DateTime", datetime);
    // FIXME: other.shot_order
    // FIXME: other.gpsdata
    if (other.desc[0])
        m_spec.attribute ("ImageDescription", other.desc);
    if (other.artist[0])
        m_spec.attribute ("Artist", other.artist);

    // libraw reoriented the image for us, so squash any orientation
    // metadata we may have found in the Exif. Preserve the original as
    // "raw:Orientation".
    m_spec.attribute ("Orientation", m_spec.get_int_attribute("Orientation", 1));
    m_spec.attribute ("Orientation", 1);

    // FIXME -- thumbnail possibly in m_processor->imgdata.thumbnail

    read_tiff_metadata (name);

    return true;
}



void
RawInput::read_tiff_metadata (const std::string &filename)
{
    // Many of these raw formats look just like TIFF files, and we can use
    // that to extract a bunch of extra Exif metadata and thumbnail.
    ImageInput *in = ImageInput::create ("tiff");
    if (! in) {
        (void) OIIO::geterror();  // eat the error
        return;
    }
    ImageSpec newspec;
    bool ok = in->open (filename, newspec);
    if (ok) {
        // Transfer "Exif:" metadata to the raw spec.
        for (ParamValueList::const_iterator p = newspec.extra_attribs.begin();
             p != newspec.extra_attribs.end();  ++p) {
            if (Strutil::istarts_with (p->name().c_str(), "Exif:")) {
                m_spec.attribute (p->name().c_str(), p->type(), p->data());
            }
        }
    }

    in->close ();
    delete in;
}



bool
RawInput::close()
{
    if (m_image) {
        LibRaw::dcraw_clear_mem(m_image);
        m_image = nullptr;
    }
    m_processor.reset ();
    m_unpacked = false;
    m_process = true;
    return true;
}



bool
RawInput::do_unpack ()
{
    if (m_unpacked)
        return true;

    // We need to unpack but we didn't when we opened the file. Close and
    // re-open with unpack.
    close ();
    bool ok = open_raw (true, m_filename, m_config);
    m_unpacked = true;
    return ok;
}



bool
RawInput::process()
{
    if (!m_image) {
        int ret = m_processor->dcraw_process();
        if (ret != LIBRAW_SUCCESS) {
            error("Processing image failed, %s", libraw_strerror(ret));
            return false;
        }

        m_image = m_processor->dcraw_make_mem_image(&ret);
        if (!m_image) {
            error("LibRaw failed to create in memory image");
            return false;
        }

        if (m_image->type != LIBRAW_IMAGE_BITMAP) {
            error("LibRaw did not return expected image type");
            return false;
        }

        if (m_image->colors != 3) {
            error("LibRaw did not return 3 channel image");
            return false;
        }

    }
    return true;
}



bool
RawInput::read_native_scanline (int y, int z, void *data)
{
    if (y < 0 || y >= m_spec.height) // out of range scanline
        return false;

    if (! m_unpacked)
        do_unpack ();

    if (! m_process) {
        // The user has selected not to apply any debayering.
        // We take the raw data directly
        unsigned short *scanline = &((m_processor->imgdata.rawdata.raw_image)[m_spec.width*y]);
        memcpy(data, scanline, m_spec.scanline_bytes(true));
        return true;
    }

    // Check the state of the internal RAW reader.
    // Have to load the entire image at once, so only do this once
    if (! m_image) { 
        if (!process()) {
            return false;
        }
    }

    int length = m_spec.width*m_image->colors; // Should always be 3 colors

    // Because we are reading UINT16's, we need to cast m_image->data
    unsigned short *scanline = &(((unsigned short *)m_image->data)[length*y]);
    memcpy(data, scanline, m_spec.scanline_bytes(true));

    return true;
}

OIIO_PLUGIN_NAMESPACE_END

