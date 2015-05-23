/*
  Copyright 2011 Larry Gritz and the other authors and contributors.
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

#ifndef OPENIMAGEIO_PSD_JPEG_MEMORY_SRC_H
#define OPENIMAGEIO_PSD_JPEG_MEMORY_SRC_H

#ifdef WIN32
//#undef FAR
#define XMD_H
#endif

extern "C" {
#include "jpeglib.h"
}

OIIO_PLUGIN_NAMESPACE_BEGIN
namespace psd_pvt {

// This function allows you to read a JPEG from memory using libjpeg.
// Newer versions of libjpeg have jpeg_mem_src which has the same functionality.
// inbuffer is the buffer that holds the JPEG data
// insize is the size of the buffer
void jpeg_memory_src (j_decompress_ptr cinfo,
                      unsigned char *inbuffer, unsigned long insize);

}  // end namespace psd_pvt
OIIO_PLUGIN_NAMESPACE_END

#endif /* OPENIMAGEIO_PSD_JPEG_MEMORY_SRC_H */
