#!/usr/bin/env python

files = [
    "sample-DXT1.dds",
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
    "dds_rgba8_mips.dds" ]
for f in files:
    command += info_command (OIIO_TESTSUITE_IMAGEDIR + "/" + f)
