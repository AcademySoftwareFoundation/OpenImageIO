/*
  Copyright 2016 Larry Gritz and the other authors and contributors.
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

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/fmath.h>

#include <iostream>
#include <memory>
#include <set>

#define HAVE_CONFIG_H  /* Sometimes DCMTK seems to need this */
#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmimage/diregist.h>
#include <dcmtk/dcmimage/dicopx.h>
#include <dcmtk/dcmimgle/dcmimage.h>


// This plugin utilises DCMTK:
//   http://dicom.offis.de/
//   http://support.dcmtk.org/docs/index.html
//
// General information about DICOM:
//   http://dicom.nema.org/standard.html
//
// Sources of sample images:
//   http://www.osirix-viewer.com/resources/dicom-image-library/
//   http://barre.nom.fr/medical/samples/



OIIO_PLUGIN_NAMESPACE_BEGIN

class DICOMInput final : public ImageInput {
public:
    DICOMInput () {}
    virtual ~DICOMInput() { close(); }
    virtual const char * format_name (void) const { return "dicom"; }
    virtual int supports (string_view feature) const { return false; }
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool open (const std::string &name, ImageSpec &newspec,
                       const ImageSpec &config);
    virtual bool close();
    virtual bool seek_subimage (int subimage, int miplevel, ImageSpec &newspec);
    virtual bool read_native_scanline (int y, int z, void *data);

private:
    std::unique_ptr<DicomImage> m_img;
    int m_framecount, m_firstframe;
    int m_bitspersample;
    std::string m_filename;
    int m_subimage = -1;
    const DiPixel *m_dipixel = nullptr;
    const char *m_internal_data = nullptr;

    void read_metadata ();
};



// Export version number and create function symbols
OIIO_PLUGIN_EXPORTS_BEGIN

    OIIO_EXPORT int dicom_imageio_version = OIIO_PLUGIN_VERSION;
    OIIO_EXPORT const char* dicom_imageio_library_version () {
        return PACKAGE_NAME " " PACKAGE_VERSION;
    }
    OIIO_EXPORT ImageInput *dicom_input_imageio_create () {
        return new DICOMInput;
    }
    OIIO_EXPORT const char *dicom_input_extensions[] = {
        "dcm", NULL
    };

OIIO_PLUGIN_EXPORTS_END



bool
DICOMInput::open (const std::string &name, ImageSpec &newspec)
{
    // If user doesn't want to provide any config, just use an empty spec.
    ImageSpec config;
    return open(name, newspec, config);
}



bool
DICOMInput::open (const std::string &name, ImageSpec &newspec,
                  const ImageSpec &config)
{
    m_filename = name;
    m_subimage = -1;
    m_img.reset ();
    return seek_subimage (0, 0, newspec);
}



bool
DICOMInput::close()
{
    m_img.reset();
    m_subimage = -1;
    m_dipixel = nullptr;
    m_internal_data = nullptr;
    return true;
}



// Names of tags to ignore
static std::set<std::string> ignore_tags {
    "Rows", "Columns", "PixelAspectRatio", "BitsAllocated",
    "BitsStored", "HighBit", "PixelRepresentation", "PixelData",
    "NumberOfFrames", "SamplesPerPixel"
};



bool
DICOMInput::seek_subimage (int subimage, int miplevel, ImageSpec &newspec)
{
    if (miplevel != 0)
        return false;

    if (subimage == m_subimage) {
        newspec = m_spec;
        return true;    // already there
    }

    if (subimage < m_subimage) {
        // Want an earlier subimage, Easier to close and start again
        close ();
        m_subimage = -1;
    }

    // Open if it's not already opened
    if (! m_img || m_subimage < 0) {
        OFLog::configure (OFLogger::FATAL_LOG_LEVEL);
        m_img.reset (new DicomImage(m_filename.c_str(),
                                    CIF_UsePartialAccessToPixelData,
                                    0 /*first frame*/, 1 /* fcount */));
        m_subimage = 0;
        if (m_img->getStatus() != EIS_Normal) {
            m_img.reset();
            error ("Unable to open DICOM file %s", m_filename);
            return false;
        }
        m_framecount = m_img->getFrameCount();
        m_firstframe = m_img->getFirstFrame();
    }

    if (subimage >= m_firstframe + m_framecount) {
        error ("Unable to seek to subimage %d", subimage);
        return false;
    }

    // Advance to the desired subimage
    while (m_subimage < subimage) {
        m_img->processNextFrames (1);
        if (m_img->getStatus() != EIS_Normal) {
            m_img.reset();
            error ("Unable to seek to subimage %d", subimage);
            return false;
        }
        ++m_subimage;
    }

    m_dipixel = m_img->getInterData ();
    m_internal_data = (const char *) m_dipixel->getData ();
    EP_Representation rep = m_dipixel->getRepresentation ();
    TypeDesc format;
    switch (rep) {
        case EPR_Uint8  : format = TypeDesc::UINT8;  break;
        case EPR_Sint8  : format = TypeDesc::INT8;   break;
        case EPR_Uint16 : format = TypeDesc::UINT16; break;
        case EPR_Sint16 : format = TypeDesc::INT16;  break;
        case EPR_Uint32 : format = TypeDesc::UINT32; break;
        case EPR_Sint32 : format = TypeDesc::INT32;  break;
        default: break;
    }
    m_internal_data = (const char *) m_img->getOutputData (0, m_subimage, 0);

    EP_Interpretation photo = m_img->getPhotometricInterpretation();
    struct PhotoTable { EP_Interpretation pi; const char *name; int chans; };
    static PhotoTable phototable[] = {
        { EPI_Unknown, "Unknown", 1 }, /// unknown, undefined, invalid
        { EPI_Missing, "Missing", 1 }, /// no element value available
        { EPI_Monochrome1, "Monochrome1", 1 }, /// monochrome 1
        { EPI_Monochrome2, "Monochrome2", 1 }, /// monochrome 2
        { EPI_PaletteColor, "PaletteColor", 3 }, /// palette color
        { EPI_RGB, "RGB", 3 }, /// RGB color
        { EPI_HSV, "HSV", 3 }, /// HSV color (retired)
        { EPI_ARGB, "ARGB", 4 }, /// ARGB color (retired)
        { EPI_CMYK, "CMYK", 4 }, /// CMYK color (retired)
        { EPI_YBR_Full, "YBR_Full", 3 }, /// YCbCr full
        { EPI_YBR_Full_422, "YBR_Full_422", 3 }, /// YCbCr full 4:2:2
        { EPI_YBR_Partial_422, "YBR_Partial_422", 3 }, /// YCbCr partial 4:2:2
        { EPI_Unknown, NULL }
    };
    int nchannels = 1;
    const char *photoname = NULL;
    for (int i = 0; phototable[i].name; ++i) {
        if (photo == phototable[i].pi) {
            nchannels = phototable[i].chans;
            photoname = phototable[i].name;
            break;
        }
    }

    m_spec = ImageSpec(m_img->getWidth(), m_img->getHeight(), nchannels, format);

    m_bitspersample = m_img->getDepth();
    if (size_t(m_bitspersample) != m_spec.format.size()*8)
        m_spec.attribute ("oiio:BitsPerSample", m_bitspersample);

    m_spec.attribute ("PixelAspectRatio", (float)m_img->getWidthHeightRatio());
    if (photoname)
        m_spec.attribute ("dicom:PhotometricInterpretation",photoname);
    if (m_spec.nchannels > 1) {
        m_spec.attribute ("dicom:PlanarConfiguration",
                          (int)((DiColorPixel *)m_dipixel)->getPlanarConfiguration());
    }

    read_metadata ();

    newspec = m_spec;   // Copy the spec to return to the user
    return true;
}



void
DICOMInput::read_metadata ()
{
    // Can't seem to figure out how to get the metadata from the
    // DicomImage class. So open the file a second time (ugh) with
    // DcmFileFormat.
    std::unique_ptr<DcmFileFormat> dcm (new DcmFileFormat);
    OFCondition status = dcm->loadFile (m_filename.c_str() /*, EXS_Unknown,
                                          EGL_noChange, DCM_MaxReadLength,
                                          ERM_metaOnly */);
    if (status.good()) {
        DcmDataset *dataset = dcm->getDataset();
        DcmStack stack;
        while (dcm->nextObject(stack, OFTrue).good()) {
            DcmObject *object = stack.top();
            ASSERT (object);
            DcmTag &tag = const_cast<DcmTag&>(object->getTag());
            std::string tagname = tag.getTagName();
            if (ignore_tags.find(tagname) != ignore_tags.end())
                continue;
            std::string name = Strutil::format ("dicom:%s", tag.getTagName());
            DcmEVR evr = tag.getEVR();
            // VR codes explained:
            // http://dicom.nema.org/Dicom/2013/output/chtml/part05/sect_6.2.html
            if (evr == EVR_FL || evr == EVR_OF || evr == EVR_DS) {
                float val;
                if (dataset->findAndGetFloat32 (tag, val).good())
                    m_spec.attribute (name, val);
            } else if (evr == EVR_FD
#if PACKAGE_VERSION_NUMBER >= 362
                       || evr == EVR_OD
#endif
                       ) {
                double val;
                if (dataset->findAndGetFloat64 (tag, val).good())
                    m_spec.attribute (name, (float)val);
                // N.B. we cast to float. Will anybody care?
            } else if (evr == EVR_SL || evr == EVR_IS) {
                Sint32 val;
                if (dataset->findAndGetSint32 (tag, val).good())
                    m_spec.attribute (name, static_cast<int>(val));
            } else if (evr == EVR_UL) {
                Uint32 val;
                if (dataset->findAndGetUint32 (tag, val).good())
                    m_spec.attribute (name, static_cast<unsigned int>(val));
            } else if (evr == EVR_US) {
                unsigned short val;
                if (dataset->findAndGetUint16 (tag, val).good())
                    m_spec.attribute (name, TypeDesc::INT16, &val);
            } else if (evr == EVR_AS || evr == EVR_CS || evr == EVR_DA ||
                       evr == EVR_DT || evr == EVR_LT || evr == EVR_PN ||
                       evr == EVR_ST || evr == EVR_TM || evr == EVR_UI ||
                       evr == EVR_UT || evr == EVR_LO || evr == EVR_SH
#if PACKAGE_VERSION_NUMBER >= 362
                       || evr == EVR_UC ||evr == EVR_UR
#endif
                       ) {
                OFString val;
                if (dataset->findAndGetOFString (tag, val).good())
                    m_spec.attribute (name, val.c_str());
            } else {
                OFString val;
                if (dataset->findAndGetOFString (tag, val).good())
                    m_spec.attribute (name, val.c_str());
                    // m_spec.attribute (name+"-"+tag.getVRName(), val.c_str());
            }
        }
        // dcm->writeXML (std::cout);
    }
}



bool
DICOMInput::read_native_scanline (int y, int z, void *data)
{
    if (y < 0 || y >= m_spec.height) // out of range scanline
        return false;

    ASSERT (m_internal_data);
    size_t size = m_spec.scanline_bytes();
    memcpy (data, m_internal_data + y*size, size);

    // Handle non-full bit depths
    int bits = m_spec.format.size() * 8;
    if (bits != m_bitspersample) {
        size_t n = m_spec.width * m_spec.nchannels;
        if (m_spec.format == TypeDesc::UINT8) {
            unsigned char *p = (unsigned char *)data;
            for (size_t i = 0; i < n; ++i)
                p[i] = bit_range_convert (p[i], m_bitspersample, bits);
        } else if (m_spec.format == TypeDesc::UINT16) {
            unsigned short *p = (unsigned short *)data;
            for (size_t i = 0; i < n; ++i)
                p[i] = bit_range_convert (p[i], m_bitspersample, bits);
        } else if (m_spec.format == TypeDesc::UINT32) {
            unsigned int *p = (unsigned int *)data;
            for (size_t i = 0; i < n; ++i)
                p[i] = bit_range_convert (p[i], m_bitspersample, bits);
        }
    }

    return true;
}


OIIO_PLUGIN_NAMESPACE_END

