#!/usr/bin/env python 

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Construct a command that will create a texture, appending console
# output to the file "out.txt".
def omaketx_command (infile, outfile, extraargs="",
                     options="", output_cmd="-otex",
                     showinfo=True, showinfo_extra="",
                     silent=False, concat=True) :
    command = (oiio_app("oiiotool") 
               + " " + make_relpath(infile,tmpdir) 
               + " " + extraargs
               + " " + output_cmd + options + " " + make_relpath(outfile,tmpdir) )
    if not silent :
        command += " >> out.txt"
    if concat:
        command += " ;\n"
    if showinfo:
        command += info_command (outfile, extraargs=showinfo_extra, safematch=1)
    return command



# Just for simplicity, make a checkerboard with a solid alpha
command += oiiotool (" --pattern checker 128x128 4 --ch R,G,B,=1.0"
                     + " -d uint8 -o " + make_relpath("checker.tif") )

# Basic test - recreate the grid texture
command += omaketx_command ("../common/grid.tif", "grid.tx")

# Test --resize (to power of 2) with the grid, which is 1000x1000
command += omaketx_command ("../common/grid.tif", "grid-resize.tx",
                            options=":resize=1")

# Test -d to set output data type
command += omaketx_command ("checker.tif", "checker-uint16.tx",
                            "-d uint16")

# Test --ch to restrict the number of channels
command += omaketx_command ("checker.tif", "checker-1chan.tx",
                            "--ch 0")

# Test --tiles to set a non-default tile size
command += omaketx_command ("checker.tif", "checker-16x32tile.tx",
                            "--tile 16 32")

# Test --separate and --compression
command += omaketx_command ("checker.tif", "checker-seplzw.tx",
                             "--planarconfig separate --compression lzw")

# Test --wrap
command += omaketx_command ("checker.tif", "checker-clamp.tx",
                            options=":wrap=clamp")

# Test --swrap and --twrap
command += omaketx_command ("checker.tif", "checker-permir.tx",
                            options=":swrap=periodic:twrap=mirror")

# Test --nomipmap
command += omaketx_command ("checker.tif", "checker-nomip.tx",
                            options=":nomipmap=1")

# Test setting matrices
command += omaketx_command ("checker.tif", "checker-camera.tx",
                           "--attrib:type=matrix worldtocamera 1,0,0,0,0,2,0,0,0,0,1,0,0,0,0,1 " +
                           "--attrib:type=matrix worldtoscreen 3,0,0,0,0,3,0,0,0,0,3,0,1,2,3,1")

# Test --opaque-detect (should drop the alpha channel)
command += omaketx_command ("checker.tif", "checker-opaque.tx",
                            options=":opaque_detect=1")

# Test --monochrome-detect (first create a monochrome image)
command += oiiotool (" --pattern constant:color=.25,.25,.25 256x256 3 "
                    + " -d uint8 -o " + make_relpath("gray.tif"))
command += omaketx_command ("gray.tif", "gray-mono.tx",
                            options=":monochrome_detect=1")

# Test --monochrome-detect on something that is NOT monochrome
command += oiiotool (" --pattern constant:color=.25,.2,.15 256x256 3 "
                    + " -d uint8 -o " + make_relpath("pink.tif"))
command += omaketx_command ("pink.tif", "pink-mono.tx",
                            options=":monochrome_detect=1")

# Test --prman : should save 'separate' planarconfig, and funny 64x32 tiles
# since we are specifying 16 bits, and it should save as 'int16' even though
# we asked for unsigned.
command += omaketx_command ("checker.tif", "checker-prman.tx",
                           "-d uint16", options=":prman=1")

# Test --fixnan : take advantage of the bad.exr images in 
# testsuite/oiiotool-fixnan.  (Use --nomipmap to cut down on stats output)
# FIXME: would also like to test --checknan, but the problem with that is
# that is actually FAILS if there's a nan.
command += omaketx_command (OIIO_TESTSUITE_ROOT+"/oiiotool-fixnan/src/bad.exr", "nan.exr",
                            "--fixnan box3", options=":nomipmap=1",
                            showinfo=True, showinfo_extra="--stats")

# Test that when outputting half textures, we clamp large float values
# rather than inadvertetly turning into Inf in the process of output to
# half.
command += oiiotool (" --pattern constant:color=1.0e6,1.0e6,1.0e6 2x2 3 -d float -o million.tif")
command += omaketx_command ("million.tif", "bigval.exr",
                            "-d half", showinfo_extra="--stats")

# Test --format to force exr even though it can't be deduced from the name.
command += omaketx_command ("checker.tif", "checker-exr.pdq",
                            options=":fileformatname=exr")

# Test that the oiio:SHA-1 hash is stable, and that that changing filter and
# using -hicomp result in different images and different hashes.
command += omaketx_command ("../common/grid.tif", "grid-lanczos3.tx",
                           options = ":filter=lanczos3", showinfo=False)
command += omaketx_command ("../common/grid.tif", "grid-lanczos3-hicomp.tx",
                           options = ":filter=lanczos3:highlightcomp=1", showinfo=False)
command += info_command ("grid.tx",
                         extraargs="--metamatch oiio:SHA-1")
command += info_command ("grid-lanczos3.tx",
                         extraargs="--metamatch oiio:SHA-1")
command += info_command ("grid-lanczos3-hicomp.tx",
                         extraargs="--metamatch oiio:SHA-1")

# Test that we cleanly replace any existing SHA-1 hash and ConstantColor
# hint in the ImageDescription of the input file.
command += oiiotool (" --pattern constant:color=1,0,0 64x64 3 "
            + " --caption \"foo SHA-1=1234abcd ConstantColor=[0.0,0,-0.0] bar\""
            + " -d uint8 -o " + make_relpath("small.tif") )
command += info_command ("small.tif", safematch=1);
command += omaketx_command ("small.tif", "small.tx",
                            options=":oiio=1:constant_color_detect=1")

# Regression test -- at one point, we had a bug where we were botching
# the poles of OpenEXR env maps, adding energy.  Check it by creating an
# all-white image, turning it into an env map, and calculating its
# statistics (should be 1.0 everywhere).
command += oiiotool (" --pattern constant:color=1,1,1 4x2 3 "
            + " -d half -o " + make_relpath("white.exr"))
command += omaketx_command ("white.exr", "whiteenv.exr",
                            output_cmd="-oenv", showinfo=False)
command += oiiotool ("--stats -a whiteenv.exr")

command += oiiotool (" --pattern noise 64x64 1"
            + " -d half -o " + make_relpath("bump.exr"))
command += omaketx_command ("bump.exr", "bumpslope.exr",
                            extraargs="-d half",
                            output_cmd="-obump", showinfo=False)
command += oiiotool ("--stats -a bumpslope.exr")
command += omaketx_command ("bump.exr", "bumpslope-cdf.exr",
                            extraargs="-d half",
                            output_cmd="-obump", options=":cdf=1", showinfo=False)
command += oiiotool ("--stats -a bumpslope-cdf.exr")
command += info_command ("bumpslope-cdf.exr", safematch=1)


outputs = [ "out.txt" ]



# To do:  --filter --checknan --fullpixels
#         --prman-metadata --ignore-unassoc
#         --mipimage 
#         --envlatl TIFF, --envlatl EXR
#         --colorconvert --unpremult -u --fovcot
