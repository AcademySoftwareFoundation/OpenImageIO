# unit tests for ImageSpec

import OpenImageIO as oiio
import array


plugin_path = raw_input("Enter the oiio plugin search path (probably <trunk>/dist/ARCH/lib) :\n ")
#plugin_path = "/home/dgaletic/code/oiio-trunk/dist/linux/lib"

def is_init_test():
    print "Starting ImageSpec constructors tests..."
    desc = oiio.TypeDesc()
    # test 1
    spec1 = None
    spec1 = oiio.ImageSpec() # the default arg is TypeDesc::UNKNOWN
    if spec1 != None:
        print "Test 1 passed"
    else:
        print "Test 1 failed"

    # test 2 - give it a typedesc!
    spec2 = None
    try:
        spec2 = oiio.ImageSpec(desc)
        if spec2 != None:
            print "Test 2 passed"
        else:
            print "Test 2 failed (returns None)"
    except:
        print "Test 2 failed (raises argument mismatch exception)" 
    
    # test 3
    spec3 = None
    try:
        spec3 = oiio.ImageSpec(800, 600, 3, desc)
        if spec3 != None:
            print "Test 3 passed"
        else:
            print "Test 3 failed (returns None)"
    except:
        print "Test 3 failed (raises argument mismatch exception)"
        
    # test 4    
    test4 = "failed"
    a = "not a proper argument"
    try:
        spec4 = oiio.ImageSpec(a)
    except:
        test4 = "passed"    
    print "Test 4", test4

    print



def is_data_members_test():
    print "Starting tests of various ImageSpec data members..."
    spec = oiio.ImageSpec()
    # test 1
    if spec.height != 0:
        print "Test 1 failed"
    else:
        print "Test 1 passed"
    # test 2
    if spec.width != 0:
        print "Test 2 failed"
    else:
        print "Test 2 passed"


    # test 3
    if spec.x != 0:
        print "Test 3 failed"
    else:
        print "Test 3 passed"
    # test 4
    if spec.y != 0:
        print "Test 4 failed"
    else:
        print "Test 4 passed"
    # test 5
    if spec.z != 0:
        print "Test 5 failed"
    else:
        print "Test 5 passed"


    # test 6
    if spec.full_x != 0:
        print "Test 6 failed"
    else:
        print "Test 6 passed"
    # test 7
    if spec.full_y != 0:
        print "Test 7 failed"
    else:
        print "Test 7 passed"
    # test 8
    if spec.full_z != 0:
        print "Test 8 failed"
    else:
        print "Test 8 passed"
    
    # test 9
    if spec.full_width != 0:
        print "Test 9 failed"
    else:
        print "Test 9 passed"
    # test 10
    if spec.full_height != 0:
        print "Test 10 failed"
    else:
        print "Test 10 passed"
    # test 11
    if spec.full_depth != 0:
        print "Test 11 failed"
    else:
        print "Test 11 passed"


    # test 12
    if spec.tile_width != 0:
        print "Test 12 failed"
    else:
        print "Test 12 passed"
    # test 13
    if spec.tile_height != 0:
        print "Test 13 failed"
    else:
        print "Test 13 passed"
    # test 14
    if spec.tile_depth != 1:
        print "Test 14 failed"
    else:
        print "Test 14 passed"


    # test 15
    if spec.alpha_channel != -1:
        print "Test 15 failed"
    else:
        print "Test 15 passed"
    # test 16 
    if spec.z_channel != -1:
        print "Test 16 failed"
    else:
        print "Test 16 passed"


    # test 17
    if spec.gamma != 1.0:
        print "Test 17 failed"
    else:
        print "Test 17 passed"

    # test 18
    if spec.quant_black != 0:
        print "Test 18 failed"
    else:
        print "Test 18 passed"
    # test 19
    if spec.quant_white != 255:
        print "Test 19 failed"
    else:
        print "Test 19 passed"
    # test 20
    if spec.quant_min != 0:
        print "Test 20 failed"
    else:
        print "Test 20 passed"
    # test 21
    if spec.quant_max != 255:
        print "Test 22 failed"
    else:
        print "Test 21 passed"

    # test 23
    if spec.nchannels != 0:
        print "Test 23 failed"
    else:
        print "Test 23 passed"

    # test 24
    test24 = ""
    try:
        assert spec.channelnames == ()
        spec.channelnames = ('c', 'm', 'y', 'k')
        assert spec.channelnames == ('c', 'm', 'y', 'k')
        test24 = "passed"
    except:
        test24 = "failed"
    print "Test 24", test24

    print



def is_default_channel_names_test():
    print "Starting ImageSpec::default_channel_names() tests..."
    spec = oiio.ImageSpec()
    # test 1
    test1 = "passed"
    try:
        spec.default_channel_names()
        assert spec.channelnames == ()
    except:
        test1 = "failed"
    print "Test 1", test1
    # test 2
    a = "not a proper argument"
    test2 = "failed"
    try:
        spec.default_channel_names_test(a)
    except:
        test2 = "passed (wrong argument raised exception)"

    print



def is_set_format_test():
    print "Starting ImageSpec::set_format() tests..."
    desc = oiio.TypeDesc(oiio.BASETYPE.UNKNOWN, oiio.AGGREGATE.SCALAR, oiio.VECSEMANTICS.NOXFORM)
    desc2 = oiio.TypeDesc(oiio.BASETYPE.UINT8, oiio.AGGREGATE.VEC2, oiio.VECSEMANTICS.COLOR)
    spec = oiio.ImageSpec()
    #test1 (uses a simple TypeDesc as an argument)
    try:
        spec.set_format(desc)
        print "Test 1 passed"
    except:
        print "Test 1 failed"
    # test 2 (uses another TypeDesc as an argument) 
    try:
        spec.set_format(desc2)
        print "Test 2 passed"
    except:
        print "Test 2 failed"
    # test 3
    a = "not a proper argument"
    try:
        spec.set_format(a)
        print "Test 3 failed (works with a wrong argument)"
    except:
        print "Test 3 passed"

    print


def is_format_from_quantize_test():
    print "Starting ImageSpec::format_from_quantize tests..."    
    spec = oiio.ImageSpec()    
    #test 1 (calls without the class instance)
    try:
        desc = oiio.ImageSpec.format_from_quantize(0, 255, 0, 255)
        print "Test 1 passed"
    except:
        print "Test 1 failed"
    # test 2 (calls from instance)
    try:
        desc = spec.format_from_quantize(0, 255, 0, 255)
        print "Test 2 passed"
    except:
        print "Test 2 failed"
    # test 3 (call with wrong arguments)
    a = "not a proper argument"
    try:
        desc = spec.format_from_quantize(a)
        print "Test 3 failed (accepts wrong arguments)"
    except:
        print "Test 3 passed"

    print


def is_channel_bytes_test():
    print "Starting ImageSpec::channel_bytes tests..."
    spec = oiio.ImageSpec()
    # test 1
    if spec.image_bytes() == 0:
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    # test 2 (call with wrong argument)
    a = "not a proper argument"
    try:
        spec.image_bytes(a)
        print "Test 2 failed"
    except:
        print "Test 2 passed"

    print


def is_pixel_bytes_test():
    print "Starting ImageSpec::pixel_bytes tests..."
    spec = oiio.ImageSpec()
    # test 1
    if spec.pixel_bytes() == 0:
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    # test 2 (call with wrong argument)
    a = "not a proper argument"
    try:
        spec.pixel_bytes(a)
        print "Test 2 failed"
    except:
        print "Test 2 passed"

    print


def is_scanline_bytes_test():
    print "Starting ImageSpec::scanline_bytes tests..."
    spec = oiio.ImageSpec()
    # test 1
    if spec.scanline_bytes() == 0:
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    # test 2 (call with wrong argument)
    a = "not a proper argument"
    try:
        spec.scanline_bytes(a)
        print "Test 2 failed"
    except:
        print "Test 2 passed"

    print


def is_tile_bytes_test():
    print "Starting ImageSpec::tile_bytes tests..."
    spec = oiio.ImageSpec()
    # test 1
    if spec.tile_bytes() == 0:
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    # test 2 (call with wrong argument)
    a = "not a proper argument"
    try:
        spec.tile_bytes(a)
        print "Test 2 failed"
    except:
        print "Test 2 passed"

    print


def is_image_bytes_test():
    print "Starting ImageSpec::image_bytes tests..."
    spec = oiio.ImageSpec()
    # test 1
    if spec.image_bytes() == 0:
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    # test 2 (call with wrong argument)
    a = "not a proper argument"
    try:
        spec.image_bytes(a)
        print "Test 2 failed"
    except:
        print "Test 2 passed"

    print


def is_tile_pixels_test():
    print "Starting ImageSpec::tile_pixels tests..."
    spec = oiio.ImageSpec()
    # test 1
    if spec.tile_pixels() == 0:
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    # test 2 (call with wrong argument)
    a = "not a proper argument"
    try:
        spec.tile_pixels(a)
        print "Test 2 failed"
    except:
        print "Test 2 passed"

    print


def is_image_pixels_test():
    print "Starting ImageSpec::image_pixels tests..."
    spec = oiio.ImageSpec()
    # test 1
    if spec.image_pixels() == 0:
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    # test 2 (call with wrong() argument)
    a = "not a proper argument"
    try:
        spec.image_pixels(a)
        print "Test 2 failed"
    except:
        print "Test 2 passed"

    print

def is_size_t_safe_test():
    print "Running ImageSpec::size_t_safe tests..."
    spec = oiio.ImageSpec() 
    # test 1
    if spec.size_t_safe():
        print "Test 1 passed"
    else:
        print "Test 1 failed"

    print


# note: auto_stride() is a static method
def is_auto_stride_test():
    print "Running ImageSpec::auto_stride() tests..."
    # note: tests 1, 2 and 3 fail because of undetermined argument mismatch
    # test 1
    # we'll give it real image format data
    spec1 = oiio.ImageSpec()
    spec2 = oiio.ImageSpec()
    inp = oiio.ImageInput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    inp.open("../../../oiio-images/tahoe-gps.jpg", spec1)
        
    try:
        oiio.ImageSpec.auto_stride(spec1.format, spec1.nchannels, spec1.width, spec1.height)
        print "Test 1 passed"
    except:
        print "Test 1 failed (argument mismatch exception raised)"

    # test 2
    try:
        oiio.ImageSpec.auto_stride(spec1.format, spec1.nchannels)
        print "Test 2 passed"
    except:
        print "Test 2 failed (argument mismatch exception raised)"

    # test 3 (called from the ImageSpec instance)
    try:
        spec1.auto_stride(spec2.format, spec2.nchannels, spec2.width, spec2.height)
        print "Test 3 passed"
    except:
        print "Test 3 failed (argument mismatch exception raised)"
    
    # test 4 (wrong argument given)
    a = "not a proper argument"    
    try:
        spec.auto_stride(a, spec2.format, spec2.nchannels, spec2.width, spec2.height)
        print "Test 4 failed"
    except:
        print "Test 4 passed"
    
    # test 5 (an impossible value of -5 for nchannels given)
    # note: this test will pass because an argument mismatch exception is caught
    # however, we're actually interested in what will happen when the problem
    # causing the mismatch in tests 1, 2 and 3 is solved
    try:
        spec.auto_stride(spec2.format, -5, spec2.width, spec2.height)
        print "Test 5 failed"
    except:
        print "Test 5 passed"

    print

    
def is_attribute_test():
    print "Running ImageSpec::attribute tests..."
    spec = oiio.ImageSpec() 
    # test 1
    try:
        spec.attribute("test_attribute_int", 1)
        print "Test 1 passed"
    except:
        print "Test 1 failed (exception raised)"
    # test 2
    try:
        spec.attribute("test_attribute_float", 3.14)
        print "Test 2 passed"
    except:
        print "Test 2 failed (exception raised)"
    # test 3
    try:
        spec.attribute("test_attribute_string", "asd")
        print "Test 3 passed"
    except:
        print "Test 3 failed (exception raised)"
    # test 4
    try:
        spec.attribute("test_attribute_int", "a")
        print "Test 4 passed"
    except:
        print "Test 4 failed (exception raised)"
    # test 5 (wrong argument passed)
    a = ["not a proper argument"]
    try:
        spec.attribute(a, 1)
        print "Test 5 failed (accepted wrong argument)"
    except:
        print "Test 5 passed"
    
    print

 
# def is_find_attribute_test(): # not yet exposed to python

# I'm not sure how get_int_attribute and get_float_attribute are supposed to work
# It this supposed to be used as a converter to int from unsigned/short/byte ?
def is_get_int_attribute_test():
    print "Tests for get_int_attribute not implemented yet"
    """    
    spec = oiio.ImageSpec()
    spec.attribute("test_attribute_int", 1)
    # test 1 # 
    print spec.get_int_attribute("test_attribute_int", 0)
    """

    print

# same as with get_int_attribute
def is_get_float_attribute_test():
    print "Tests for get_float_attribute not implemented yet"
    
    print

# same as with get_int_attribute
def is_get_string_attribute_test():
    print "Tests for get_string_attribute not implemented yet"
    
    print

def is_metadata_val_test():
    print "Tests for metadata_val not implemented yet"

    print


def run_is_tests():
    is_init_test()
    is_data_members_test()
    is_default_channel_names_test()
    is_set_format_test()
    is_format_from_quantize_test()
    is_channel_bytes_test()
    is_pixel_bytes_test()
    is_scanline_bytes_test()
    is_tile_bytes_test()
    is_image_bytes_test()
    is_tile_pixels_test()
    is_image_pixels_test()
    is_size_t_safe_test()
    is_auto_stride_test()
    is_attribute_test()
    is_get_int_attribute_test()
    is_get_float_attribute_test()
    is_get_string_attribute_test()
    is_metadata_val_test()

run_is_tests()








