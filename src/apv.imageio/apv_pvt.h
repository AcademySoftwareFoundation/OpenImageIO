// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Shared helpers for the APV (OpenAPV) reader and writer.

#pragma once

#include <oapv/oapv.h>

#include <OpenImageIO/imageio.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

namespace apv_pvt {

// Raw APV bitstream files are a sequence of [4-byte big-endian AU size]
// [access unit]. Each AU begins with the 4-byte signature "aPv1".
inline constexpr unsigned char apv_signature[4] = { 0x61, 0x50, 0x76, 0x31 };

// Luma coefficients for the YCbCr matrices we support, by CICP
// MatrixCoefficients code (H.273). Returns false for codes we don't
// handle (callers should fall back to BT.709).
inline bool
matrix_luma_coefficients(int matrix_coefficients, float& kr, float& kb)
{
    switch (matrix_coefficients) {
    case 1:  // BT.709
        kr = 0.2126f;
        kb = 0.0722f;
        return true;
    case 5:  // BT.601 (625)
    case 6:  // BT.601 (525)
        kr = 0.299f;
        kb = 0.114f;
        return true;
    case 9:   // BT.2020 non-constant luminance
    case 10:  // BT.2020 constant luminance (treated as NCL here)
        kr = 0.2627f;
        kb = 0.0593f;
        return true;
    default: return false;
    }
}



// Number of image channels our plugin exposes for an APV color format.
// Returns 0 for formats we do not support.
inline int
channels_for_format(int color_format)
{
    switch (color_format) {
    case OAPV_CF_YCBCR400: return 1;
    case OAPV_CF_YCBCR420:
    case OAPV_CF_YCBCR422:
    case OAPV_CF_YCBCR444: return 3;
    case OAPV_CF_YCBCR4444: return 4;
    default: return 0;
    }
}



// Scale an n-bit code value up to the full 16-bit range with bit
// replication (the same convention other OIIO readers use for 10/12 bit
// media).
inline uint16_t
scale_to_16bits(uint32_t v, int bits)
{ return uint16_t((v << (16 - bits)) | (v >> (2 * bits - 16))); }



// Minimal oapv_imgb allocator, mirroring the reference application: the
// library does not export one, callers own the frame buffers. Plane
// dimensions are macroblock aligned as the library requires.

inline int
imgb_addref(oapv_imgb_t* imgb)
{ return ++imgb->refcnt; }

inline int
imgb_getref(oapv_imgb_t* imgb)
{ return imgb->refcnt; }

inline int
imgb_release(oapv_imgb_t* imgb)
{
    int refcnt = --imgb->refcnt;
    if (refcnt == 0) {
        for (int i = 0; i < OAPV_MAX_CC; i++)
            free(imgb->baddr[i]);
        free(imgb);
    }
    return refcnt;
}

inline oapv_imgb_t*
imgb_create(int w, int h, int cs)
{
    oapv_imgb_t* imgb = (oapv_imgb_t*)calloc(1, sizeof(oapv_imgb_t));
    if (!imgb)
        return nullptr;
    int bd     = OAPV_CS_GET_BYTE_DEPTH(cs);
    imgb->w[0] = w;
    imgb->h[0] = h;
    switch (OAPV_CS_GET_FORMAT(cs)) {
    case OAPV_CF_YCBCR400: imgb->np = 1; break;
    case OAPV_CF_YCBCR420:
        imgb->w[1] = imgb->w[2] = (w + 1) >> 1;
        imgb->h[1] = imgb->h[2] = (h + 1) >> 1;
        imgb->np                = 3;
        break;
    case OAPV_CF_YCBCR422:
        imgb->w[1] = imgb->w[2] = (w + 1) >> 1;
        imgb->h[1] = imgb->h[2] = h;
        imgb->np                = 3;
        break;
    case OAPV_CF_YCBCR444:
        imgb->w[1] = imgb->w[2] = w;
        imgb->h[1] = imgb->h[2] = h;
        imgb->np                = 3;
        break;
    case OAPV_CF_YCBCR4444:
        imgb->w[1] = imgb->w[2] = imgb->w[3] = w;
        imgb->h[1] = imgb->h[2] = imgb->h[3] = h;
        imgb->np                             = 4;
        break;
    default: free(imgb); return nullptr;
    }
    for (int i = 0; i < imgb->np; i++) {
        imgb->aw[i]    = ((imgb->w[i] + OAPV_MB_W - 1) / OAPV_MB_W) * OAPV_MB_W;
        imgb->ah[i]    = ((imgb->h[i] + OAPV_MB_H - 1) / OAPV_MB_H) * OAPV_MB_H;
        imgb->s[i]     = imgb->aw[i] * bd;
        imgb->e[i]     = imgb->ah[i];
        imgb->bsize[i] = imgb->s[i] * imgb->e[i];
        imgb->a[i] = imgb->baddr[i] = calloc(1, imgb->bsize[i]);
        if (!imgb->a[i]) {
            for (int j = 0; j < i; j++)
                free(imgb->baddr[j]);
            free(imgb);
            return nullptr;
        }
    }
    imgb->cs      = cs;
    imgb->addref  = imgb_addref;
    imgb->getref  = imgb_getref;
    imgb->release = imgb_release;
    imgb->addref(imgb);
    return imgb;
}

}  // namespace apv_pvt

OIIO_PLUGIN_NAMESPACE_END
