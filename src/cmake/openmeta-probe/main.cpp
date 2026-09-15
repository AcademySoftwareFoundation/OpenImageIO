// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <openmeta/exif_tiff_decode.h>
#include <openmeta/xmp_decode.h>

int
main()
{
    openmeta::MetaStore store;
    openmeta::decode_exif_tiff({}, store, {}, {});
    openmeta::decode_xmp_packet({}, store);
}
