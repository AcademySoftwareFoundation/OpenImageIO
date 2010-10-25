/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
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

#ifndef OPENIMAGEIO_JPEG2000_PVT_H
#define OPENIMAGEIO_JPEG2000_PVT_H

#include "jasper/jasper.h"


// channels id
#define RED 0
#define GREEN 1
#define BLUE 2
#define OPACITY 3
#define GREY 0


// compression(?) used
#define JP2_STREAM "jp2"
#define JPC_STREAM "jpc"


OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace OpenImageIO;


class Jpeg2000Input : public ImageInput {
 public:
    Jpeg2000Input () { init (); }
    virtual ~Jpeg2000Input () { close (); }
    virtual const char *format_name (void) const { return "jpeg2000"; }
    virtual bool open (const std::string &name, ImageSpec &spec);
    virtual bool close (void);
    virtual bool read_native_scanline (int y, int z, void *data);

 private:
    std::string m_filename;
    // pointer to the stream from which we read data; in our case it is
    // always open file
    jas_stream_t *m_stream;
    // structure when we store uncompressed image
    jas_image_t *m_image;
    int m_fam_clrspc;
    // here we store informations about data from one channel
    std::vector <jas_matrix_t*> m_matrix_chan;
    std::vector<int> m_cmpt_id; //ids of the components
    std::vector<char> m_pixels;
    size_t m_scanline_size;

    void init (void);
    // read informations about all channels of the given image
    // stored in private filed 'm_image'
    bool read_channels (void);
};



class Jpeg2000Output : public ImageOutput {
 public:
    Jpeg2000Output () { init (); }
    virtual ~Jpeg2000Output () { close (); }
    virtual const char *format_name (void) const { return "jpeg2000"; }
    virtual bool supports (const std::string &feature) const { return false; }
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode=Create);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);
 private:
    std::string m_filename;
    std::vector<unsigned char> m_scratch;
    std::vector<char> m_pixels;
    jas_image_t *m_image;
    // pointer to the stream to which we save data; in our case it is
    // always open file
    jas_stream_t *m_stream;
    // structure that stores information (not data) about one channel
    jas_image_cmptparm_t *m_components;
    std::vector<jas_matrix_t*> m_scanline;
    size_t m_scanline_size;
    std::string stream_format;

    void init (void);
    // initializing information about component
    void component_struct_init (jas_image_cmptparm_t *cmpt);

};

OIIO_PLUGIN_NAMESPACE_END


#endif // OPENIMAGEIO_JPEG2000_PVT_H
