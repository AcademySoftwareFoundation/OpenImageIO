#!/usr/bin/python 


# Test that we cleanly replace any existing SHA-1 hash and ConstantColor
# hint in the ImageDescription of the input file.
command += (oiio_app("oiiotool") + " --pattern:color=1,0,0 constant 64x64 3 "
            + " --caption \"foo SHA-1=1234abcd ConstantColor=[0.0,0,-0.0] bar\""
            + " -d uint8 -o " + oiio_relpath("small.tif") + " >> out.txt;\n")
command += info_command ("small.tif");
command += maketx_command ("small.tif", "small.tx",
                           "--oiio --constant-color-detect")
command += info_command ("small.tx");

outputs = [ "out.txt" ]
