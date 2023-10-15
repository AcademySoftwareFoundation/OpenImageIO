// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


// Bluenoise table. This table is derived using free blue noise textures
// found here: https://momentsingraphics.de/BlueNoise.html
// Generated with these scripts: https://github.com/MomentsInGraphics/BlueNoise/
// The textures bear this notice:
//
// To the extent possible under law, Christoph Peters has waived all copyright
// and related or neighboring rights to the files in this directory and its
// subdirectories. This work is published from: Germany.
//
// The work is made available under the terms of the Creative Commons CC0 Public
// Domain Dedication.
//
// For more information please visit:
// https://creativecommons.org/publicdomain/zero/1.0/


// Here is the command used to transform from the PNG to float OpenEXR:
//
// oiiotool -iconfig oiio:UnassociatedAlpha 1 HDR_RGBA_4.png -d float -o HDR_RGBA_4.exr
//
// Then we dumped the values out to form this header tile, mostly using
//
// oiiotool -dumpdata:C=bluenoise_table HDR_RGBA_4.exr > bluenoise.inc
//


#include <OpenImageIO/export.h>
#include <OpenImageIO/imagebuf.h>

#include "imageio_pvt.h"

OIIO_NAMESPACE_BEGIN

namespace pvt {

// This inc file hold the actual table, declared as bluenoise_table.
#include "bluenoise.inc"

}  // namespace pvt

OIIO_NAMESPACE_END
