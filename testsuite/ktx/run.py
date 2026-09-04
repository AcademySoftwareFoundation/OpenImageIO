#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# All of the test files here are copied, as is, from KTX-Software repo and fall
# under the Apache-2.0 license of KTX-Software:
#
#   Copyright 2013-2020 Mark Callow SPDX-License-Identifier: Apache-2.0

#
# KTX-Software has two sets of ktx2 test files:
#   - a relatively small set for libktx:
#     https://github.com/KhronosGroup/KTX-Software/tree/e2f948066c108b56b8d0052b460b2ac7d34886aa/tests/resources/ktx2
#   - a very larget tests set for ktx tools:
#     https://github.com/KhronosGroup/KTX-Software-CTS/tree/6d23ae9e52cce2ebc6495c4692ec89f632ff70d4
#
# commit hashse:
#   - libktx test files: 6c474d8627999de8acf07d819c196f83d025cd44
#   - ktx tools test files (CTS): 6d23ae9e52cce2ebc6495c4692ec89f632ff70d4
# 
# Since OIIO KTX2 plugin simply forwards all operations to libktx, there is no
# need to do extensive testing on encoding/decoding functionalities. libktx
# already does very extensive testing on thousands of ktx2 inputs. What we do
# instead is that we test that we call libktx correctly and that parameters
# (which are numerous) are passed correctly.
#

# save the error output
redirect = ' >> out.txt 2>&1 '
files = [

    # raw (uncompressed + non-supercompressed) formats
    "r8g8b8a8_srgb.ktx2",
    "r8g8b8a8_srgb_mip.ktx2",
    "r8g8b8_srgb_mip.ktx2",
    "r8g8b8a8_srgb_3d_7.ktx2",
    "r8g8b8a8_srgb_array_7_mip.ktx2",

    # ETC-compressed formats (not supported)
    # "r8g8b8a8_srgb_mip_etc2.ktx2",
    # "etc2_unorm_array_7.ktx2",

    # raw (uncompressed) and supercompressed formats
    "color_grid_zstd_5.ktx2",
    # "skybox_zstd_22.ktx2", # VK_FORMAT_B10G11R11_UFLOAT_PACK32 not yet supported

    # ASTC-compressed formats
    "r8g8b8a8_srgb_mip_astc.ktx2",
    "ktx_app_astc_8x8.ktx2",
    # "astc_8x8_unorm_array_7.ktx2", # VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK

    # BCn-compressed formats (to be supported)
    # "bc3_unorm_array_7.ktx2",
    # "pattern_02_bc2.ktx2",

    # UASTC formats (widely used within KTX2 container format)
    "color_grid_uastc_zstd_5.ktx2",
    "Iron_Bars_001_normal_uastc_zstd_10.ktx2",
    "ktx_document_uastc_rdo_4_zstd_5.ktx2",
    "cubemap_goldengate_uastc_rdo_4_zstd_5.ktx2",

    # HDR formats
    "Desk_small_zstd_15.ktx2", # VK_FORMAT_R16G16B16_SFLOAT
    "Desk_uastc_hdr4x4_zstd_15.ktx2",
    "Desk_uastc_hdr6x6i.ktx2",
    "Desk_astc_hdr6x6.ktx2", # VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK 

    # Basis LZ/ETC1S formats (widely used within KTX2 container format)
    "kodim17_blze.ktx2",
    "r8g8b8a8_srgb_mip_blze.ktx2",
    "color_grid_blze.ktx2",
    "alpha_simple_blze.ktx2",
    "cubemap_yokohama_blze.ktx2",
    "FlightHelmet_baseColor_blze.ktx2",
    "Iron_Bars_001_normal_blze.ktx2",
    "ktx_document_blze.ktx2",

    # Misc (orientation flags, alpha configurations, etc.)
    "alpha_complex_straight.ktx2",
    "orient_down_metadata.ktx2",
    "orient_up_metadata.ktx2",
]

# Most basic testing; just test `oiiotool --info` on libktx main test files
for f in files:
    command += info_command (OIIO_TESTSUITE_IMAGEDIR + "/" + f)

# Create a simple checker pattern RGBA PNG (has to be RGBA because Basis
# Universal codecs cannot transcode to opaque uncompressed formats)
command += (oiio_app("oiiotool") 
            + " --pattern checker 64x64 4 -d uint8 -o checker_original.png >> out.txt ;\n")

# Default write (with nothing specified) should default to a lossless format
# + supercompression scheme and should match exactly with original input
command += oiiotool ("checker_original.png -o checker_default.ktx2")
command += diff_command ("checker_original.png", "checker_default.ktx2", "--fail 0 --warn 0")

# UASTC write test: check generation of an UASTC-based KTX2 file
command += oiiotool ("checker_original.png --attrib ktx:codec uastc -o checker_uastc.ktx2")
command += diff_command ("checker_original.png", "checker_uastc.ktx2")

# ETC1S write test: check generation of an ETC1S-based KTX2 file
command += oiiotool ("checker_original.png --attrib ktx:codec etc1s -o checker_etc1s.ktx2")
command += diff_command ("checker_original.png", "checker_etc1s.ktx2")

# We do not test read-then-write of compressed-ktx2 files because any read-write
# cycle worsens quality and is absolutely not the intended purpose of ktx usage
# within OIIO (or in general).

# Test that re-orientation metadata is handled correctly.
# orient_[up|down]_metadata are essentially the same image with one flipped
# vertically (orient_up_metadata)
command += (oiio_app("oiiotool") + (OIIO_TESTSUITE_IMAGEDIR + "/" + "orient_up_metadata.ktx2")
            + " --reorient -o orient_up_metadata.png >> out.txt ;\n")
command += diff_command (OIIO_TESTSUITE_IMAGEDIR + "/" + "orient_down_metadata.ktx2", "orient_up_metadata.png")
