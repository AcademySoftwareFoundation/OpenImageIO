#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from __future__ import annotations

import os

import numpy

import OpenImageIO as oiio


checker = os.path.abspath ("../common/textures/checker.tx")
grid = os.path.abspath ("../common/textures/grid.tx")
udname = "file.<UDIM>.tx"





######################################################################
# main test starts here

try:
    print ("Testing Wrap/MipMode/InterpMode enums:")
    print ("  Wrap.Periodic defined:", hasattr (oiio.Wrap, "Periodic"))
    print ("  MipMode.Trilinear defined:", hasattr (oiio.MipMode, "Trilinear"))
    print ("  InterpMode.Bilinear defined:", hasattr (oiio.InterpMode, "Bilinear"))
    print ("")

    print ("Testing TextureOpt fields:")
    field_opt = oiio.TextureOpt()
    field_opt.firstchannel = 1
    field_opt.subimage = 0
    field_opt.subimagename = "density"
    field_opt.swrap = oiio.Wrap.Clamp
    field_opt.twrap = oiio.Wrap.Mirror
    field_opt.rwrap = oiio.Wrap.Black
    field_opt.mipmode = oiio.MipMode.Trilinear
    field_opt.interpmode = oiio.InterpMode.Bicubic
    field_opt.anisotropic = 8
    field_opt.conservative_filter = True
    field_opt.sblur = 0.5
    field_opt.tblur = 0.25
    field_opt.swidth = 1.0
    field_opt.twidth = 2.0
    field_opt.fill = 0.75
    field_opt.rwidth = 3.0
    field_opt.rnd = 42
    field_opt.missingcolor = (1.0, 2.0, 3.0, 4.0)
    print ("  firstchannel:", field_opt.firstchannel)
    print ("  subimagename:", field_opt.subimagename)
    print ("  swrap is Clamp:", field_opt.swrap == oiio.Wrap.Clamp)
    print ("  mipmode is Trilinear:", field_opt.mipmode == oiio.MipMode.Trilinear)
    print ("  interpmode is Bicubic:", field_opt.interpmode == oiio.InterpMode.Bicubic)
    print ("  anisotropic:", field_opt.anisotropic)
    print ("  conservative_filter:", field_opt.conservative_filter)
    print ("  missingcolor:", field_opt.missingcolor)
    field_opt.missingcolor = None  # type: ignore[assignment]
    print ("  missingcolor cleared:", field_opt.missingcolor == ())
    print ("")

    texture_sys = oiio.TextureSystem()
    texture_opt = oiio.TextureOpt()
    texture_opt.swrap = oiio.Wrap.Periodic
    texture_opt.twrap = oiio.Wrap.Periodic
    texture_opt.interpmode = oiio.InterpMode.Bilinear

    print ("checker middle, top mip, channels: 1 =",
           texture_sys.texture (checker, texture_opt, 0.5, 0.5, 0, 0, 0, 0, 1))
    print ("checker middle, top mip, channels: 2 =",
           texture_sys.texture (checker, texture_opt, 0.5, 0.5, 0, 0, 0, 0, 2))
    print ("checker middle, top mip, channels: 3 =",
           texture_sys.texture (checker, texture_opt, 0.5, 0.5, 0, 0, 0, 0, 3))
    print ("")

    print ("checker middle, mip tail, channels: 1 =",
           texture_sys.texture (checker, texture_opt, 0.5, 0.5,
                                1/1024.0, 1/1024.0, 1/1024.0, 1/1024.0, 1))
    print ("")

    print ("texture nchannels < 1 returns empty tuple:",
           texture_sys.texture (checker, texture_opt, 0.5, 0.5, 0, 0, 0, 0, 0))
    print ("")

    texture_opt.missingcolor = (1.0, 2.0, 3.0, 4.0)
    print ("missingcolor channels: 1 =",
           texture_sys.texture ("", texture_opt, 0, 0, 0, 0, 0, 0, 1))
    print ("missingcolor channels: 2 =",
           texture_sys.texture ("", texture_opt, 0, 0, 0, 0, 0, 0, 2))
    print ("missingcolor channels: 3 =",
           texture_sys.texture ("", texture_opt, 0, 0, 0, 0, 0, 0, 3))
    print ("missingcolor channels: 4 =",
           texture_sys.texture ("", texture_opt, 0, 0, 0, 0, 0, 0, 4))
    print ("")

    texture_opt.missingcolor = None  # type: ignore[assignment]
    print ("default-missingcolor =",
           texture_sys.texture ("", texture_opt, 0, 0, 0, 0, 0, 0, 4))
    print ("")

    print ("Testing texture3d:")
    tex3d = texture_sys.texture3d (checker, texture_opt,
                                   (0.5, 0.5, 0.5),
                                   (0.001, 0.0, 0.0),
                                   (0.0, 0.001, 0.0),
                                   (0.0, 0.0, 0.001),
                                   3)
    print ("  center is gray RGB:", tex3d == (0.5, 0.5, 0.5))
    print ("")

    print ("Testing environment:")
    env = texture_sys.environment (checker, texture_opt,
                                 (0.0, 0.0, 1.0),
                                 (0.001, 0.0, 0.0),
                                 (0.0, 0.001, 0.0),
                                 3)
    print ("  returns 3 channels:", len (env) == 3)
    print ("  center is gray RGB:", env == (0.5, 0.5, 0.5))
    print ("")

    # Stream the top mip through texture() and compare to ImageBuf.
    checker_buf = oiio.ImageBuf (checker)
    render_buf = oiio.ImageBuf (checker_buf.spec())
    image_pixels = [
        texture_sys.texture (checker, texture_opt,
                             (x + 0.5) / 512.0, (y + 0.5) / 512.0,
                             0, 0, 0, 0, 3)
        for y in range (512)
        for x in range (512)
    ]
    render_buf.set_pixels (render_buf.roi, numpy.array (image_pixels))
    diff = oiio.ImageBufAlgo.compare (checker_buf, render_buf, 0, 0)
    print ("top mip pixel differences when streaming =", diff.nfail)
    print ("")

    print ("Testing metadata helpers:")
    resolved = texture_sys.resolve_filename (checker)
    print ("  resolve_filename ends with checker.tx:",
           resolved.endswith ("checker.tx"))
    spec = texture_sys.imagespec (checker)
    print ("  imagespec width>0:", spec is not None and spec.width > 0)
    print ("  imagespec missing is None:",
           texture_sys.imagespec ("no_such_texture_file.tx") is None)
    print ("  is_udim checker:", texture_sys.is_udim (checker))
    print ("  is_udim pattern:", texture_sys.is_udim (udname))
    print ("")

    print ("Testing udim:")
    (utiles, vtiles, tilenames) = texture_sys.inventory_udim (udname)
    print ("  inventory tiles:", utiles, "x", vtiles)
    print ("  inventory nonempty files:", sum (1 for t in tilenames if t))
    print ("  resolve_udim nonempty:",
           len (texture_sys.resolve_udim (udname, 0.25, 0.25)) > 0)
    print ("")

    print ("Testing attribute():")
    texture_sys.attribute ("searchpath", "../common/textures")
    searchpath = texture_sys.getattribute ("searchpath", oiio.TypeString)
    print ("  searchpath contains textures:",
           isinstance (searchpath, str) and "textures" in searchpath)
    texture_sys.attribute ("max_memory_MB", oiio.TypeFloat, 512.0)
    print ("  max_memory_MB typed round-trip:",
           texture_sys.getattribute ("max_memory_MB", oiio.TypeFloat) == 512.0)
    print ("")

    print ("Testing getattribute() and getattributetype():")
    print ("  getattributetype stat:image_size:",
           texture_sys.getattributetype ("stat:image_size"))
    print ("  getattributetype total_files:",
           texture_sys.getattributetype ("total_files"))
    print ("  getattributetype max_memory_MB:",
           texture_sys.getattributetype ("max_memory_MB"))
    print ("  getattributetype worldtocommon:",
           texture_sys.getattributetype ("worldtocommon"))
    image_size = texture_sys.getattribute ("stat:image_size")
    print ("  stat:image_size is int:", isinstance (image_size, int))
    print ("  stat:image_size positive:", image_size > 0)
    print ("  total_files is int:",
           isinstance (texture_sys.getattribute ("total_files"), int))
    print ("  max_memory_MB is float:",
           isinstance (texture_sys.getattribute ("max_memory_MB"), float))
    world = texture_sys.getattribute ("worldtocommon")
    print ("  worldtocommon is 16-tuple:",
           isinstance (world, tuple) and len (world) == 16)
    print ("")

    print ("Testing cache lifecycle:")
    texture_sys.close (checker)
    texture_sys.invalidate (grid, True)
    texture_sys.invalidate_all (False)
    texture_sys.close_all()
    print ("  close/invalidate ok: True")
    print ("")

    print ("Testing TextureSystem.destroy:")
    private_ts = oiio.TextureSystem (shared=False)
    oiio.TextureSystem.destroy (private_ts)
    print ("  destroy shared=False ok: True")
    print ("")

    print ("Testing has_error/geterror:")
    err_ts = oiio.TextureSystem (shared=False)
    print ("  has_error before:", err_ts.has_error())
    err_ts.texture ("no_such_texture_file.tex", texture_opt,
                    0, 0, 0, 0, 0, 0, 1)
    print ("  has_error after bad file:", err_ts.has_error())
    err1 = err_ts.geterror (clear=False)
    err2 = err_ts.geterror (clear=False)
    print ("  geterror persists:", err1 == err2 and len (err1) > 0)
    err3 = err_ts.geterror (clear=True)
    print ("  geterror clear returns message:", err3 == err1)
    print ("  has_error after clear:", err_ts.has_error())
    print ("")

    print ("Testing getstats/reset_stats:")
    stats = texture_sys.getstats (1, True)
    print ("  getstats nonempty:", len (stats) > 0)
    texture_sys.reset_stats()
    print ("  reset_stats ok: True")
    print ("")

    print ("Done.")
except Exception as detail:
    print ("Unknown exception:", detail)
