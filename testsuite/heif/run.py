#!/usr/bin/env python

imagedir = "ref/"
files = [ "IMG_7702_small.heic", "Chimera-AV1-8bit-162.avif" ]
for f in files:
    command = command + info_command (os.path.join(imagedir, f))

# avif conversion is expected to fail if libheif is built without AV1 support
failureok = 1
