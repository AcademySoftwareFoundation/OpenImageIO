#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

failureok = 1
redirect = ' >> out.txt 2>&1 '
files = [
    "dds_bc1.dds",
    "dds_bc1_mips.dds",
    "dds_bc2.dds",
    "dds_bc3.dds",
    "dds_bc4.dds",
    "dds_bc5.dds",
    "dds_bc6hu.dds",
    "dds_bc6hu_hdr.dds",
    "dds_bc7.dds",
    "dds_bgr8.dds",
    "dds_npot_bc3.dds",
    "dds_npot_bc3_mips.dds",
    "dds_npot_rgba8.dds",
    "dds_npot_rgba8_mips.dds",
    "dds_r5g6b5.dds",
    "dds_rgb8.dds",
    "dds_rgba4.dds",
    "dds_rgba8.dds",
    "dds_rgba8_mips.dds",
    "dds_a8.dds",
    "dds_abgr8.dds",
    "dds_bc3nm.dds",
    "dds_bc3rxgb.dds",
    "dds_bc3ycocg.dds",
    "dds_l8.dds",
    "dds_l8a8.dds",
    "dds_rgb10a2.dds",
    "dds_rgb332.dds",
    "dds_rgb5a1.dds",
    "dds_dxgi_bc1_srgb.dds",
    "dds_dxgi_bc2_srgb.dds",
    "dds_dxgi_bc3_srgb.dds",
    "dds_dxgi_bc7_srgb.dds",
    "dds_dxgi_rgba8_srgb.dds",
    "dds_dxgi_bgra8_srgb.dds",
    "dds_dxgi_bgrx8_srgb.dds",
    "dds_dxgi_rgb10a2.dds",
    "dds_dxgi_r16.dds",

    # In some CI matrix cases a PtexReader
    # also attempts to read this, and outputs
    # a "PtexReader error: read failed (EOF)"
    # failure error.
    #"broken/dds_8bytes.dds",
    "broken/dds_bc3_just_header.dds",
    "broken/dds_bc3_no_full_header.dds",
    "broken/dds_bc7_just_header.dds",
    "broken/dds_bc7_not_enough_data.dds" ]
for f in files:
    command += info_command (OIIO_TESTSUITE_IMAGEDIR + "/" + f)

# Test more corrupted files or those that used to crash
command += info_command ("src/crash-1634.dds", hash=True)
command += info_command ("src/crash-1635.dds", hash=True)
command += info_command ("src/crash-3950.dds", hash=True)
