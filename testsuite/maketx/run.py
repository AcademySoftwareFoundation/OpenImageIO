#!/usr/bin/env python 

# location of oiio-images directory
oiio_images = parent + "/oiio-images/"

# Just for simplicity, make a checkerboard with a solid alpha
command += oiiotool (" --pattern checker 128x128 4 --ch R,G,B,=1.0"
            + " -d uint8 -o " + oiio_relpath("checker.tif") )

# Basic test - recreate the grid texture
command += maketx_command (oiio_images + "grid.tif", "grid.tx", showinfo=True)

# Test --resize (to power of 2) with the grid, which is 1000x1000
command += maketx_command (oiio_images + "grid.tif", "grid-resize.tx",
                           "--resize", showinfo=True)

# Test -d to set output data type
command += maketx_command ("checker.tif", "checker-uint16.tx",
                           "-d uint16", showinfo=True)

# Test --nchannels to restrict the number of channels
command += maketx_command ("checker.tif", "checker-1chan.tx",
                           "--nchannels 1", showinfo=True)

# Test --tiles to set a non-default tile size
command += maketx_command ("checker.tif", "checker-16x32tile.tx",
                           "--tile 16 32", showinfo=True)

# Test --separate and --compression
command += maketx_command ("checker.tif", "checker-seplzw.tx",
                           "--separate --compression lzw", showinfo=True)

# Test --wrap
command += maketx_command ("checker.tif", "checker-clamp.tx",
                           "--wrap clamp", showinfo=True)

# Test --swrap and --twrap
command += maketx_command ("checker.tif", "checker-permir.tx",
                           "--swrap periodic --twrap mirror", showinfo=True)

# Test --nomipmap
command += maketx_command ("checker.tif", "checker-nomip.tx",
                           "--nomipmap", showinfo=True)

# Test --Mcamera, --Mscreen
command += maketx_command ("checker.tif", "checker-camera.tx",
                           "--Mcamera 1 0 0 0 0 2 0 0 0 0 1 0 0 0 0 1 --Mscreen 3 0 0 0 0 3 0 0 0 0 3 0 1 2 3 1",
                           showinfo=True)

# Test --opaque-detect (should drop the alpha channel)
command += maketx_command ("checker.tif", "checker-opaque.tx",
                           "--opaque-detect", showinfo=True)

# Test --monochrome-detect (first create a monochrome image)
command += oiiotool (" --pattern constant:color=.25,.25,.25 256x256 3 "
                    + " -d uint8 -o " + oiio_relpath("gray.tif"))
command += maketx_command ("gray.tif", "gray-mono.tx",
                           "--monochrome-detect", showinfo=True)

# Test --monochrome-detect on something that is NOT monochrome
command += oiiotool (" --pattern constant:color=.25,.2,.15 256x256 3 "
                    + " -d uint8 -o " + oiio_relpath("pink.tif"))
command += maketx_command ("pink.tif", "pink-mono.tx",
                           "--monochrome-detect", showinfo=True)

# Test --prman : should save 'separate' planarconfig, and funny 64x32 tiles
# since we are specifying 16 bits, and it should save as 'int16' even though
# we asked for unsigned.
command += maketx_command ("checker.tif", "checker-prman.tx",
                           "-d uint16 --prman", showinfo=True)

# Test --fixnan : take advantage of the bad.exr images in 
# testsuite/oiiotool-fixnan.  (Use --nomipmap to cut down on stats output)
# FIXME: would also like to test --checknan, but the problem with that is
# that is actually FAILS if there's a nan.
command += maketx_command ("../oiiotool-fixnan/src/bad.exr",
                           "nan.exr", "--fixnan box3 --nomipmap",
                           showinfo=True, showinfo_extra="--stats")

# Test --format to force exr even though it can't be deduced from the name.
command += maketx_command ("checker.tif", "checker-exr.pdq",
                           "--format exr", showinfo=True)

# Test that we cleanly replace any existing SHA-1 hash and ConstantColor
# hint in the ImageDescription of the input file.
command += oiiotool (" --pattern constant:color=1,0,0 64x64 3 "
            + " --caption \"foo SHA-1=1234abcd ConstantColor=[0.0,0,-0.0] bar\""
            + " -d uint8 -o " + oiio_relpath("small.tif") )
command += info_command ("small.tif", safematch=1);
command += maketx_command ("small.tif", "small.tx",
                           "--oiio --constant-color-detect", showinfo=True)

# Test that the oiio:SHA-1 hash is stable, and that that changing filter and
# using -hicomp result in different images and different hashes.
command += maketx_command (oiio_images + "grid.tif", "grid-lanczos3.tx",
                           extraargs = "-filter lanczos3")
command += maketx_command (oiio_images + "grid.tif", "grid-lanczos3-hicomp.tx",
                           extraargs = "-filter lanczos3 -hicomp")
command += info_command ("grid.tx",
                         extraargs="--metamatch oiio:SHA-1")
command += info_command ("grid-lanczos3.tx",
                         extraargs="--metamatch oiio:SHA-1")
command += info_command ("grid-lanczos3-hicomp.tx",
                         extraargs="--metamatch oiio:SHA-1")

# Regression test -- at one point, we had a bug where we were botching
# the poles of OpenEXR env maps, adding energy.  Check it by creating an
# all-white image, turning it into an env map, and calculating its
# statistics (should be 1.0 everywhere).
command += oiiotool (" --pattern constant:color=1,1,1 4x2 3 "
            + " -d half -o " + oiio_relpath("white.exr"))
command += maketx_command ("white.exr", "whiteenv.exr",
                           "--envlatl")
command += oiiotool ("--stats whiteenv.exr")

#Test --bumpslopes to export a 6 channels height map with gradients
command += oiiotool (" --pattern noise 64x64 1"
           + " -d half -o " + oiio_relpath("bump.exr"))
command += maketx_command ("bump.exr", "bumpslope.exr",
                           "--bumpslopes -d half", showinfo=True)


outputs = [ "out.txt" ]



# To do:  --filter --checknan --fullpixels
#         --prman-metadata --ignore-unassoc
#         --mipimage 
#         --envlatl TIFF, --envlatl EXR
#         --colorconvert --unpremult -u --fovcot
