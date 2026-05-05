// clang-format off
/* etcdec.h - v0.91
   provides functions to decompress blocks of ETC/EAC compressed images
   written by Sergii "iOrange" Kudlai in 2022

   This library does not allocate memory and is trying to use as less stack as possible

   The library was never optimized specifically for speed but for the overall size
   it has zero external dependencies and is not using any runtime functions

   Supported ETC formats:
   ETC1
   ETC2_RGB
   ETC2_RGB_A1 ("punchthrough" alpha)
   ETC2_RGBA
   EAC_R11
   EAC_RG11

   ETC1/ETC2_RGB/ETC2_RGB_A1/ETC2_RGBA are expected to decompress into 4*4 RGBA blocks 8bit per component (32bit pixel)
   EAC_R11/EAC_RG11 are expected to decompress into 4*4 R/RG blocks of either 32bit float or 16bit unsigned int16 per
   component (32/16bit (R11) and 64/32bit (RG11) pixel)

   For more info, issues and suggestions please visit https://github.com/iOrange/etcdec

   CREDITS:
      Vladimir Vondrus (@mosra)      - fixes for platforms that define char as unsigned type

   LICENSE: See end of file for license information.
*/

#ifndef ETCDEC_HEADER_INCLUDED
#define ETCDEC_HEADER_INCLUDED

/* if ETCDEC_STATIC causes problems, try defining ETCDECDEF to 'inline' or 'static inline' */
#ifndef ETCDECDEF
#ifdef ETCDEC_STATIC
#define ETCDECDEF    static
#else
#ifdef __cplusplus
#define ETCDECDEF    extern "C"
#else
#define ETCDECDEF    extern
#endif
#endif
#endif


/*  Used information sources:

    ETC1 compression
    https://registry.khronos.org/OpenGL/extensions/OES/OES_compressed_ETC1_RGB8_texture.txt
    http://www.jacobstrom.com/publications/packman_sketch.pdf

    ETC2/EAC compression
    https://registry.khronos.org/OpenGL/specs/gl/glspec43.core.pdf
*/


#define ETCDEC_ETC_RGB_BLOCK_SIZE       8
#define ETCDEC_ETC_RGB_A1_BLOCK_SIZE    8
#define ETCDEC_EAC_RGBA_BLOCK_SIZE      16
#define ETCDEC_EAC_R11_BLOCK_SIZE       8
#define ETCDEC_EAC_RG11_BLOCK_SIZE      16

#define ETCDEC_ETC_RGB_COMPRESSED_SIZE(w, h)        ((((w)>>2)*((h)>>2))*ETCDEC_ETC_RGB_BLOCK_SIZE)
#define ETCDEC_ETC_RGB_A1_COMPRESSED_SIZE(w, h)     ((((w)>>2)*((h)>>2))*ETCDEC_ETC_RGB_A1_BLOCK_SIZE)
#define ETCDEC_EAC_RGBA_COMPRESSED_SIZE(w, h)       ((((w)>>2)*((h)>>2))*ETCDEC_EAC_RGBA_BLOCK_SIZE)
#define ETCDEC_EAC_R11_COMPRESSED_SIZE(w, h)        ((((w)>>2)*((h)>>2))*ETCDEC_EAC_R11_BLOCK_SIZE)
#define ETCDEC_EAC_RG11_COMPRESSED_SIZE(w, h)       ((((w)>>2)*((h)>>2))*ETCDEC_EAC_RG11_BLOCK_SIZE)

ETCDECDEF void etcdec_etc_rgb(const void* compressedBlock, void* decompressedBlock, int destinationPitch);
ETCDECDEF void etcdec_etc_rgb_a1(const void* compressedBlock, void* decompressedBlock, int destinationPitch);
ETCDECDEF void etcdec_eac_rgba(const void* compressedBlock, void* decompressedBlock, int destinationPitch);
ETCDECDEF void etcdec_eac_r11_u16(const void* compressedBlock, void* decompressedBlock, int destinationPitch);
ETCDECDEF void etcdec_eac_rg11_u16(const void* compressedBlock, void* decompressedBlock, int destinationPitch);
ETCDECDEF void etcdec_eac_r11_float(const void* compressedBlock, void* decompressedBlock, int destinationPitch, int isSigned);
ETCDECDEF void etcdec_eac_rg11_float(const void* compressedBlock, void* decompressedBlock, int destinationPitch, int isSigned);


#ifdef ETCDEC_IMPLEMENTATION

/* http://graphics.stanford.edu/~seander/bithacks.html#VariableSignExtend */
static int etcdec__extend_sign(int val, int bits) {
    return (val << (32 - bits)) >> (32 - bits);
}

static int etcdec__clamp_255(int value) {
    return value < 0 ? 0 : (value > 255 ? 255 : value);
}
static int etcdec__clamp_2047(int value) {
    return value < 0 ? 0 : (value > 2047 ? 2047 : value);
}


#ifndef ETCDEC_BSWAP64
static unsigned long long etcdec__bswap(unsigned long long x) {
    return (((x) & 0xFF00000000000000ull) >> 56) |
           (((x) & 0x00FF000000000000ull) >> 40) |
           (((x) & 0x0000FF0000000000ull) >> 24) |
           (((x) & 0x000000FF00000000ull) >>  8) |
           (((x) & 0x00000000FF000000ull) <<  8) |
           (((x) & 0x0000000000FF0000ull) << 24) |
           (((x) & 0x000000000000FF00ull) << 40) |
           (((x) & 0x00000000000000FFull) << 56);
}

#define ETCDEC_BSWAP64(x) etcdec__bswap(x)
#endif /* ETCDEC_BSWAP64 */

static void etcdec__decompress_legacy_etc_mode(unsigned long long block,
                                               int r0, int g0, int b0,
                                               int r1, int g1, int b1,
                                               unsigned char* decompressed,
                                               int isOpaque,
                                               int destinationPitch) {
    int flipBit, codeWord0, codeWord1;
    int i, j, x0, y0, x1, y1, m, idx;
    const int (*modifiersTablePtr)[4];

    /* already remapped so we can just use pixel indices "as-is" */
    static int modifierTableRemappedOpaque[8][4] = {
        {  2,   8,  -2,   -8 },
        {  5,  17,  -5,  -17 },
        {  9,  29,  -9,  -29 },
        { 13,  42, -13,  -42 },
        { 18,  60, -18,  -60 },
        { 24,  80, -24,  -80 },
        { 33, 106, -33, -106 },
        { 47, 183, -47, -183 }
    };

    static int modifierTableRemappedTransparent[8][4] = {
        { 0,   8, 0,   -8 },
        { 0,  17, 0,  -17 },
        { 0,  29, 0,  -29 },
        { 0,  42, 0,  -42 },
        { 0,  60, 0,  -60 },
        { 0,  80, 0,  -80 },
        { 0, 106, 0, -106 },
        { 0, 183, 0, -183 }
    };

    flipBit = (block & 0x100000000ull) != 0;

    codeWord0 = (block >> 37) & 0x7;
    codeWord1 = (block >> 34) & 0x7;

    modifiersTablePtr = isOpaque ? modifierTableRemappedOpaque : modifierTableRemappedTransparent;

    /* now decode both blocks, using proper orientation based on flipBit */
    for (i = 0; i < 2; ++i) {
        for (j = 0; j < 4; ++j) {
            x0 = flipBit ? i : j;
            x1 = flipBit ? (i + 2) : j;
            y0 = flipBit ? j : i;
            y1 = flipBit ? j : (i + 2);

            /* if isOpaque == 0 and idx is "msb=1 & lsb=0" (== 2) -> pixel is completely transparent */

            /* block A */
            m = x0 + y0 * 4;
            idx = (((block >> (m + 16)) & 1) << 1) | ((block >> m) & 1);
            m = (x0 * destinationPitch) + (y0 * 4);
            if (isOpaque || idx != 2) {
                decompressed[m + 0] = (unsigned char)etcdec__clamp_255(r0 + modifiersTablePtr[codeWord0][idx]);
                decompressed[m + 1] = (unsigned char)etcdec__clamp_255(g0 + modifiersTablePtr[codeWord0][idx]);
                decompressed[m + 2] = (unsigned char)etcdec__clamp_255(b0 + modifiersTablePtr[codeWord0][idx]);
                decompressed[m + 3] = 0xFF;
            } else {
                *((unsigned int*)(decompressed + m)) = 0u;
            }

            // block B
            m = x1 + y1 * 4;
            idx = (((block >> (m + 16)) & 1) << 1) | ((block >> m) & 1);
            m = (x1 * destinationPitch) + (y1 * 4);
            if (isOpaque || idx != 2) {
                decompressed[m + 0] = (unsigned char)etcdec__clamp_255(r1 + modifiersTablePtr[codeWord1][idx]);
                decompressed[m + 1] = (unsigned char)etcdec__clamp_255(g1 + modifiersTablePtr[codeWord1][idx]);
                decompressed[m + 2] = (unsigned char)etcdec__clamp_255(b1 + modifiersTablePtr[codeWord1][idx]);
                decompressed[m + 3] = 0xFF;
            } else {
                *((unsigned int*)(decompressed + m)) = 0u;
            }
        }
    }
}

static void etcdec__decompress_etc_mode_t_h(unsigned long long block,
                                            int mode,
                                            unsigned char* decompressed,
                                            int isOpaque,
                                            int destinationPitch) {
    int r0, g0, b0, r1, g1, b1;
    int ra, rb, ga, gb, ba, bb, da, db, dist;
    int i, j, k, idx;
    unsigned int paintColors[4];    /* 0xAABBGGRR */

    static char distanceTable[8] = { 3, 6, 11, 16, 23, 32, 41, 64 };

    if (mode == 1) {    /* "T" mode */
        ra = (block >> 59) & 0x3;
        rb = (block >> 56) & 0x3;
        g0 = (block >> 52) & 0xF;
        b0 = (block >> 48) & 0xF;
        r1 = (block >> 44) & 0xF;
        g1 = (block >> 40) & 0xF;
        b1 = (block >> 36) & 0xF;
        da = (block >> 34) & 0x3;
        db = (block >> 32) & 0x1;

        r0 = (ra << 2) | rb;
    } else {            /* "H" mode */
        r0 = (block >> 59) & 0xF;
        ga = (block >> 56) & 0x7;
        gb = (block >> 52) & 0x1;
        ba = (block >> 51) & 0x1;
        bb = (block >> 47) & 0x7;
        r1 = (block >> 43) & 0xF;
        g1 = (block >> 39) & 0xF;
        b1 = (block >> 35) & 0xF;
        da = (block >> 34) & 0x1;
        db = (block >> 32) & 0x1;

        g0 = (ga << 1) | gb;
        b0 = (ba << 3) | bb;
    }

    /* These four bit values are extended to RGB888 by replicating
       the four higher order bits in the four lower order bits. */
    r0 = (r0 << 4) | r0;
    g0 = (g0 << 4) | g0;
    b0 = (b0 << 4) | b0;
    r1 = (r1 << 4) | r1;
    g1 = (g1 << 4) | g1;
    b1 = (b1 << 4) | b1;

    if (mode == 1) {    /* "T" mode */
        dist = (da << 1) | db;
        dist = distanceTable[dist];

        paintColors[0] = 0xFF000000 | (b0 << 16) | (g0 << 8) | r0;
        paintColors[2] = 0xFF000000 | (b1 << 16) | (g1 << 8) | r1;
        paintColors[1] = 0xFF000000 | (etcdec__clamp_255(b1 + dist) << 16)
                                    | (etcdec__clamp_255(g1 + dist) << 8)
                                    |  etcdec__clamp_255(r1 + dist);
        paintColors[3] = 0xFF000000 | (etcdec__clamp_255(b1 - dist) << 16)
                                    | (etcdec__clamp_255(g1 - dist) << 8)
                                    |  etcdec__clamp_255(r1 - dist);
    } else {            /* "H" mode */
        dist = ((r0 << 16) | (g0 << 8) | b0) >= ((r1 << 16) | (g1 << 8) | b1) ? 1 : 0;
        dist |= (da << 2) | (db << 1);
        dist = distanceTable[dist];

        paintColors[0] = 0xFF000000 | (etcdec__clamp_255(b0 + dist) << 16)
                                    | (etcdec__clamp_255(g0 + dist) << 8)
                                    |  etcdec__clamp_255(r0 + dist);
        paintColors[1] = 0xFF000000 | (etcdec__clamp_255(b0 - dist) << 16)
                                    | (etcdec__clamp_255(g0 - dist) << 8)
                                    |  etcdec__clamp_255(r0 - dist);
        paintColors[2] = 0xFF000000 | (etcdec__clamp_255(b1 + dist) << 16)
                                    | (etcdec__clamp_255(g1 + dist) << 8)
                                    |  etcdec__clamp_255(r1 + dist);
        paintColors[3] = 0xFF000000 | (etcdec__clamp_255(b1 - dist) << 16)
                                    | (etcdec__clamp_255(g1 - dist) << 8)
                                    |  etcdec__clamp_255(r1 - dist);
    }

    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            k = i + j * 4;
            idx = (((block >> (k + 16)) & 1) << 1) | ((block >> k) & 1);
            /* if isOpaque == 0 and idx is "msb=1 & lsb=0" (== 2) -> pixel is completely transparent */
            if (isOpaque || idx != 2) {
                ((unsigned int*)decompressed)[j] = paintColors[idx];
            } else {
                ((unsigned int*)decompressed)[j] = 0u;
            }
        }

        decompressed += destinationPitch;
    }
}

static void etcdec__decompress_etc_mode_planar(unsigned long long block, unsigned char* decompressed, int destinationPitch) {
    int ro, go, bo, rh, gh, bh, rv, gv, bv;
    int go1, go2, bo1, bo2, bo3, rh1, rh2;
    int i, j;

    ro  = (block >> 57) & 0x3F;
    go1 = (block >> 56) & 0x01;
    go2 = (block >> 49) & 0x3F;
    bo1 = (block >> 48) & 0x01;
    bo2 = (block >> 43) & 0x03;
    bo3 = (block >> 39) & 0x07;
    rh1 = (block >> 34) & 0x1F;
    rh2 = (block >> 32) & 0x01;
    gh  = (block >> 25) & 0x7F;
    bh  = (block >> 19) & 0x3F;
    rv  = (block >> 13) & 0x3F;
    gv  = (block >>  6) & 0x7F;
    bv  = (block >>  0) & 0x3F;

    go = (go1 << 6) | go2;
    bo = (bo1 << 5) | (bo2 << 3) | bo3;
    rh = (rh1 << 1) | rh2;

    ro = (ro << 2) | (ro >> 4);
    rh = (rh << 2) | (rh >> 4);
    rv = (rv << 2) | (rv >> 4);
    go = (go << 1) | (go >> 6);
    gh = (gh << 1) | (gh >> 6);
    gv = (gv << 1) | (gv >> 6);
    bo = (bo << 2) | (bo >> 4);
    bh = (bh << 2) | (bh >> 4);
    bv = (bv << 2) | (bv >> 4);

    /* With three base colors in RGB888 format, the color of each
       pixel can then be determined as:
        R(x, y) = x * (RH − RO) / 4.0 + y * (RV − RO) / 4.0 + RO
        G(x, y) = x * (GH − GO) / 4.0 + y * (GV − GO) / 4.0 + GO
        B(x, y) = x * (BH − BO) / 4.0 + y * (BV − BO) / 4.0 + BO */
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            decompressed[(j * 4) + 0] = (unsigned char)etcdec__clamp_255((j * (rh - ro) + i * (rv - ro) + (ro << 2) + 2) >> 2);
            decompressed[(j * 4) + 1] = (unsigned char)etcdec__clamp_255((j * (gh - go) + i * (gv - go) + (go << 2) + 2) >> 2);
            decompressed[(j * 4) + 2] = (unsigned char)etcdec__clamp_255((j * (bh - bo) + i * (bv - bo) + (bo << 2) + 2) >> 2);
            decompressed[(j * 4) + 3] = 0xFF;
        }

        decompressed += destinationPitch;
    }
}

static void etcdec__decompress_etc_block(const void* compressedBlock, void* decompressedBlock, int isPunchthrough, int destinationPitch) {
    unsigned long long block;
    int diffBit, newMode;
    int r0, g0, b0, r1, g1, b1;
    unsigned char* decompressed;

    block = ETCDEC_BSWAP64(((unsigned long long*)compressedBlock)[0]);
    decompressed = (unsigned char*)decompressedBlock;

    /* if isPunchthrough == TRUE -> this is actually an 'opaque' bit */
    diffBit = (block & 0x200000000ull) != 0;

    newMode = 0;    /* assume legacy mode by default */

    if (!isPunchthrough && !diffBit) {  /* "individual" mode */
        r0 = (block >> 60) & 0xF;
        r1 = (block >> 56) & 0xF;
        g0 = (block >> 52) & 0xF;
        g1 = (block >> 48) & 0xF;
        b0 = (block >> 44) & 0xF;
        b1 = (block >> 40) & 0xF;

        /* These four bit values are extended to RGB888 by replicating
           the four higher order bits in the four lower order bits. */
        r0 = (r0 << 4) | r0;
        g0 = (g0 << 4) | g0;
        b0 = (b0 << 4) | b0;
        r1 = (r1 << 4) | r1;
        g1 = (g1 << 4) | g1;
        b1 = (b1 << 4) | b1;
    } else {                            /* "differential" mode */
        r0 = (block >> 59) & 0x1F;
        r1 = r0 + etcdec__extend_sign((block >> 56) & 0x7, 3);
        g0 = (block >> 51) & 0x1F;
        g1 = g0 + etcdec__extend_sign((block >> 48) & 0x7, 3);
        b0 = (block >> 43) & 0x1F;
        b1 = b0 + etcdec__extend_sign((block >> 40) & 0x7, 3);

        if (r1 < 0 || r1 > 31) {
            /* First, R and dR are added, and if the sum is not
               within the interval[0, 31], the "T" mode is selected */
            newMode = 1;
        } else if (g1 < 0 || g1 > 31) {
            /* Otherwise, if the sum of Gand dG is outside
               the interval[0, 31], the "H" mode is selected */
            newMode = 2;
        } else if (b1 < 0 || b1 > 31) {
            /* Otherwise, if the sum of Band dB is outside
               of the interval[0, 31], the "planar" mode is selected */
            newMode = 3;
        } else {
            /* Finally the "differential" mode is selected */

            /* These five-bit codewords are extended to RGB888 by replicating
               the top three highest order bits to the three lowest order bits. */
            r0 = (r0 << 3) | (r0 >> 2);
            g0 = (g0 << 3) | (g0 >> 2);
            b0 = (b0 << 3) | (b0 >> 2);
            r1 = (r1 << 3) | (r1 >> 2);
            g1 = (g1 << 3) | (g1 >> 2);
            b1 = (b1 << 3) | (b1 >> 2);
        }
    }

    if (!newMode) {             /* legacy mode: ETC1 & ETC2 */
        etcdec__decompress_legacy_etc_mode(block, r0, g0, b0, r1, g1, b1, decompressed, !isPunchthrough || diffBit, destinationPitch);
    } else if (newMode < 3) {   /* ETC2 "T" and "H" modes */
        etcdec__decompress_etc_mode_t_h(block, newMode, decompressed, !isPunchthrough || diffBit, destinationPitch);
    } else {                    /* ETC2 "planar" mode */
        etcdec__decompress_etc_mode_planar(block, decompressed, destinationPitch);
    }
}

static void etcdec__decompress_eac_block(const void* compressedBlock, void* decompressedBlock, int is11Bit, int destinationPitch, int pixelSize) {
    unsigned long long block;
    unsigned char* decompressed;
    int baseCodeword, multiplier, modifier, idx;
    int i, j, k;
    const signed char* modifiersPtr;

    static signed char modifierTable[16][8] = {
            { -3, -6,  -9, -15, 2, 5, 8, 14 },
            { -3, -7, -10, -13, 2, 6, 9, 12 },
            { -2, -5,  -8, -13, 1, 4, 7, 12 },
            { -2, -4,  -6, -13, 1, 3, 5, 12 },
            { -3, -6,  -8, -12, 2, 5, 7, 11 },
            { -3, -7,  -9, -11, 2, 6, 8, 10 },
            { -4, -7,  -8, -11, 3, 6, 7, 10 },
            { -3, -5,  -8, -11, 2, 4, 7, 10 },
            { -2, -6,  -8, -10, 1, 5, 7,  9 },
            { -2, -5,  -8, -10, 1, 4, 7,  9 },
            { -2, -4,  -8, -10, 1, 3, 7,  9 },
            { -2, -5,  -7, -10, 1, 4, 6,  9 },
            { -3, -4,  -7, -10, 2, 3, 6,  9 },
            { -1, -2,  -3, -10, 0, 1, 2,  9 },
            { -4, -6,  -8,  -9, 3, 5, 7,  8 },
            { -3, -5,  -7,  -9, 2, 4, 6,  8 }
    };

    block = ETCDEC_BSWAP64(((unsigned long long*)compressedBlock)[0]);
    decompressed = (unsigned char*)decompressedBlock;
    baseCodeword = (block >> 56) & 0xFF;
    multiplier = (block >> 52) & 0xF;
    modifiersPtr = modifierTable[(block >> 48) & 0xF];

    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            idx = (block >> ((15 - (j * 4 + i)) * 3)) & 0x7;
            modifier = modifiersPtr[idx];

            if (is11Bit) {
                /* EAC R11/RG11 */
                /* If the multiplier value is zero, we should set the multiplier to 1.0/8.0 */
                /* so that the "multiplier * 8" will resolve to 1 */
                k = etcdec__clamp_2047((baseCodeword * 8 + 4) + (modifier * (multiplier ? multiplier * 8 : 1)));

                /* Now just extending the 11-bits value to 16-bits for convenience */
                *((unsigned short*)(decompressed + (j * pixelSize))) = (unsigned short)((k << 5) | (k >> 6));
            } else {
                /* EAC ETC2 Alpha channel */
                decompressed[j * pixelSize] = (unsigned char)etcdec__clamp_255(baseCodeword + (modifier * multiplier));
            }
        }

        decompressed += destinationPitch;
    }
}


ETCDECDEF void etcdec_etc_rgb(const void* compressedBlock, void* decompressedBlock, int destinationPitch) {
    etcdec__decompress_etc_block(compressedBlock, decompressedBlock, 0, destinationPitch);
}

ETCDECDEF void etcdec_etc_rgb_a1(const void* compressedBlock, void* decompressedBlock, int destinationPitch) {
    etcdec__decompress_etc_block(compressedBlock, decompressedBlock, 1, destinationPitch);
}

ETCDECDEF void etcdec_eac_rgba(const void* compressedBlock, void* decompressedBlock, int destinationPitch) {
    /* first half of the block (64 bits) is an Alpha (EAC 8 bits) compressed data */
    /* second half of the block (64 bits) is just an ETC2_RGB compressed data */
    etcdec__decompress_etc_block(((char*)compressedBlock) + 8, decompressedBlock, 0, destinationPitch);
    etcdec__decompress_eac_block(compressedBlock, ((char*)decompressedBlock) + 3, 0, destinationPitch, 4);
}

ETCDECDEF void etcdec_eac_r11_u16(const void* compressedBlock, void* decompressedBlock, int destinationPitch) {
    etcdec__decompress_eac_block(compressedBlock, decompressedBlock, 1, destinationPitch, 2);
}

ETCDECDEF void etcdec_eac_rg11_u16(const void* compressedBlock, void* decompressedBlock, int destinationPitch) {
    etcdec__decompress_eac_block(compressedBlock, decompressedBlock, 1, destinationPitch, 4);
    etcdec__decompress_eac_block(((char*)compressedBlock) + 8, ((char*)decompressedBlock) + 2, 1, destinationPitch, 4);
}

ETCDECDEF void etcdec_eac_r11_float(const void* compressedBlock, void* decompressedBlock, int destinationPitch, int isSigned) {
    unsigned short block[16];
    unsigned char* decompressed;
    const unsigned short* b;
    int i, j;
    short s;

    etcdec_eac_r11_u16(compressedBlock, block, 4 * 2);
    b = block;
    decompressed = (unsigned char*)decompressedBlock;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j, ++b) {
            if (isSigned) {
                s = (short)b[0];
                *((float*)(decompressed + j * 4)) = (s < 0 ? ((float)s / 32768.0f) : ((float)s / 32767.0f));
            } else {
                *((float*)(decompressed + j * 4)) = (float)b[0] / 65535.0f;
            }
        }
        decompressed += destinationPitch;
    }
}

ETCDECDEF void etcdec_eac_rg11_float(const void* compressedBlock, void* decompressedBlock, int destinationPitch, int isSigned) {
    unsigned short block[16*2];
    unsigned char* decompressed;
    const unsigned short* b;
    int i, j;
    short sr, sg;

    etcdec_eac_rg11_u16(compressedBlock, block, 4 * 4);
    b = block;
    decompressed = (unsigned char*)decompressedBlock;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j, b += 2) {
            if (isSigned) {
                sr = (short)b[0];
                sg = (short)b[1];
                *((float*)(decompressed + j * 8 + 0)) = (sr < 0 ? ((float)sr / 32768.0f) : ((float)sr / 32767.0f));
                *((float*)(decompressed + j * 8 + 4)) = (sg < 0 ? ((float)sg / 32768.0f) : ((float)sg / 32767.0f));
            }
            else {
                *((float*)(decompressed + j * 8 + 0)) = (float)b[0] / 65535.0f;
                *((float*)(decompressed + j * 8 + 4)) = (float)b[1] / 65535.0f;
            }
        }
        decompressed += destinationPitch;
    }
}

#endif /* ETCDEC_IMPLEMENTATION */

#endif /* ETCDEC_HEADER_INCLUDED */

/* LICENSE:

This software is available under 2 licenses -- choose whichever you prefer.

------------------------------------------------------------------------------
ALTERNATIVE A - MIT License

Copyright (c) 2022 Sergii Kudlai

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

------------------------------------------------------------------------------
ALTERNATIVE B - The Unlicense

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <https://unlicense.org>

*/
