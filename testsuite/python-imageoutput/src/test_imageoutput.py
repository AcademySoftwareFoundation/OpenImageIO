#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO
from __future__ import annotations

import OpenImageIO as oiio

import numpy as np


# Read the one subimage from input then write it to output using
# write_image, write_scanlines, write_scanline, write_tile, or write_tiles,
# depending on the 'method' argument).  (Just copy one subimage, one MIP
# level.)
def copy_subimage (in_filename: str, input: oiio.ImageInput, output: oiio.ImageOutput, method="image",
                   memformat=oiio.TypeFloat) :
    spec = input.spec ()
    if method == "image" :
        pixels = input.read_image (memformat)
        if pixels is None :
            print ("Error reading input pixels in", in_filename)
            return False
        output.write_image (pixels)
    elif method == "scanlines" and spec.tile_width == 0 :
        pixels = input.read_image (memformat)
        if pixels is None :
            print ("Error reading input pixels in", in_filename)
            return False
        output.write_scanlines (spec.y, spec.y+spec.height, spec.z,
                                pixels)
    elif method == "scanline" and spec.tile_width == 0 :
        for z in range(spec.z, spec.z+spec.depth) :
            for y in range(spec.y, spec.y+spec.height) :
                pixels = input.read_scanline (y, z, memformat)
                if pixels is None :
                    print ("Error reading input pixels in", in_filename)
                    return False
                output.write_scanline (y, z, pixels)
    elif method == "tiles" and spec.tile_width != 0 :
        pixels = input.read_image (memformat)
        if pixels is None :
            print ("Error reading input pixels in", in_filename)
            return False
        output.write_tiles (spec.x, spec.x+spec.width,
                            spec.y, spec.y+spec.height,
                            spec.z, spec.z+spec.depth,
                            pixels)
    elif method == "tile" and spec.tile_width != 0 :
        for z in range(spec.z, spec.z+spec.depth, spec.tile_depth) :
            for y in range(spec.y, spec.y+spec.height, spec.tile_height) :
                for x in range(spec.x, spec.x+spec.width, spec.tile_width) :
                    pixels = input.read_tile (x, y, z, memformat)
                    if pixels is None :
                        print ("Error reading input pixels in", in_filename)
                        return False
                    output.write_tile (x, y, z, pixels)
    else :
        print ("Unknown method:", method)
        return False
    return True


# Read the whole image then write using write_image, write_scanlines,
# write_scanline, write_tile, or write_tiles, depending on the 'method'
# argument).  (Just copy one subimage, one MIP level.)
def copy_image (in_filename: str, out_filename: str, method="image",
                memformat=oiio.TypeFloat, outformat=oiio.TypeUnknown) :
    input = oiio.ImageInput.open (in_filename)
    if not input :
        print ('Could not open "' + in_filename + '"')
        print ("\tError: ", oiio.geterror())
        print ()
        return
    outspec = input.spec()
    if outformat != oiio.TypeUnknown :
        outspec.format = outformat
    output = oiio.ImageOutput.create (out_filename)
    if not output :
        print ("Could not create ImageOutput for", out_filename)
        return
    ok = output.open (out_filename, outspec)
    if not ok :
        print ("Could not open", out_filename)
        return
    ok = copy_subimage (in_filename, input, output, method, memformat)
    input.close ()
    output.close ()
    if ok :
        print ("Copied", in_filename, "to", out_filename, "as", method,
               "(memformat", memformat, "outformat", outformat, ")")


def test_subimages (out_filename="multipart.exr") :
    output = oiio.ImageOutput.create (out_filename)
    assert output is not None
    spec = oiio.ImageSpec (64, 64, 3, "half")
    specs = (spec, spec, spec)
    output.open (out_filename, specs)
    buffer = np.zeros ((64, 64, 3), dtype=float)
    for i in range(3) :
        if i != 0 :
            output.open (out_filename, specs[i], "AppendSubimage")
        output.write_image (buffer)
    output.close ()

######################################################################
# main test starts here

try:

    copy_image ("scanline.tif", "grid-image.tif", method="image")
    copy_image ("scanline.tif", "grid-scanline.tif", method="scanline")
    copy_image ("scanline.tif", "grid-scanlines.tif", method="scanlines")
    copy_image ("tiled.tif", "grid-timage.tif", method="image")
    copy_image ("tiled.tif", "grid-tile.tif", method="tile")
    copy_image ("tiled.tif", "grid-tiles.tif", method="tiles")

    # Regression test for crash when changing formats
    copy_image ("scanline.tif", "grid-image.tif",
                memformat=oiio.TypeUInt8, outformat=oiio.TypeUInt16)

    # Exercise 'half'
    copy_image ("scanline.tif", "grid-half.exr",
                memformat='half', outformat='half')

    # Ensure we can write multiple subimages
    test_subimages ()

    print ("Testing format_name and supports:")
    probe = oiio.ImageOutput.create ("probe_format.tif")
    print ("  create succeeded:", probe is not None)
    if probe :
        print ("  format_name is tiff:", probe.format_name() == "tiff")
        print ("  supports tiles truthy:", bool (probe.supports ("tiles")))
        print ("  supports returns numeric:",
               isinstance (probe.supports ("thumbnail"), (int, bool)))
    print ("")

    print ("Testing create() invalid format:")
    invalid = oiio.ImageOutput.create ("no_such_format.zzz")
    print ("  invalid create returns None:", invalid is None)
    oiio.geterror()  # clear global error from failed create
    print ("")

    print ("Testing open() bad mode:")
    spec8 = oiio.ImageSpec (8, 8, 3, oiio.UINT8)
    mode_out = oiio.ImageOutput.create ("bad_mode.tif")
    try :
        mode_out.open ("bad_mode.tif", spec8, "BadMode")
        print ("  bad mode: no exception")
    except ValueError :
        print ("  bad mode: ValueError")
    print ("")

    print ("Testing write path errors:")
    scan_out = oiio.ImageOutput.create ("scan_mismatch.tif")
    scan_out.open ("scan_mismatch.tif", spec8)
    tile_ok = scan_out.write_tile (0, 0, 0,
                                   np.zeros ((8, 8, 3), dtype=np.uint8))
    print ("  tile on scanline fails:", (not tile_ok) and scan_out.has_error)
    print ("  tile on scanline error nonempty:",
           len (scan_out.geterror()) > 0)
    scan_out.close ()
    tile_spec = oiio.ImageSpec (64, 64, 3, oiio.UINT8)
    tile_spec.tile_width = 64
    tile_spec.tile_height = 64
    tile_out = oiio.ImageOutput.create ("tile_mismatch.tif")
    tile_out.open ("tile_mismatch.tif", tile_spec)
    scanline_ok = tile_out.write_scanline (0, 0,
                                           np.zeros ((64, 3), dtype=np.uint8))
    print ("  scanline on tiled fails:", (not scanline_ok) and tile_out.has_error)
    tile_out.close ()
    short_out = oiio.ImageOutput.create ("shortbuf.tif")
    short_out.open ("shortbuf.tif", spec8)
    short_ok = short_out.write_image (np.zeros ((4, 4, 3), dtype=np.uint8))
    print ("  short buffer fails:", (not short_ok) and short_out.has_error)
    short_out.close ()
    print ("")

    print ("Testing copy_image:")
    inp = oiio.ImageInput.open ("scanline.tif")
    copy_out = oiio.ImageOutput.create ("copy_image.tif")
    copy_spec = inp.spec()
    copy_out.open ("copy_image.tif", copy_spec)
    copy_ok = copy_out.copy_image (inp)
    copy_out.close ()
    inp.close ()
    print ("  copy_image ok:", copy_ok)
    print ("")

    print ("Testing set_thumbnail:")
    thumb_out = oiio.ImageOutput.create ("thumb_test.tif")
    thumb_out.open ("thumb_test.tif", spec8)
    thumb = oiio.ImageBuf (oiio.ImageSpec (4, 4, 3, oiio.UINT8))
    thumb_ok = thumb_out.set_thumbnail (thumb)
    print ("  set_thumbnail returned bool:", isinstance (thumb_ok, bool))
    thumb_out.close ()
    print ("")

    print ("Testing AppendMIPLevel:")
    mip_out = oiio.ImageOutput.create ("mip_level.exr")
    mip_spec = oiio.ImageSpec (64, 64, 1, oiio.FLOAT)
    mip_out.open ("mip_level.exr", mip_spec)
    mip_out.write_image (np.zeros ((64, 64, 1), dtype=np.float32))
    mip_out.open ("mip_level.exr",
                  oiio.ImageSpec (32, 32, 1, oiio.FLOAT),
                  "AppendMIPLevel")
    mip_out.write_image (np.zeros ((32, 32, 1), dtype=np.float32))
    mip_out.close ()
    print ("  AppendMIPLevel ok: True")
    print ("")

    print ("Testing has_error/geterror on instance:")
    err_out = oiio.ImageOutput.create ("err_out.tif")
    err_out.open ("err_out.tif", spec8)
    err_out.write_image (np.zeros ((4, 4, 3), dtype=np.uint8))
    print ("  has_error after bad write:", err_out.has_error)
    err1 = err_out.geterror (clear=False)
    err2 = err_out.geterror (clear=False)
    print ("  geterror persists:", err1 == err2 and len (err1) > 0)
    err3 = err_out.geterror (clear=True)
    print ("  geterror clear returns message:", err3 == err1)
    print ("  has_error after clear:", err_out.has_error)
    err_out.close ()
    print ("")

    print ("Done.")
except Exception as detail:
    print ("Unknown exception:", detail)

