#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import os
import numpy

import OpenImageIO as oiio


checker = os.path.abspath("../common/textures/checker.tx")
texture_sys = oiio.TextureSystem()
texture_opt = oiio.TextureOpt()

texture_opt.swrap = oiio.Wrap.Periodic
texture_opt.twrap = oiio.Wrap.Periodic


# Test getattribute


print ("checker middle, top mip, channels: 1 =", texture_sys.texture(checker, texture_opt, 0.5, 0.5, 0, 0, 0, 0, 1))
print ("checker middle, top mip, channels: 2 =", texture_sys.texture(checker, texture_opt, 0.5, 0.5, 0, 0, 0, 0, 2))
print ("checker middle, top mip, channels: 3 =", texture_sys.texture(checker, texture_opt, 0.5, 0.5, 0, 0, 0, 0, 3))
print ("")

print ("checker middle, mip tail, channels: 1 =", texture_sys.texture(checker, texture_opt, 0.5, 0.5, 1/1024.0, 1/1024.0, 1/1024.0, 1/1024.0, 1))
print ("")

texture_opt.missingcolor = (1.0, 2.0, 3.0, 4.0)
print ("missingcolor channels: 1 =", texture_sys.texture("", texture_opt, 0, 0, 0, 0, 0, 0, 1))
print ("missingcolor channels: 2 =", texture_sys.texture("", texture_opt, 0, 0, 0, 0, 0, 0, 2))
print ("missingcolor channels: 3 =", texture_sys.texture("", texture_opt, 0, 0, 0, 0, 0, 0, 3))
print ("missingcolor channels: 4 =", texture_sys.texture("", texture_opt, 0, 0, 0, 0, 0, 0, 4))
print ("")

texture_opt.missingcolor = None
print ("default-missingcolor =", texture_sys.texture("", texture_opt, 0, 0, 0, 0, 0, 0, 4))
print ("")

# Test if fetching the textures biggest mip via the texture system, results in the same image
texture_opt.interpmode = oiio.InterpMode.Bilinear
checker_buf = oiio.ImageBuf(checker)
render_buf = oiio.ImageBuf(checker_buf.spec())

image_pixels = [
    texture_sys.texture(checker, texture_opt, (x+0.5)/512.0, (y+0.5)/512.0, 0, 0, 0, 0, 3)
    for y in range(512)
    for x in range(512)
]
render_buf.set_pixels(render_buf.roi, numpy.array(image_pixels))
diff = oiio.ImageBufAlgo.compare(checker_buf, render_buf, 0, 0)

print("top mip pixel differences when streaming =", diff.nfail)

print ("")

# Test udim
udname = 'file.<UDIM>.tx'
(utiles, vtiles, tilenames) = texture_sys.inventory_udim(udname)
print("udim {} -> {}x{} {}".format(udname, utiles, vtiles, tilenames))


# Test getattribute() and getattributetype()
print ("getattributetype stat:image_size", texture_sys.getattributetype("stat:image_size"))
print ("getattributetype total_files", texture_sys.getattributetype("total_files"))
print ("getattributetype max_memory_MB", texture_sys.getattributetype("max_memory_MB"))
print ("getattributetype worldtocommon", texture_sys.getattributetype("worldtocommon"))

# Test getattribute(name) with the type inferred from the attribute
print ("untyped getattribute stat:image_size", texture_sys.getattribute("stat:image_size"))
print ("untyped getattribute total_files", texture_sys.getattribute("total_files"))
print ("untyped getattribute max_memory_MB", texture_sys.getattribute("max_memory_MB"))
print ("untyped getattribute worldtocommon", texture_sys.getattribute("worldtocommon"))

print("Done.")
