#!/usr/bin/env python

from __future__ import print_function
from __future__ import absolute_import
import OpenImageIO as oiio


# Print the contents of an ImageSpec
def print_imagespec (spec, msg="") :
    if msg != "" :
        print (str(msg))
    print ("  resolution (width,height,depth) = ", spec.width, spec.height, spec.depth)
    print ("  origin (x,y,z) = ", spec.x, spec.y, spec.z)
    print ("  full res = ", spec.full_width, spec.full_height, spec.full_depth)
    print ("  full origin = ", spec.full_x, spec.full_y, spec.full_z)
    if spec.tile_width :
        print ("  tile size = ", spec.tile_width, spec.tile_height, spec.tile_depth)
    else :
        print ("  untiled")
    print ("  format = ", str(spec.format))
    print ("  nchannels = ", spec.nchannels)
    print ("  channelformats = ", spec.channelformats)
    print ("  channel names = ", spec.channelnames)
    print ("  alpha channel = ", spec.alpha_channel)
    print ("  z channel = ", spec.z_channel)
    print ("  deep = ", spec.deep)
    # print ("  quantization: black=%d, white=%d, min=%d, max=%d" 
    #        % (spec.quant_black, spec.quant_white, spec.quant_min, spec.quant_max))
    for i in range(len(spec.extra_attribs)) :
        print ("  ", spec.extra_attribs[i].name, "=", spec.extra_attribs[i].value)
    print ()



######################################################################
# main test starts here

try:
    # test contstructors
    print_imagespec (oiio.ImageSpec(), "ctr: null")
    print_imagespec (oiio.ImageSpec(oiio.UINT16), "ctr: UINT16")
    print_imagespec (oiio.ImageSpec(640, 480, 3, oiio.UINT8), "ctr: 640x480,3 UINT8")

    # test setting fields
    s = oiio.ImageSpec()
    s.set_format (oiio.FLOAT)
    s.x = 1
    s.y = 2
    s.z = 3
    s.width = 640
    s.height = 480
    s.depth = 1
    s.full_x = 4
    s.full_y = 5
    s.full_z = 6
    s.full_width = 1280
    s.full_height = 960
    s.full_depth = 7
    s.tile_width = 32
    s.tile_height = 64
    s.tile_depth = 16
    s.nchannels = 5;
    s.alpha_channel = 3;
    s.z_channel = 4;
    s.default_channel_names()
    s.channelformats = (oiio.UINT8, oiio.UINT8, oiio.UINT8, oiio.UINT8, oiio.FLOAT)
    print_imagespec (s, "lots of fields")
    print (" B channel =", s.channelindex("B"))
    print (" foo channel =", s.channelindex("foo"))

    print ("channel bytes =", s.channel_bytes())
    print ("  channel_bytes(1) =", s.channel_bytes(1), "native", s.channel_bytes(1,True))
    print ("  channel_bytes(4) =", s.channel_bytes(4), "native", s.channel_bytes(4,True))
    print ("pixel bytes =", s.pixel_bytes(), "native", s.pixel_bytes(True))
    print ("scanline bytes =", s.scanline_bytes(), "native", s.scanline_bytes(True))
    print ("tile bytes =", s.tile_bytes(), "native", s.tile_bytes(True))
    print ("image bytes =", s.image_bytes(), "native", s.image_bytes(True))
    print ("tile pixels =", s.tile_pixels())
    print ("image_pixels =", s.image_pixels())
    print ("size_t_safe =", s.size_t_safe())
    print ("get_channelformats:", s.get_channelformats())
    print ("channelindex('G') =", s.channelindex('G'))
    print ("channel_name(1) =", s.channel_name(1))
    print ("channelformat(1) =", s.channelformat(1))
    print ()

    s.attribute ("foo_str", "blah")
    s.attribute ("foo_int", 14)
    s.attribute ("foo_float", 3.14)
    s.attribute ("foo_vector", oiio.TypeDesc.TypeVector, (1, 0, 11))
    s.attribute ("foo_matrix", oiio.TypeDesc.TypeMatrix,
                 (1, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 1, 2, 3, 1))
    s.attribute ("smpte:TimeCode", oiio.TypeDesc.TypeTimeCode, (18356486, 4294967295))
    s["delfoo_str"] =  "egg"
    s["delfoo_int"] = 29
    s["delfoo_float"] = 99.5
    print ("get_int_attribute('foo_int') retrieves", s.get_int_attribute ("foo_int"))
    print ("get_int_attribute('foo_int',21) with default retrieves", s.get_int_attribute ("foo_int", 21))
    print ("get_int_attribute('foo_no',23) retrieves", s.get_int_attribute ("foo_no", 23))
    print ("get_float_attribute('foo_float') retrieves", s.get_float_attribute ("foo_float"))
    print ("get_float_attribute('foo_float_no') retrieves", s.get_float_attribute ("foo_float_no"))
    print ("get_float_attribute('foo_float_no',2.7) retrieves", s.get_float_attribute ("foo_float_no", 2.7))
    print ("get_string_attribute('foo_str') retrieves", s.get_string_attribute ("foo_str"))
    print ("get_string_attribute('foo_str_no') retrieves", s.get_string_attribute ("foo_str_no"))
    print ("get_string_attribute('foo_str_no','xx') retrieves", s.get_string_attribute ("foo_str_no", "xx"))
    print ()
    print ("getattribute('foo_int') retrieves", s.getattribute("foo_int"))
    print ("getattribute('foo_float') retrieves", s.getattribute("foo_float"))
    print ("getattribute('foo_str') retrieves", s.getattribute("foo_str"))
    print ("getattribute('foo_vector') retrieves", s.getattribute("foo_vector"))
    print ("getattribute('foo_matrix') retrieves", s.getattribute("foo_matrix"))
    print ("getattribute('foo_no') retrieves", s.getattribute("foo_no"))
    print ("getattribute('smpte:TimeCode') retrieves", s.getattribute("smpte:TimeCode"))
    print ("s['delfoo_float'] =", s['delfoo_float'])
    print ("s['delfoo_int'] =", s['delfoo_int'])
    print ("s['delfoo_str'] =", s['delfoo_str'])
    print ()

    print ("extra_attribs size is", len(s.extra_attribs))
    for i in range(len(s.extra_attribs)) :
        print (i, s.extra_attribs[i].name, s.extra_attribs[i].type, s.extra_attribs[i].value)
        print (s.metadata_val (s.extra_attribs[i], True))
    print ()
    print ("seralize(xml):")
    print (s.serialize("xml"))
    print ("serialize(text, human):")
    print (s.serialize("text", "detailedhuman"))
    print ()

    s.attribute("dog", "Spot")
    print ("Added dog: ", s.getattribute("dog"))
    s.erase_attribute("dog")
    print ("After erasing dog, dog = ", s.getattribute("dog"))
    print()

    # test initialization from ROI
    print ("Testing construction from ROI:")
    sroi = oiio.ImageSpec (oiio.ROI(0,640,0,480,0,1,0,3), oiio.FLOAT);
    print_imagespec (sroi)

    # Also test global OIIO functions here
    print ("\nTesting global attribute store/retrieve:")
    oiio.attribute ("plugin_searchpath", "perfect")
    print ("get_string_attribute plugin_searchpath : ", oiio.get_string_attribute ("plugin_searchpath", "bad"))
    print ("get_int_attribute plugin_searchpath : ", oiio.get_int_attribute ("plugin_searchpath", 0))
    print ("getattribute TypeString plugin_searchpath : ", oiio.getattribute ("plugin_searchpath", oiio.TypeDesc.TypeString))
    print ("getattribute TypeFloat plugin_searchpath : ", oiio.getattribute ("plugin_searchpath", oiio.TypeDesc.TypeFloat))
    print ("getattribute TypeString blahblah : ", oiio.getattribute ("blahblah", oiio.TypeDesc.TypeString))

    print ("Done.")
except Exception as detail:
    print ("Unknown exception:", detail)

