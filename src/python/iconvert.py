#!/usr/bin/env python
# Parse the options the user provided from the command line
def option_parser():
    parser = OptionParser()
    parser.add_option("-v", action="store_true", dest="verbose", default=False)
    parser.add_option("--inplace", action="store_true", dest="inplace", default=False)
    parser.add_option("-d", dest="data_format_name", default="")
    parser.add_option("--sRGB", action="store_true", dest="sRGB", default=False)
    parser.add_option("--tile", nargs=3, dest="tile")
    parser.add_option("--scanline", action="store_true", dest="scanline", default=False)
    parser.add_option("--separate", action="store_true", dest="separate", default=False)
    parser.add_option("--contig", action="store_true", dest="contig", default=False)
    parser.add_option("--compression", dest="compression")
    parser.add_option("--quality", type="int", dest="quality", default = -1)
    parser.add_option("--no-copy-image", action="store_true", dest="no_copy", default=False)
    parser.add_option("--adjust-time", action="store_true", dest="adjust_time", default=False)
    parser.add_option("--caption", dest="caption", default=None)
    parser.add_option("-k", "--keyword", action="append", dest="keywords")
    parser.add_option("--clear-keywords", action="store_true", default=False)
    parser.add_option("--attrib", nargs=2, action="append", dest="attribs")
    parser.add_option("--orientation", type="int", dest="orientation", default = 0)
    parser.add_option("--rotcw", action="store_true", dest="rotcw", default=False)
    parser.add_option("--rotccw", action="store_true", dest="rotccw", default=False)
    parser.add_option("--rot180", action="store_true", dest="rot180", default=False)
    parser.add_option("--plugin-path", dest="path", default="")
    # FIXME: I suppose there should be a way to enter input/output files without
    # having to specify an option, like "python iconvert.py -g 0.9 input.jpg output.jpg"
    # However, I could not find it in the docs so I've set it that the user has
    # to put -i and -o before input/output.
    parser.add_option("-i", action="append", dest="input_files", default=[])
    parser.add_option("-o", action="append", dest="output_files", default=[])

    (options, args) = parser.parse_args()
     
    if len(options.input_files) > len(options.output_files) and not options.inplace:
        print "Must have both an input and output filename specified"
        return False

    if len(options.input_files) == 0 and options.inplace:
        print "Must have at least one filename specified"
        return False

    if (int(options.rotcw) + int(options.rotccw) + int(options.rot180) + \
    (options.orientation>0)) > 1:
        print "iconvert: more than one of --rotcw, --rotccw, --rot180, --orientation"
        return False
    
    if options.path == "":
        print "OIIO plugin path not provided, assuming \"\""    

    return parser.parse_args()


def convert_files(in_file, out_file):
    nocopy = options.no_copy
    tempname = out_file
    # Check whether the conversion is inplace.
    if tempname == in_file:
        try: 
            ext = out_file.rfind(".")
            tempname += ".tmp" + out_file[ext:]
        except:
            print "Error: Output file does not have an extension"
            
    
    # image input
    inp = oiio.ImageInput.create(in_file, options.path)
    if not inp:
        msg = "Could not crete ImageInput for " + in_file
        sys.exit(msg)

    inspec = oiio.ImageSpec()    
    inp.open(in_file, inspec)

    # image output
    out = oiio.ImageOutput.create(tempname, options.path)
    if not out:
        msg = "Unable to create ImageOutput for " + out_file
        sys.exit(msg)

    # adjust spec
    outspec = inspec
    nocopy = adjust_spec(inp, inspec, outspec)
    out.open(tempname, outspec, oiio.ImageOutputOpenMode.Create)    

    # convert
    if nocopy == False:
        ok = out.copy_image(inp)
        if not ok:
            print "Error"
    else:
        arr = array.array("B", "\0" * inspec.image_bytes())
        ok = inp.read_image(outspec.format, arr)
        if not ok:
            print "Error reading"
        else:
            ok = out.write_image(outspec.format, arr)
            if not ok:
                print "Error writing"
    out.close()
    inp.close()

    # if the conversion was --inplace, this will result to True
    if out_file != tempname:
        if ok:
            # since it was inplace, in_file == out_file
            # so we need to replace the original file with tempfile
            os.remove(out_file)
            os.rename(tempname, out_file)
        else:
            os.remove(tempname)
            


def adjust_spec(inp, inspec, outspec):
    nocopy = options.no_copy
    # the following line is from the original iconvert, but I'm not sure
    # it is needed. It's already outspec = inspec, right?
    #outspec.set_format(inspec.format)
    if options.data_format_name != "":
        if data_format_name == "uint8":
            outspec.set_format(oiio.BASETYPE.UINT8)
        elif data_format_name == "int8":
            outspec.set_format(oiio.BASETYPE.INT8)
        elif data_format_name == "uint16":
            outspec.set_format(oiio.BASETYPE.UINT16)
        elif data_format_name == "int16":
            outspec.set_format(oiio.BASETYPE.INT16)
        elif data_format_name == "half":
            outspec.set_format(oiio.BASETYPE.HALF)            
        elif data_format_name == "float":
            outspec.set_format(oiio.BASETYPE.FLOAT)
        elif data_format_name == "double":
            outspec.set_format(oiio.BASETYPE.DOUBLE)
        if outspec.format != inspec.format:
            nocopy = True

    
    if options.sRGB:
        outspec.linearity = oiio.sRGB
        
    #ImageSpec.find_attribute() is not exposed to Python
    #if inp.format_name() != "jpeg" or outspec.find_attribute("Exif:ColorSpace"):
        #outspec.attribute("Exif:ColorSpace", 1)

    # handling tiles is not exposed to Python
    if options.tile:
        outspec.tile_width = options.tile[0]
        outspec.tile_height = options.tile[1]
        outspec.tile_depth = options.tile[2]

    if options.scanline:
        outspec.tile_width = 0
        outspec.tile_height = 0
        outspec.tile_depth = 0

    if outspec.tile_width != inspec.tile_width or \
    outspec.tile_height != inspec.tile_height or \
    outspec.tile_depth != inspec.tile_depth:
        nocopy = True

    if options.compression:
        outspec.attribute("compression", options.compression)
        # 2nd argument should be exposed as default
        if options.compression != inspec.get_string_attribute("compression", ""):
            nocopy = True
            
    # FIXME: If quality is provided, the resultig image is larger than the
    # input image, and it is always the same no matter what quality (1-100).
    # (I suppose it uses the maximum possible value)
    # Should a --compression method be provided if --quality is used?
    if options.quality > 0:
        outspec.attribute("CompressionQuality", options.quality)        
        # the 2nd argument should be exposed as default (in ImageSpec wrapper)
        # FIXME: The default arg is supposed to be 0, and get_int_attribute always
        # returns whatever is provided as the 2nd argument - 0 in this case.
        # I can't find out what's wrong in the binding.
        if options.quality != inspec.get_int_attribute("CompressionQuality", 0):
            nocopy = True
            
    if options.contig:
        outspec.attribute("planarconfig", "contig")
    if options.separate:
        outspec.attribute("planarconfig", "separate")

    if options.orientation >= 1:
        outspec.attribute("Orientation", options.orientation)
    else:
        orientation = outspec.get_int_attribute("Orientation", 1)
        if orientation >= 1 and orientation <= 8:
            cw = [0, 6, 7, 8, 5, 2, 3, 4, 1]
            if options.rotcw or options.rotccw or options.rot180:
                orientation = cw[orientation]
            if options.rotcw or options.rot180:
                orientation = cw[orientation]
            if options.rotccw:
                orientation = cw[orientation]
            outspec.attribute("Orientation", orientation)

    if options.caption != None:
        outspec.attribute("ImageDescription", options.caption)

    if options.clear_keywords == True:
        outspec.attribute("Keywords", "")

    # this looks a lot simpler than in c++ :)
    if options.keywords != None:
        oldkw = outspec.get_string_attribute("Keywords", "")
        newkw = oldkw
        for keyword in options.keywords:
            newkw += "; " + keyword
        outspec.attribute("Keywords", newkw)
                
    if options.attribs:
        for i in options.attribs:
            outspec.attribute(i[0], i[1])
    return nocopy
        
    

# main
import OpenImageIO as oiio
import array
from optparse import OptionParser
import os
import sys

(options, args) = option_parser()   

if options.inplace:
    for image in options.input_files:
        if convert_files(image, image) == False:
            sys.exit("Conversion failed")
else:
    for i in range(len(options.input_files)):
        if convert_files(options.input_files[i], options.output_files[i]) == False:
            sys.exit("Conversion failed")
                        



