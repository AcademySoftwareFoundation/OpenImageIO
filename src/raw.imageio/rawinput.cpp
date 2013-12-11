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

#include "imageio.h"
#include <iostream>
#include <time.h>       /* time_t, struct tm, gmtime */
#include <libraw/libraw.h>

// This plugin utilises LibRaw:
// http://www.libraw.org/
// Documentation: 
// http://www.libraw.org/docs

OIIO_PLUGIN_NAMESPACE_BEGIN

class RawInput : public ImageInput {
public:
    RawInput () : m_process(true), m_image(NULL) {}
    virtual ~RawInput() { close(); }
    virtual const char * format_name (void) const { return "raw"; }
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool open (const std::string &name, ImageSpec &newspec, ImageSpec &config);
    virtual bool close();
    virtual bool read_native_scanline (int y, int z, void *data);

private:
    bool process();
    bool m_process;
    LibRaw m_processor;
    libraw_processed_image_t *m_image;
};



// Export version number and create function symbols
OIIO_PLUGIN_EXPORTS_BEGIN

    OIIO_EXPORT int raw_imageio_version = OIIO_PLUGIN_VERSION;
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
RawInput::open(const std::string &name, ImageSpec &newspec) {
    // If user doesn't want to provide any config, just use an empty spec.
    ImageSpec config;
    return open(name, newspec, config);
}



bool
RawInput::open(const std::string &name, ImageSpec &newspec, ImageSpec &config) {
    int ret;

    // open the image
    if ( (ret = m_processor.open_file(name.c_str()) ) != LIBRAW_SUCCESS) {
        error ("Could not open file \"%s\", %s", name.c_str(), libraw_strerror(ret));
        return false;
    }

    if ( (ret = m_processor.unpack() ) != LIBRAW_SUCCESS) {
        error ("Could not unpack \"%s\", %s",name.c_str(), libraw_strerror(ret));
        return false;
    }

    // Set file information
    m_spec = ImageSpec(m_processor.imgdata.sizes.iwidth,
                       m_processor.imgdata.sizes.iheight,
                       3, // LibRaw should only give us 3 channels
                       TypeDesc::UINT16);

    // Output 16 bit images
    m_processor.imgdata.params.output_bps = 16;


    // Set the gamma curve to Linear
    m_spec.attribute("oiio:ColorSpace","Linear");
    m_processor.imgdata.params.gamm[0] = 1.0;
    m_processor.imgdata.params.gamm[1] = 1.0;

    // Check to see if the user has explicitly set the output colorspace primaries
    ImageIOParameter *csp = config.find_attribute ("raw:ColorSpace", TypeDesc::STRING, false);
    if (csp) {
        static std::string colorspaces[] = { "raw",
                                             "sRGB",
                                             "Adobe",
                                             "Wide",
                                             "ProPhoto",
                                             "XYZ"
                                             };

        std::string cs = *(const char**) csp->data();
        size_t c;
        for (c=0; c < sizeof(colorspaces) / sizeof(std::string); c++) {
            if (cs == colorspaces[c]) {
                break;
            }
        }
        if (cs == colorspaces[c]) {
            m_processor.imgdata.params.output_color = c;
        }
        else {
            error("raw:ColorSpace set to unknown value");
            return false;
        }
        // Set the attribute in the output spec
        m_spec.attribute("raw:ColorSpace", cs);
    }
    else {
        // By default we use sRGB primaries for simplicity
        m_processor.imgdata.params.output_color = 1;
        m_spec.attribute("raw:ColorSpace", "sRGB");
    }


    // Exposure adjustment
    ImageIOParameter *ex = config.find_attribute ("raw:Exposure", TypeDesc::FLOAT, false);
    
    if (ex) {
        float exposure = *(float*)ex->data();
        
        if (exposure < 0.25f || exposure > 8.0f)
        {
            error("raw:Exposure invalid value. range 0.25f - 8.0f");
            return false;
        }
        
        m_processor.imgdata.params.exp_correc = 1; // enable exposure correction
        m_processor.imgdata.params.exp_shift = exposure; // set exposure correction

        // Set the attribute in the output spec
        m_spec.attribute("raw:Exposure", exposure);
    }


    // Interpolation quality
    // note: LibRaw must be compiled with demosaic pack GPL2 to use
    // demosaic algorithms 5-9. It must be compiled with demosaic pack GPL3 for 
    // algorithm 10. If either of these packs are not includeded, it will silently use option 3 - AHD
    ImageIOParameter *dm = config.find_attribute ("raw:Demosaic", TypeDesc::STRING, false);
    if (dm) {
        static std::string demosaic_algs[] = { "linear",
                                               "VNG",
                                               "PPG",
                                               "AHD",
                                               "DCB",
                                               "Modified AHD",
                                               "AFD",
                                               "VCD",
                                               "Mixed",
                                               "LMMSE",
                                               "AMaZE"
                                               // Future demosaicing algorithms should go here
                                               };

        std::string demosaic = *(const char**) dm->data();
        size_t d;
        for (d=0; d < sizeof(demosaic_algs) / sizeof(std::string); d++) {
            if (demosaic == demosaic_algs[d]) {
                break;
            }
        }
        if (demosaic == demosaic_algs[d]) {
            m_processor.imgdata.params.user_qual = d;
        }
        else if (demosaic == "none") {
            // See if we can access the Bayer patterned data for this raw file
            libraw_decoder_info_t decoder_info;
            m_processor.get_decoder_info(&decoder_info);
            if (!(decoder_info.decoder_flags & LIBRAW_DECODER_FLATFIELD))
            {
                error("Unable to extract unbayered data from file \"%s\"", name.c_str());
                return false;
            }

            // User has selected no demosaicing, so no processing needs to be done
            m_process = false;

            // The image width and height may be different now, so update with new values
            // Also we will only be reading back a single, bayered channel
            m_spec.width = m_processor.imgdata.sizes.raw_width;
            m_spec.height = m_processor.imgdata.sizes.raw_height;
            m_spec.nchannels = 1;
            m_spec.channelnames.clear(); m_spec.channelnames.push_back("R");

            // Also, any previously set demosaicing options are void, so remove them
            m_spec.erase_attribute("oiio:Colorspace", TypeDesc::STRING);
            m_spec.erase_attribute("raw:Colorspace", TypeDesc::STRING);
            m_spec.erase_attribute("raw:Exposure", TypeDesc::STRING);

        }
        else {
            error("raw:Demosaic set to unknown value");
            return false;
        }
        // Set the attribute in the output spec
        m_spec.attribute("raw:Demosaic", demosaic);
    }
    else {
        m_processor.imgdata.params.user_qual = 3;
        m_spec.attribute("raw:Demosaic", "AHD");
    }


    // Metadata
    m_spec.attribute("Exif:Flash", (int) m_processor.imgdata.color.flash_used);
    m_spec.attribute("Exif:ISOSpeedRatings", (int) m_processor.imgdata.other.iso_speed);
    m_spec.attribute("Exif:ShutterSpeedValue", m_processor.imgdata.other.shutter);
    m_spec.attribute("Exif:ApertureValue", m_processor.imgdata.other.aperture);
    m_spec.attribute("Exif:FocalLength", m_processor.imgdata.other.focal_len);
    //TODO work out GPS
    if (std::string(m_processor.imgdata.other.desc) != "") {
        m_spec.attribute("ImageDescription", m_processor.imgdata.other.desc);
    }
    if (std::string(m_processor.imgdata.other.artist) != "") {
        m_spec.attribute("Artist", m_processor.imgdata.other.artist);
    }
    struct tm * m_tm = gmtime(&m_processor.imgdata.other.timestamp);
    char datetime[20]; strftime(datetime, 20, "%Y-%m-%d %H:%M:%S", m_tm);
    m_spec.attribute("DateTime", datetime);

    // Copy the spec to return to the user
    newspec = m_spec;
    return true;
}


bool
RawInput::close()
{
    if(m_image) {
        LibRaw::dcraw_clear_mem(m_image);
        m_image = NULL;
    }
    return true;
}


bool
RawInput::process()
{
    if (!m_image) {
        int ret = m_processor.dcraw_process();
        
        if (ret != LIBRAW_SUCCESS) {
            error("Processing image failed, %s", libraw_strerror(ret));
            return false;
        }

        m_image = m_processor.dcraw_make_mem_image(&ret);

        if (!m_image) {
            error("LibRaw failed to create in memory image");
            return false;
        }

        if(m_image->type != LIBRAW_IMAGE_BITMAP) {
            error("LibRaw did not return expected image type");
            return false;
        }

        if(m_image->colors != 3) {
            error("LibRaw did not return 3 channel image");
            return false;
        }

    }
    return true;
}


bool
RawInput::read_native_scanline (int y, int z, void *data)
{
    if ( y < 0 || y >= m_spec.height) // out of range scanline
        return false;

    if (! m_process) {
        // The user has selected not to apply any debayering.
        // We take the raw data directly
        unsigned short *scanline = &((m_processor.imgdata.rawdata.raw_image)[m_spec.width*y]);
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

