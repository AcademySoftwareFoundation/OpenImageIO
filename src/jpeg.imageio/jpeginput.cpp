/////////////////////////////////////////////////////////////////////////////
// Copyright 2004 NVIDIA Corporation and Copyright 2008 Larry Gritz.
// All Rights Reserved.
//
// Extensions by Larry Gritz based on open-source code by NVIDIA.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of NVIDIA nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// (This is the Modified BSD License)
/////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <ctype.h>
#include <cstdio>
#include <iostream>

#include <boost/scoped_array.hpp>

#include "imageio.h"
using namespace OpenImageIO;

#include "fmath.h"
#include "thread.h"

extern "C" {
#include "jpeglib.h"
#include "tiff.h"
}



static mutex marker_mutex;   // Guard non-reentrant marker
static ImageIOFormatSpec *marker_spec;  // Spec that my_marker_handler mods



// See JPEG library documentation in /usr/share/doc/libjpeg-devel-6b


class JpgInput : public ImageInput {
 public:
    JpgInput () { init(); }
    virtual ~JpgInput () { close(); }
    virtual const char * format_name (void) const { return "JPEG"; }
    virtual bool open (const char *name, ImageIOFormatSpec &spec);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool close ();
 private:
    FILE *fd;
    bool first_scanline;
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    void init () { fd = NULL; }
};



// Export version number and create function symbols
extern "C" {
    DLLEXPORT int imageio_version = IMAGEIO_VERSION;
    DLLEXPORT JpgInput *jpeg_input_imageio_create () {
        return new JpgInput;
    }
    DLLEXPORT const char *jpeg_input_extensions[] = {
        "jpg", "jpe", "jpeg", NULL
    };
};



static void
add_tag (const char *name, TIFFDirEntry *dirp, const char *buf, bool swab)
{
    if (dirp->tdir_type == TIFF_SHORT && dirp->tdir_count == 1) {
        unsigned short d;
        d = * (unsigned short *) &dirp->tdir_offset;  // short stored in offset itself
        if (swab)
            swap_endian (&d);
        marker_spec->add_parameter (name, (int)d);
    } else if (dirp->tdir_type == TIFF_LONG && dirp->tdir_count == 1) {
        unsigned int d;
        d = * (unsigned int *) &dirp->tdir_offset;  // int stored in offset itself
        if (swab)
            swap_endian (&d);
        marker_spec->add_parameter (name, (int)d);
    } else if (dirp->tdir_type == TIFF_RATIONAL && dirp->tdir_count == 1) {
        unsigned int num, den;
        num = ((unsigned int *) &(buf[dirp->tdir_offset]))[0];
        den = ((unsigned int *) &(buf[dirp->tdir_offset]))[1];
        if (swab) {
            swap_endian (&num);
            swap_endian (&den);
        }
        double db = (double)num / (double)den;
        marker_spec->add_parameter (name, (float)db);
    } else if (dirp->tdir_type == TIFF_SRATIONAL && dirp->tdir_count == 1) {
        unsigned int num, den;
        num = ((unsigned int *) &(buf[dirp->tdir_offset]))[0];
        den = ((unsigned int *) &(buf[dirp->tdir_offset]))[1];
        if (swab) {
            swap_endian (&num);
            swap_endian (&den);
        }
        double db = (double)num / (double)den;
        marker_spec->add_parameter (name, (float)db);
    } else if (dirp->tdir_type == TIFF_ASCII) {
        marker_spec->add_parameter (name, buf+dirp->tdir_offset);
    } else {
        std::cerr << "didn't know how to process type " 
                  << dirp->tdir_type << " x " << dirp->tdir_count << "\n";
    }
}



#if 0
static std::string
resunit_tag (TIFFDirEntry *dirp, const char *buf, bool swab)
{
    if (dirp->tdir_type != TIFF_SHORT)
        return "none";
    short s = add_tag (dirp, buf, swab);
    if (s == RESUNIT_INCH)
        return "in";
    else if (s == RESUNIT_CENTIMETER)
        return "cm";
    return "none";
}
#endif



class TagMap {
    typedef std::map<int, const char *> map_t;
public:
    TagMap (void) { init(); }

    const char *operator[] (int index) const {
        map_t::const_iterator i;
        i = m_map.find (index);
        return i == m_map.end() ? NULL : i->second;
    }
private:
    map_t m_map;

    void init (void) {
        // Skip ones handled by the usual JPEG code
        m_map[TIFFTAG_IMAGEWIDTH] = NULL;
        m_map[TIFFTAG_IMAGELENGTH] = NULL;
        m_map[TIFFTAG_BITSPERSAMPLE] = NULL;
        m_map[TIFFTAG_COMPRESSION] = NULL;
        m_map[TIFFTAG_PHOTOMETRIC] = NULL;
        m_map[TIFFTAG_SAMPLESPERPIXEL] = NULL;
        m_map[TIFFTAG_PLANARCONFIG] = NULL;
        m_map[TIFFTAG_YCBCRSUBSAMPLING] = NULL;
        m_map[TIFFTAG_YCBCRPOSITIONING] = NULL;

        // TIFF tags we may come across
        m_map[TIFFTAG_ORIENTATION] = "orientation";
        m_map[TIFFTAG_XRESOLUTION] = "xresolution";
        m_map[TIFFTAG_YRESOLUTION] = "yresolution";
        m_map[TIFFTAG_RESOLUTIONUNIT] = "resolutionunit";
        m_map[TIFFTAG_IMAGEDESCRIPTION] = "description";
        m_map[TIFFTAG_MAKE] = "make";
        m_map[TIFFTAG_MODEL] = "model";
        m_map[TIFFTAG_SOFTWARE] = "software";
        m_map[TIFFTAG_ARTIST] = "artist";
        m_map[TIFFTAG_COPYRIGHT] = "copyright";
        m_map[TIFFTAG_DATETIME] = "datetime";

        // EXIF tags we may come across
        m_map[EXIFTAG_EXPOSURETIME] = "exposuretime";
        m_map[EXIFTAG_FNUMBER] = "fstop";  // exif_FNumber?
        m_map[EXIFTAG_EXPOSUREPROGRAM] = "exif_ExposureProgram"; // ?? translate to ascii names?
        m_map[EXIFTAG_SPECTRALSENSITIVITY] = "exif_SpectralSensitivity";
        m_map[EXIFTAG_ISOSPEEDRATINGS] = "exif_IFOSpeedRatings";
        m_map[EXIFTAG_OECF] = NULL;  // skip it
        m_map[EXIFTAG_EXIFVERSION] = NULL;  // skip it
        m_map[EXIFTAG_DATETIMEORIGINAL] = "exif_DateTimeOriginal";
        m_map[EXIFTAG_DATETIMEDIGITIZED] = "exif_DateTimeDigitized";
        m_map[EXIFTAG_COMPONENTSCONFIGURATION] = "exif_ComponentsConfiguration";
        m_map[EXIFTAG_COMPRESSEDBITSPERPIXEL] = "exif_CompressedBitsPerPixel";
        m_map[EXIFTAG_SHUTTERSPEEDVALUE] = "exif_ShutterSpeedValue"; // APEX units
        m_map[EXIFTAG_APERTUREVALUE] = "exif_ApertureValue"; // APEX units
        m_map[EXIFTAG_BRIGHTNESSVALUE] = "exif_BrightnessValue";
        m_map[EXIFTAG_EXPOSUREBIASVALUE] = "exif_ExposureBiasValue";
        m_map[EXIFTAG_MAXAPERTUREVALUE] = "exif_MaxApertureValue";
        m_map[EXIFTAG_SUBJECTDISTANCE] = "exif_SubjectDistance";
        m_map[EXIFTAG_METERINGMODE] = "exif_MeteringMode"; // translate to tokens?
        m_map[EXIFTAG_LIGHTSOURCE] = "exif_LightSource"; // translate to tokens?
        m_map[EXIFTAG_FLASH] = "exif_Flash";  // translate to tokens?
        m_map[EXIFTAG_FOCALLENGTH] = "focallength";
        m_map[EXIFTAG_SUBJECTAREA] = "exif_SubjectArea";
        m_map[EXIFTAG_MAKERNOTE] = NULL;  // skip it
        m_map[EXIFTAG_USERCOMMENT] = NULL; // skip it
        m_map[EXIFTAG_SUBSECTIME] = "exif_SubsecTime";
        m_map[EXIFTAG_SUBSECTIMEORIGINAL] = "exif_SubsecTimeOriginal";
        m_map[EXIFTAG_SUBSECTIMEDIGITIZED] = "exif_SubsecTimeDigitized";
        m_map[EXIFTAG_FLASHPIXVERSION] = NULL; // skip "exif_FlashPixVesion";
        m_map[EXIFTAG_COLORSPACE] = "exif_ColorSpace";
        m_map[EXIFTAG_PIXELXDIMENSION] = "exif_PixelXDimension";
        m_map[EXIFTAG_PIXELYDIMENSION] = "exif_PixelTDimension";
        m_map[EXIFTAG_RELATEDSOUNDFILE] = NULL; // skip
        m_map[EXIFTAG_FLASHENERGY] = "exif_FlashEnergy";
        m_map[EXIFTAG_SPATIALFREQUENCYRESPONSE] = "exif_SpatialFrequencyResponse";
        m_map[EXIFTAG_FOCALPLANEXRESOLUTION] = "exif_FocalPlaneXResolution";
        m_map[EXIFTAG_FOCALPLANEYRESOLUTION] = "exif_FocalPlaneYResolution";
        m_map[EXIFTAG_FOCALPLANERESOLUTIONUNIT] = "exif_FocalPlaneResolutionUnit";  // Symbolic?
        m_map[EXIFTAG_SUBJECTLOCATION] = "exif_SubjectLocation";
        m_map[EXIFTAG_EXPOSUREINDEX] = "exif_ExposureIndex";
        m_map[EXIFTAG_SENSINGMETHOD] = "exif_SensingMethod";
        m_map[EXIFTAG_FILESOURCE] = "exif_FileSource";
        m_map[EXIFTAG_SCENETYPE] = "exif_SceneType";
        m_map[EXIFTAG_CFAPATTERN] = "exif_CFAPattern";
        m_map[EXIFTAG_CUSTOMRENDERED] = "exif_CustomRendered";
        m_map[EXIFTAG_EXPOSUREMODE] = "exif_ExposureMode";
        m_map[EXIFTAG_WHITEBALANCE] = "exif_WhiteBalance";
        m_map[EXIFTAG_DIGITALZOOMRATIO] = "exif_DigitalZoomRatio";
        m_map[EXIFTAG_FOCALLENGTHIN35MMFILM] = "exif_FocalLengthIn35mmFilm";
        m_map[EXIFTAG_SCENECAPTURETYPE] = "exif_SceneCaptureType";
        m_map[EXIFTAG_GAINCONTROL] = "exif_GainControl";
        m_map[EXIFTAG_CONTRAST] = "exif_Contrast";
        m_map[EXIFTAG_SATURATION] = "exif_Saturation";
        m_map[EXIFTAG_SHARPNESS] = "exif_Sharpness";
        m_map[EXIFTAG_DEVICESETTINGDESCRIPTION] = "exif_DeviceSettingDescription";
        m_map[EXIFTAG_SUBJECTDISTANCERANGE] = "exif_SubjectDistanceRange";
        m_map[EXIFTAG_IMAGEUNIQUEID] = "exif_ImageUniqueID";
    }
};

static TagMap tag_to_name;



static void
process_tag (TIFFDirEntry *dirp, const char *buf, bool swab)
{
    TIFFDirEntry dir = *dirp;
    if (swab) {
        swap_endian (&dir.tdir_tag);
        swap_endian (&dir.tdir_type);
        swap_endian (&dir.tdir_count);
        swap_endian (&dir.tdir_offset);
    }
    switch (dir.tdir_tag) {
    case TIFFTAG_EXIFIFD : {
        // Special case: It's a pointer to a private EXIF directory.
        // Handle the whole thing recursively.
        unsigned int offset = dirp->tdir_offset;  // int stored in offset itself
        if (swab)
            swap_endian (&offset);
        unsigned char *ifd = ((unsigned char *)buf + offset);
        unsigned short ndirs = *(unsigned short *)ifd;
        if (swab)
            swap_endian (&ndirs);
        std::cerr << "EXIF Number of directory entries = " << ndirs << "\n";
        for (int d = 0;  d < ndirs;  ++d)
            process_tag ((TIFFDirEntry *) (ifd+2+d*sizeof(TIFFDirEntry)),
                         (char *)buf, swab);
        return;
        }
    default: 
        // Everything else -- use our table to handle the general case
        {
            const char *name = tag_to_name[dir.tdir_tag];
            if (name) {
                add_tag (name, &dir, buf, swab);
                return;
            }
        }
    }
    std::cerr << "Dir : tag=" << dir.tdir_tag
              << ", type=" << dir.tdir_type
              << ", count=" << dir.tdir_count
              << ", offset=" << dir.tdir_offset << "\n";
}



// Read next byte from jpeg strem.
// Borrowed from source code of libjpeg's "djpeg", which I used as an example
// of how to read the APPx markers.
static unsigned int
jpeg_getc (j_decompress_ptr cinfo)
{
    struct jpeg_source_mgr * datasrc = cinfo->src;

    if (datasrc->bytes_in_buffer == 0) {
        if (! (*datasrc->fill_input_buffer) (cinfo)) {
            // ERREXIT (cinfo, JERR_CANT_SUSPEND);
            // FIXME - error handling
            return 0;
        }
    }
    datasrc->bytes_in_buffer--;
    return GETJOCTET(*datasrc->next_input_byte++);
}



// Borrowed from source code of libjpeg's "djpeg", which I used as an example
// of how to read the APPx markers.
static int
my_marker_handler (j_decompress_ptr cinfo)
{
    std::cerr << "my_marker\n";

    int length = jpeg_getc(cinfo) << 8;
    length += jpeg_getc(cinfo);
    length -= 2;			/* discount the length word itself */

    // FIXME -- handle comments
#if 0
    if (cinfo->unread_marker == JPEG_COM)
        fprintf(stderr, "Comment, length %ld:\n", (long) length);
    else			/* assume it is an APPn otherwise */
        fprintf(stderr, "APP%d, length %ld:\n",
                cinfo->unread_marker - JPEG_APP0, (long) length);
#endif

    boost::scoped_array<unsigned char> blob (new unsigned char [length+1]);
    for (int i = 0;  i < length;  ++i)
        blob[i] = jpeg_getc (cinfo);
    blob[length] = 0;  // Just in case it's a string that didn't terminate

    if (cinfo->unread_marker == (JPEG_APP0+1)) {
        unsigned char *buf = &blob[0];
        if (strncmp ((char *)buf, "Exif", 5)) {
            std::cerr << "JPEG: saw APP1, but didn't start 'Exif'\n";
            return 1;
        }
        buf += 6;
        TIFFHeader *head = (TIFFHeader *)buf;
        if (head->tiff_magic != 0x4949 && head->tiff_magic != 0x4d4d) {
            std::cerr << "JPEG: saw Exif, didn't see TIFF magic\n";
            return 1;
        }
        bool host_little = littleendian();
        bool file_little = (head->tiff_magic == 0x4949);
        bool swab = (host_little != file_little);
        if (swab)
            swap_endian (&head->tiff_diroff);
        // std::cerr << "TIFF directory offset = " << head->tiff_diroff << "\n";
        unsigned char *ifd = (buf + head->tiff_diroff);
        unsigned short ndirs = *(unsigned short *)ifd;
        if (swab)
            swap_endian (&ndirs);
        // std::cerr << "Number of directory entries = " << ndirs << "\n";
        for (int d = 0;  d < ndirs;  ++d)
            process_tag ((TIFFDirEntry *) (ifd+2+d*sizeof(TIFFDirEntry)),
                         (char *)buf, swab);
    }

    return 1;
}



bool
JpgInput::open (const char *name, ImageIOFormatSpec &newspec)
{
    // Check that file exists and can be opened
    fd = fopen (name, "rb");
    if (fd == NULL)
        return false;

    // Check magic number to assure this is a JPEG file
    int magic = 0;
    fread (&magic, 4, 1, fd);
    rewind (fd);
    const int JPEG_MAGIC = 0xffd8ffe0, JPEG_MAGIC_OTHER_ENDIAN =  0xe0ffd8ff;
    const int JPEG_MAGIC2 = 0xffd8ffe1, JPEG_MAGIC2_OTHER_ENDIAN =  0xe1ffd8ff;
    if (magic != JPEG_MAGIC && magic != JPEG_MAGIC_OTHER_ENDIAN &&
        magic != JPEG_MAGIC2 && magic != JPEG_MAGIC2_OTHER_ENDIAN) {
        fclose (fd);
        return false;
    }

    m_spec = ImageIOFormatSpec();

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress (&cinfo);            // initialize decompressor
    jpeg_stdio_src (&cinfo, fd);                // specify the data source

    // EXIF and other special tags need to be extracted by our custom
    // marker handler.  Except, duh, there's no blind pointer or other
    // way for the marker handler to know which ImageIO it's associated
    // with.  So we lock this section to make it thread-safe.
    marker_mutex.lock ();
    assert (marker_spec == NULL);
    marker_spec = &m_spec;
    jpeg_set_marker_processor (&cinfo, JPEG_APP0+1, my_marker_handler);
    jpeg_set_marker_processor (&cinfo, JPEG_COM, my_marker_handler);

    jpeg_read_header (&cinfo, FALSE);           // read the file parameters

    jpeg_set_marker_processor (&cinfo, JPEG_APP0+1, NULL);
    jpeg_set_marker_processor (&cinfo, JPEG_COM, NULL);
    marker_spec = NULL;
    marker_mutex.unlock ();
    // End critical section for the marker processing

    jpeg_start_decompress (&cinfo);             // start working
    first_scanline = true;                      // start decompressor

    m_spec.x = 0;
    m_spec.y = 0;
    m_spec.z = 0;
    m_spec.width = cinfo.output_width;
    m_spec.height = cinfo.output_height;
    m_spec.nchannels = cinfo.output_components;
    m_spec.depth = 1;
    m_spec.full_width = m_spec.width;
    m_spec.full_height = m_spec.height;
    m_spec.full_depth = m_spec.depth;
    m_spec.set_format (PT_UINT8);
    m_spec.tile_width = 0;
    m_spec.tile_height = 0;
    m_spec.tile_depth = 0;

    m_spec.channelnames.clear();
    switch (m_spec.nchannels) {
    case 1:
        m_spec.channelnames.push_back("A");
        break;
    case 3:
        m_spec.channelnames.push_back("R");
        m_spec.channelnames.push_back("G");
        m_spec.channelnames.push_back("B");
        break;
    case 4:
        m_spec.channelnames.push_back("R");
        m_spec.channelnames.push_back("G");
        m_spec.channelnames.push_back("B");
        m_spec.channelnames.push_back("A");
        break;
    default:
        fclose (fd);
        return false;
    }

    newspec = m_spec;
    return true;
}



bool
JpgInput::read_native_scanline (int y, int z, void *data)
{
    first_scanline = false;
    assert (y == (int)cinfo.output_scanline);
    assert (y < (int)cinfo.output_height);
    jpeg_read_scanlines (&cinfo, (JSAMPLE **)&data, 1); // read one scanline
    return true;
}



bool
JpgInput::close ()
{
    if (fd != NULL) {
        if (!first_scanline)
            jpeg_finish_decompress (&cinfo);
        jpeg_destroy_decompress(&cinfo);
        fclose (fd);
    }
    init ();   // Reset to initial state
    return true;
}

