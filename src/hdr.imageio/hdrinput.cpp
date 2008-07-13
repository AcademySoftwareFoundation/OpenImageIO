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

#include <cassert>
#include <cstdio>
#include <iostream>

#include "imageio.h"
using namespace OpenImageIO;
#include "fmath.h"
#include "rgbe.h"



class HdrInput : public ImageInput {
public:
    HdrInput () { init(); }
    virtual ~HdrInput () { close(); }
    virtual const char * format_name (void) const { return "hdr"; }
    virtual bool open (const char *name, ImageIOFormatSpec &spec);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool close ();
    virtual int current_subimage (void) const { return m_subimage; }
    virtual bool seek_subimage (int index, ImageIOFormatSpec &newspec);

private:
    std::string m_filename;       ///< File name
    FILE *m_fd;                   ///< The open file handle
    int m_subimage;               ///< What subimage are we looking at?
    std::vector<float> m_pixels;  ///< Data buffer

    void init () {
        m_fd = NULL;
        m_subimage = -1;
        m_pixels.clear ();
    }

//    int RGBE_ReadHeader(FILE *fp, int *width, int *height, rgbe_header_info *info);
};



// Export version number and create function symbols
extern "C" {
    DLLEXPORT int imageio_version = IMAGEIO_VERSION;
    DLLEXPORT HdrInput *hdr_input_imageio_create () {
        return new HdrInput;
    }
    DLLEXPORT const char *hdr_input_extensions[] = {
        "hdr", "rgbe", NULL
    };
};



bool
HdrInput::open (const char *name, ImageIOFormatSpec &newspec)
{
    m_filename = name;
    return seek_subimage (0, newspec);
}



#if 0
int
HdrInput::RGBE_ReadHeader (FILE *fp, int *width, int *height, rgbe_header_info *info)
{
  char buf[128];
  int found_format;
  float tempf;
  int i;

  found_format = 0;
  if (info) {
    info->valid = 0;
    info->programtype[0] = 0;
    info->gamma = info->exposure = 1.0;
  }
  if (fgets(buf,sizeof(buf)/sizeof(buf[0]),fp) == NULL)
    return rgbe_error(rgbe_read_error,NULL);
  if ((buf[0] != '#')||(buf[1] != '?')) {
    /* if you want to require the magic token then uncomment the next line */
    /*return rgbe_error(rgbe_format_error,"bad initial token"); */
  }
  else if (info) {
    info->valid |= RGBE_VALID_PROGRAMTYPE;
    for(i=0;i<sizeof(info->programtype)-1;i++) {
      if ((buf[i+2] == 0) || isspace(buf[i+2]))
	break;
      info->programtype[i] = buf[i+2];
    }
    info->programtype[i] = 0;
    if (fgets(buf,sizeof(buf)/sizeof(buf[0]),fp) == 0)
      return rgbe_error(rgbe_read_error,NULL);
  }
  bool found_FORMAT_line = false;
  for(;;) {
    if ((buf[0] == 0)||(buf[0] == '\n')) {
        if (found_FORMAT_line)
            break;
        return rgbe_error(rgbe_format_error,"no FORMAT specifier found");
    }
    else if (strcmp(buf,"FORMAT=32-bit_rle_rgbe\n") == 0) {
        found_FORMAT_line = true;
        //LG says no:    break;       /* format found so break out of loop */
    }
    else if (info && (sscanf(buf,"GAMMA=%g",&tempf) == 1)) {
      info->gamma = tempf;
      info->valid |= RGBE_VALID_GAMMA;
    }
    else if (info && (sscanf(buf,"EXPOSURE=%g",&tempf) == 1)) {
      info->exposure = tempf;
      info->valid |= RGBE_VALID_EXPOSURE;
    }
    if (fgets(buf,sizeof(buf)/sizeof(buf[0]),fp) == 0)
      return rgbe_error(rgbe_read_error,NULL);
  }
//  if (fgets(buf,sizeof(buf)/sizeof(buf[0]),fp) == 0)
//    return rgbe_error(rgbe_read_error,NULL);
  if (strcmp(buf,"\n") != 0) {
      printf ("Found '%s'\n", buf);
    return rgbe_error(rgbe_format_error,
		      "missing blank line after FORMAT specifier");
  }
  if (fgets(buf,sizeof(buf)/sizeof(buf[0]),fp) == 0)
    return rgbe_error(rgbe_read_error,NULL);
  if (sscanf(buf,"-Y %d +X %d",height,width) < 2)
    return rgbe_error(rgbe_format_error,"missing image size specifier");
  return RGBE_RETURN_SUCCESS;
}
#endif



bool
HdrInput::seek_subimage (int index, ImageIOFormatSpec &newspec)
{
    if (index != 0)
        return false;

    close();

    // Check that file exists and can be opened
    m_fd = fopen (m_filename.c_str(), "rb");
    if (m_fd == NULL)
        return false;

    m_spec = ImageIOFormatSpec();

    int r = RGBE_ReadHeader (m_fd, &m_spec.width, &m_spec.height, NULL);
    if (r != RGBE_RETURN_SUCCESS) {
        close ();
        return false;
    }

    m_spec.nchannels = 3;           // HDR files are always 3 channel
    m_spec.full_width = m_spec.width;
    m_spec.full_height = m_spec.height;
    m_spec.full_depth = m_spec.depth;
    m_spec.set_format (PT_FLOAT);   // HDR files are always float
    m_spec.channelnames.push_back ("R");
    m_spec.channelnames.push_back ("G");
    m_spec.channelnames.push_back ("B");

    m_subimage = index;
    newspec = m_spec;
    return true;
}



bool
HdrInput::read_native_scanline (int y, int z, void *data)
{
    if (! m_pixels.size()) {    // We haven't read the pixels yet
        m_pixels.resize (m_spec.width * m_spec.height * m_spec.nchannels);
        RGBE_ReadPixels_RLE (m_fd, &m_pixels[0], m_spec.width, m_spec.height);
    }
    memcpy (data, &m_pixels[y * m_spec.width * m_spec.nchannels],
            m_spec.scanline_bytes());
    return true;
}



bool
HdrInput::close ()
{
    if (m_fd)
        fclose (m_fd);
    init ();   // Reset to initial state
    return true;
}

