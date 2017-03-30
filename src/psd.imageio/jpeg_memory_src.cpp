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

#include <cstdio>

#include <OpenImageIO/imageio.h>

#include "jpeg_memory_src.h"
#include "jerror.h"

namespace {

void
init_memory_source (j_decompress_ptr cinfo)
{
}



void
term_memory_source (j_decompress_ptr cinfo)
{
}



void
skip_input (j_decompress_ptr cinfo, long num_bytes)
{
    struct jpeg_source_mgr *src = cinfo->src;

    if (!num_bytes)
        return;

    while (num_bytes > (long)src->bytes_in_buffer) {
      num_bytes -= (long)src->bytes_in_buffer;
      (*src->fill_input_buffer)(cinfo);
    }
    src->next_input_byte += (size_t)num_bytes;
    src->bytes_in_buffer -= (size_t)num_bytes;
}



boolean
fill_input (j_decompress_ptr cinfo)
{
    static JOCTET mybuffer[4];

    WARNMS(cinfo, JWRN_JPEG_EOF);
    /* Insert a fake EOI marker */
    mybuffer[0] = (JOCTET) 0xFF;
    mybuffer[1] = (JOCTET) JPEG_EOI;

    cinfo->src->next_input_byte = mybuffer;
    cinfo->src->bytes_in_buffer = 2;

    return TRUE;
}

} // anon namespace



OIIO_PLUGIN_NAMESPACE_BEGIN
namespace psd_pvt {


void
jpeg_memory_src (j_decompress_ptr cinfo,
                 unsigned char *inbuffer, unsigned long insize)
{
    struct jpeg_source_mgr * src;

    if (inbuffer == NULL || insize == 0)
        ERREXIT(cinfo, JERR_INPUT_EMPTY);

    if (cinfo->src == NULL) {
        cinfo->src = (struct jpeg_source_mgr *)
          (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
          (size_t)sizeof(struct jpeg_source_mgr));
    }

    src = cinfo->src;
    src->init_source = init_memory_source;
    src->fill_input_buffer = fill_input;
    src->skip_input_data = skip_input;
    src->resync_to_restart = jpeg_resync_to_restart;
    src->term_source = term_memory_source;
    src->bytes_in_buffer = (size_t) insize;
    src->next_input_byte = (JOCTET *) inbuffer;
}


} // namespace psd_pvt
OIIO_PLUGIN_NAMESPACE_END
