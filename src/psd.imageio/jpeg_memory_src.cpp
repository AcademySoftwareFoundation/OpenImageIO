// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#include <cstdio>

#include <OpenImageIO/imageio.h>

#include "jerror.h"
#include "jpeg_memory_src.h"

namespace {

void
init_memory_source(j_decompress_ptr cinfo)
{
}



void
term_memory_source(j_decompress_ptr cinfo)
{
}



void
skip_input(j_decompress_ptr cinfo, long num_bytes)
{
    struct jpeg_source_mgr* src = cinfo->src;

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
fill_input(j_decompress_ptr cinfo)
{
    static JOCTET mybuffer[4];

    WARNMS(cinfo, JWRN_JPEG_EOF);
    /* Insert a fake EOI marker */
    mybuffer[0] = (JOCTET)0xFF;
    mybuffer[1] = (JOCTET)JPEG_EOI;

    cinfo->src->next_input_byte = mybuffer;
    cinfo->src->bytes_in_buffer = 2;

    return TRUE;
}

}  // namespace



OIIO_PLUGIN_NAMESPACE_BEGIN
namespace psd_pvt {


void
jpeg_memory_src(j_decompress_ptr cinfo, unsigned char* inbuffer,
                unsigned long insize)
{
    struct jpeg_source_mgr* src;

    if (inbuffer == NULL || insize == 0)
        ERREXIT(cinfo, JERR_INPUT_EMPTY);

    if (cinfo->src == NULL) {
        cinfo->src = (struct jpeg_source_mgr*)(*cinfo->mem->alloc_small)(
            (j_common_ptr)cinfo, JPOOL_PERMANENT,
            (size_t)sizeof(struct jpeg_source_mgr));
    }

    src                    = cinfo->src;
    src->init_source       = init_memory_source;
    src->fill_input_buffer = fill_input;
    src->skip_input_data   = skip_input;
    src->resync_to_restart = jpeg_resync_to_restart;
    src->term_source       = term_memory_source;
    src->bytes_in_buffer   = (size_t)insize;
    src->next_input_byte   = (JOCTET*)inbuffer;
}


}  // namespace psd_pvt
OIIO_PLUGIN_NAMESPACE_END
