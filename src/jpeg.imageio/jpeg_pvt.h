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


/////////////////////////////////////////////////////////////////////////////
// Private definitions internal to the jpeg.imageio plugin
/////////////////////////////////////////////////////////////////////////////


#ifndef OPENIMAGEIO_JPEG_PVT_H
#define OPENIMAGEIO_JPEG_PVT_H

#include <csetjmp>

extern "C" {
#include "jpeglib.h"
}


OIIO_PLUGIN_NAMESPACE_BEGIN

class JpgInput : public ImageInput {
 public:
    JpgInput () { init(); }
    virtual ~JpgInput () { close(); }
    virtual const char * format_name (void) const { return "jpeg"; }
    virtual bool valid_file (const std::string &filename) const;
    virtual bool open (const std::string &name, ImageSpec &spec);
    virtual bool open (const std::string &name, ImageSpec &spec,
                       const ImageSpec &config);
    virtual bool seek_subimage (int index, int miplevel, ImageSpec &newspec) {
        return (index == 0 && miplevel == 0);   // JPEG has only one subimage
    }
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool close ();
    const std::string &filename () const { return m_filename; }
    void * coeffs () const { return m_coeffs; }

    struct my_error_mgr {
        struct jpeg_error_mgr pub;    /* "public" fields */
        jmp_buf setjmp_buffer;        /* for return to caller */
        JpgInput *jpginput;           /* back pointer to *this */
    };
    typedef struct my_error_mgr * my_error_ptr;

    // Called by my_error_exit
    void jpegerror (my_error_ptr myerr, bool fatal=false);

 private:
    FILE *m_fd;
    std::string m_filename;
    int m_next_scanline;      // Which scanline is the next to read?
    bool m_raw;               // Read raw coefficients, not scanlines
    bool m_fatalerr;          // JPEG reader hit a fatal error
    struct jpeg_decompress_struct m_cinfo;
    my_error_mgr m_jerr;
    jvirt_barray_ptr *m_coeffs;

    void init () {
        m_fd = NULL;
        m_raw = false;
        m_fatalerr = false;
        m_coeffs = NULL;
        m_jerr.jpginput = this;
    }

    // Rummage through the JPEG "APP1" marker pointed to by buf, decoding
    // IPTC (International Press Telecommunications Council) metadata
    // information and adding attributes to spec.  This assumes it's in
    // the form of an IIM (Information Interchange Model), which is actually
    // considered obsolete and is replaced by an XML scheme called XMP.
    void jpeg_decode_iptc (const unsigned char *buf);

    void close_file () {
        if (m_fd)
            fclose (m_fd);   // N.B. the init() will set m_fd to NULL
        init ();
    }

    friend class JpgOutput;
};



OIIO_PLUGIN_NAMESPACE_END


#endif /* OPENIMAGEIO_JPEG_PVT_H */

