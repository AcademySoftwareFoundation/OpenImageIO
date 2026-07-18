#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# All of the test files here are copied, as is, from KTX-Software repo and fall
# under the license of KTX-Software:
#
#   Copyright 2013-2020 Mark Callow SPDX-License-Identifier: Apache-2.0

# KTX-Software has two sets of ktx2 test files:
#   - a relatively small set for libktx: https://github.com/KhronosGroup/KTX-Software/tree/e2f948066c108b56b8d0052b460b2ac7d34886aa/tests/resources/ktx2
#   - a very larget tests set for ktx tools: https://github.com/KhronosGroup/KTX-Software-CTS/tree/6d23ae9e52cce2ebc6495c4692ec89f632ff70d4
#
# commit hashse:
#   - libktx test files: 6c474d8627999de8acf07d819c196f83d025cd44
#   - ktx tools test files (CTS): 6d23ae9e52cce2ebc6495c4692ec89f632ff70d4

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
    # "astc_8x8_unorm_array_7.ktx2", # VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK not yet supported

    # BCn-compressed formats (to be supported)
    # "bc3_unorm_array_7.ktx2",
    # "pattern_02_bc2.ktx2",

    # UASTC formats (widely used within KTX2 container format)
    "color_grid_uastc_zstd_5.ktx2",
    "Iron_Bars_001_normal_uastc_zstd_10.ktx2",
    "ktx_document_uastc_rdo_4_zstd_5.ktx2",
    "cubemap_goldengate_uastc_rdo_4_zstd_5.ktx2",

    # HDR formats (not yet supported)
    # "Desk_uastc_hdr4x4_zstd_15.ktx2",
    # "Desk_uastc_hdr6x6i.ktx2",
    # "Desk_astc_hdr6x6.ktx2",
    # "Desk_small_zstd_15.ktx2", # VK_FORMAT_R16G16B16_SFLOAT is not yet supported

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

for f in files:
    command += info_command (OIIO_TESTSUITE_IMAGEDIR + "/" + f)

# We do not test read-write of compressed-ktx2 files because any read-write
# cycle worsens quality and is absolutely not the intended purpose of ktx usage
# within OIIO

# Test write of PNG inputs

# Default write (with nothing specified) should default to a loseless format + supercompression scheme

