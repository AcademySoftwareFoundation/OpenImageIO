/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// (this is the MIT license)
/////////////////////////////////////////////////////////////////////////////


#include <cstdio>
#include <cstdlib>
#include <cmath>

#include <tiffio.h>

#include <ImfTestFile.h>
#include <ImfInputFile.h>
#include <ImfTiledInputFile.h>
#include <ImfChannelList.h>
#include <ImfEnvmap.h>
#include <ImfIntAttribute.h>
#include <ImfFloatAttribute.h>
#include <ImfMatrixAttribute.h>
#include <ImfStringAttribute.h>
#include <ImfEnvmapAttribute.h>
//using namespace Imf;

#include "dassert.h"
#include "paramtype.h"
#include "imageio.h"
#include "thread.h"
#include "strutil.h"

using namespace OpenImageIO;



class OpenEXRInput : public ImageInput {
public:
    OpenEXRInput () { init(); }
    virtual ~OpenEXRInput () { close(); }
    virtual const char * format_name (void) const { return "OpenEXR"; }
    virtual bool open (const char *name, ImageIOFormatSpec &newspec);
    virtual bool close ();
    virtual int current_subimage (void) const { return m_subimage; }
    virtual bool seek_subimage (int index, ImageIOFormatSpec &newspec);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool read_native_tile (int x, int y, int z, void *data);

private:
    const Imf::Header *m_exr_header;  ///< Ptr to image header
    Imf::InputFile *m_input_scanline; ///< Input for scanline files
    Imf::TiledInputFile *m_input_tiled; ///< Input for tiled files
    std::string m_filename;          ///< Stash the filename
    int m_levelmode;                 ///< The level mode of the file
    int m_roundingmode;              ///< Rounding mode of the file
    int m_subimage;                  ///< What subimage are we looking at?
    int m_nsubimages;                ///< How many subimages are there?
    int m_topwidth;                  ///< Width of top mip level
    int m_topheight;                 ///< Height of top mip level
    bool m_cubeface;                 ///< It's a cubeface environment map
    std::vector<unsigned char> m_scratch; ///< Scratch space for us to use

    void init () {
        m_exr_header = NULL;
        m_input_scanline = NULL;
        m_input_tiled = NULL;
        m_subimage = -1;
    }

    // Get a string tiff tag field and put it into extra_params
    void get_string_field (const std::string &name, int tag) {
#if 0
        char *s = NULL;
        TIFFGetField (m_tif, tag, &s);
        if (s && *s)
            spec.add_parameter (name, PT_STRING, 1, &s);
#endif
    }

    // Get a float-oid tiff tag field and put it into extra_params
    void get_float_field (const std::string &name, int tag,
                          ParamBaseType type=PT_FLOAT) {
#if 0
        float f[16];
        if (TIFFGetField (m_tif, tag, f))
            spec.add_parameter (name, type, 1, &f);
#endif
    }

    // Get an int tiff tag field and put it into extra_params
    void get_int_field (const std::string &name, int tag) {
#if 0
        int i;
        if (TIFFGetField (m_tif, tag, &i))
            spec.add_parameter (name, PT_INT, 1, &i);
#endif
    }

    // Get an int tiff tag field and put it into extra_params
    void get_short_field (const std::string &name, int tag) {
#if 0
        unsigned short s;
        if (TIFFGetField (m_tif, tag, &s)) {
            int i = s;
            spec.add_parameter (name, PT_INT, 1, &i);
        }
#endif
    }
};



// Obligatory material to make this a recognizeable imageio plugin:
extern "C" {

DLLEXPORT OpenEXRInput *
openexr_input_imageio_create ()
{
    return new OpenEXRInput;
}

// DLLEXPORT int imageio_version = IMAGEIO_VERSION; // it's in tiffoutput.cpp

DLLEXPORT const char * openexr_input_extensions[] = {
    "exr", NULL
};

};



bool
OpenEXRInput::open (const char *name, ImageIOFormatSpec &newspec)
{
    // Quick check to reject non-exr files
    bool tiled;
    if (! Imf::isOpenExrFile (name, tiled))
        return false;

    spec = ImageIOFormatSpec();  // Clear everything with default constructor
    try {
        if (tiled) {
            m_input_tiled = new Imf::TiledInputFile (name);
            m_exr_header = &(m_input_tiled->header());
        } else {
            m_input_scanline = new Imf::InputFile (name);
            m_exr_header = &(m_input_scanline->header());
        }
    }
    catch (const std::exception &e) {
        error ("OpenEXR exception: %s", e.what());
        return false;
    }
    if (! m_input_scanline && ! m_input_tiled) {
        error ("Unknown error opening EXR file");
        return false;
    }

    Imath::Box2i datawindow = m_exr_header->dataWindow();
    spec.x = datawindow.min.x;
    spec.y = datawindow.min.y;
    spec.z = 0;
    spec.width = datawindow.max.x - datawindow.min.x + 1;
    spec.height = datawindow.max.y - datawindow.min.y + 1;
    spec.depth = 1;
    m_topwidth = spec.width;      // Save top-level mipmap dimensions
    m_topheight = spec.height;
    spec.full_width = spec.width;
    spec.full_height = spec.height;
    spec.full_depth = spec.depth;
    if (tiled) {
        spec.tile_width = m_input_tiled->tileXSize();
        spec.tile_height = m_input_tiled->tileYSize();
    } else {
        spec.tile_width = 0;
        spec.tile_height = 0;
    }
    spec.tile_depth = 0;
    spec.format = PT_HALF;  // FIXME: do the right thing for non-half
    spec.nchannels = 0;
    const Imf::ChannelList &channels (m_exr_header->channels());
    Imf::ChannelList::ConstIterator ci;
    int c;
    for (c = 0, ci = channels.begin();  ci != channels.end();  ++c, ++ci) {
        // const Imf::Channel &channel = ci.channel();
        std::cerr << "Channel " << ci.name() << '\n';
        spec.channelnames.push_back (ci.name());
        if (ci.name() == "Z")
            spec.z_channel = c;
        if (ci.name() == "A" || ci.name() == "Alpha")
            spec.alpha_channel = c;
        ++spec.nchannels;
    }
    // FIXME: should we also figure out the layers?
    
    if (tiled) {
        // FIXME: levelmode
        m_levelmode = m_input_tiled->levelMode();
        m_roundingmode = m_input_tiled->levelRoundingMode();
        if (m_levelmode == Imf::MIPMAP_LEVELS)
            m_nsubimages = m_input_tiled->numLevels();
        else if (m_levelmode == Imf::RIPMAP_LEVELS)
            m_nsubimages = std::max (m_input_tiled->numXLevels(),
                                     m_input_tiled->numYLevels());
        else
            m_nsubimages = 1;
    } else {
        m_levelmode = Imf::ONE_LEVEL;
        m_nsubimages = 1;
    }

    const Imf::EnvmapAttribute *envmap;
    envmap = m_exr_header->findTypedAttribute<Imf::EnvmapAttribute>("envmap");
    if (envmap) {
        m_cubeface = (envmap->value() == Imf::ENVMAP_CUBE);
        spec.add_parameter ("textureformat", m_cubeface ? "CubeFace Environment" : "LatLong Environment");
        // FIXME - detect CubeFace Shadow
        spec.add_parameter ("up", "y");  // OpenEXR convention
    } else {
        m_cubeface = false;
        if (tiled)
            spec.add_parameter ("textureformat", "Plain Texture");
        // FIXME - detect Shadow
    }

    m_subimage = 0;
    newspec = spec;
    return true;
}



bool
OpenEXRInput::seek_subimage (int index, ImageIOFormatSpec &newspec)
{
    if (index < 0 || index >= m_nsubimages)   // out of range
        return false;

    m_subimage = index;

    if (index == 0 && m_levelmode == Imf::ONE_LEVEL) {
        newspec = spec;
        return true;
    }

    // Compute the resolution of the requested subimage.
    int w = m_topwidth, h = m_topheight;
    if (m_levelmode == Imf::MIPMAP_LEVELS) {
        while (index--) {
            if (w > 1) {
                if ((w & 1) && m_roundingmode == Imf::ROUND_UP)
                    w = w/2 + 1;
                else w /= 2;
            }
            if (h > 1) {
                if ((h & 1) && m_roundingmode == Imf::ROUND_UP)
                    h = h/2 + 1;
                else h /= 2;
            }
        }
    } else if (m_levelmode == Imf::RIPMAP_LEVELS) {
        // FIXME
    } else {
        ASSERT(0);
    }

    spec.width = w;
    spec.height = h;
    spec.full_width = w;
    spec.full_height = m_cubeface ? w : h;
    newspec = spec;

    return true;
}



bool
OpenEXRInput::close ()
{
    delete m_input_scanline;
    delete m_input_tiled;
    m_subimage = -1;
    init ();  // Reset to initial state
    return true;
}



bool
OpenEXRInput::read_native_scanline (int y, int z, void *data)
{
    ASSERT (m_input_scanline != NULL);

    // Compute where OpenEXR needs to think the full buffers starts.
    // OpenImageIO requires that 'data' points to where the client wants
    // to put the pixels being read, but OpenEXR's frameBuffer.insert()
    // wants where the address of the "virtual framebuffer" for the
    // whole image.
    char *buf = (char *)data
              - spec.x * spec.pixel_bytes() 
              - (y + spec.y) * spec.scanline_bytes();

    try {
        Imf::FrameBuffer frameBuffer;
        for (int c = 0;  c < spec.nchannels;  ++c) {
            frameBuffer.insert (spec.channelnames[c].c_str(),
                                Imf::Slice (Imf::HALF,  // FIXME
                                            buf + c * spec.channel_bytes(),
                                            spec.pixel_bytes(),
                                            spec.scanline_bytes()));
            // FIXME - what if all channels aren't the same data type?
        }
        m_input_scanline->setFrameBuffer (frameBuffer);
        y -= spec.y;
        m_input_scanline->readPixels (y, y);
    }
    catch (const std::exception &e) {
        error ("Filed OpenEXR read: %s", e.what());
        return false;
    }
    return true;
}



bool
OpenEXRInput::read_native_tile (int x, int y, int z, void *data)
{
    ASSERT (m_input_tiled != NULL);

    // Compute where OpenEXR needs to think the full buffers starts.
    // OpenImageIO requires that 'data' points to where the client wants
    // to put the pixels being read, but OpenEXR's frameBuffer.insert()
    // wants where the address of the "virtual framebuffer" for the
    // whole image.
    char *buf = (char *)data
              - spec.x * spec.pixel_bytes() 
              - (y + spec.y) * spec.scanline_bytes();

    try {
        Imf::FrameBuffer frameBuffer;
        for (int c = 0;  c < spec.nchannels;  ++c) {
            frameBuffer.insert (spec.channelnames[c].c_str(),
                                Imf::Slice (Imf::HALF,  // FIXME
                                            buf + c * spec.channel_bytes(),
                                            spec.pixel_bytes(),
                                            spec.scanline_bytes()));
            // FIXME - what if all channels aren't the same data type?
        }
        m_input_tiled->setFrameBuffer (frameBuffer);
        x -= spec.x;
        y -= spec.y;
        m_input_tiled->readTile (x/spec.tile_width, y/spec.tile_height,
                                 m_subimage, m_subimage);
    }
    catch (const std::exception &e) {
        error ("Filed OpenEXR read: %s", e.what());
        return false;
    }
    return true;
}
