#unit tests for Python bindings: ImageInput

import OpenImageIO as oiio
import array

def callback(fl):
    pass

plugin_path = raw_input("Enter the oiio plugin search path (probably <trunk>/dist/ARCH/lib) :\n ")
#plugin_path = "/home/dgaletic/code/oiio-trunk/dist/linux/lib"


def ii_create_test():
    print "Running ImageInput::create() tests..."
    spec = oiio.ImageSpec() #this is tested in ImageSpec test
    # test 1
    pic1 = oiio.ImageInput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    if pic1 == None:
        print "Test 1 failed: check your plugin path and whether you ran the script from <trunk>/src/python. The oiio-images folder is supposed to be in the same folder as <trunk>. \n This error will cause all the open_<something> and read_<something> tests to fail."
    else:
        print "Test 1 passed"
    # test 2
    pic2 = oiio.ImageInput.create("", plugin_path)
    if pic2 == None:    
        print "Test 2 passed"
    else:
        print "Test 2 error"        
    # test 3
    test3 = "failed"
    try:
        pic3 = oiio.ImageInput.create(3, plugin_path)
    except:
        test3 = "passed"
    print "Test 3", test3
    # test 4
    pic4 = oiio.ImageInput.create(plugin_path)
    if pic4 == None:
        print "Test 4 passed"
    else:
        print "Test 4 failed"

    print



def ii_open_test():
    print "Running ImageInput::open() tests..."
    pic_o = oiio.ImageInput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    if pic_o == None:
        print "Can't open test image, skipping open() tests"
        print
        return 
    spec_o = oiio.ImageSpec() #this is tested in ImageSpec test
    # test 1
    if (pic_o.open("../../../oiio-images/tahoe-gps.jpg", spec_o)):
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    # test 2
    if (not pic_o.open("", spec_o)):
        print "Test 2 passed"
    else:
        print "Test 2 failed"
    # test 3
    test3 = "failed"
    try:
        pic_o.open(spec_o)
    except:
        test3 = "passed"
    print "Test 3", test3

    print



def ii_open_with_config_test():
    # I don't know how config is supposed to act so I'm skipping this test for now
    print "Running ImageInput::open() (overload) tests..."
    print "Unimplemented yet"
    print
    pic_owc = oiio.ImageInput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    if pic_owc == None:
        print "Can't open test image, skipping open_with_config() tests"
        print
        return 
    spec_owc = oiio.ImageSpec()

def ii_spec_test():
    print "Running ImageInput::spec() tests..."
    pic_s = oiio.ImageInput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    if pic_s == None:
        print "Can't open test image, skipping spec() tests"
        print
        return
    # test 1
    test1 = "passed"
    try:
        spec = pic_s.spec()
    except:
        test1 = "failed"
    print "Test 1", test1
    # test 2
    test2 = "failed"
    a = "this can't be used as an argument"
    try:
        spec = pic_s.spec()
    except:
        test2 = "passed"
    print "Test 2", test2    
    
    print



def ii_close_test():
    print "Running ImageInput::close() tests..."
    pic_cl = oiio.ImageInput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    if pic_cl == None:
        print "Can't open test image, skipping close() tests"
        print
        return
    # test 1 
    if (pic_cl.close()):
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    # test 2
    test2 = "failed"
    a = "this can't be used as an argument"
    try:
        pic_cl.close(a)
    except:
        test2 = "passed"
    print "Test 2", test2
    # test 3
    
    print



def ii_current_subimage_test():
    print "Running ImageInput::current_subimage()..."
    pic_cs = oiio.ImageInput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    if pic_cs == None:
        print "Can't open test image, skipping read_image() tests"
        print
        return
    # test 1
    if pic_cs.current_subimage() == 0:
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    # this needs more tests
    print "Other tests unimplemented yet"    

    print



def ii_seek_subimage_test():
    print "Running ImageInput::seek_subimage()..."
    pic_ss = oiio.ImageInput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    if pic_ss == None:
        print "Can't open test image, skipping read_image() tests"
        print
        return
    # test 1
    spec_ss1 = oiio.ImageSpec()
    if (pic_ss.seek_subimage(0, spec_ss1)):
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    #test 2    
    spec_ss2 = oiio.ImageSpec()
    if (not pic_ss.seek_subimage(-1, spec_ss2)):
        print "Test 2 passed"
    else:
        print "Test 2 failed"
    # test 3
    test3 = "failed"
    a = "this is not a proper argument"
    try:
        pic_ss.seek_subimage(a, spec_ss2)
    except:
        test3 = "passed"
    print "Test 3", test3

    print



def ii_read_image_test():
    print "Running ImageInput::read_image()..."
    pic_ri = oiio.ImageInput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    if pic_ri == None:
        print "Can't open test image, skipping read_image() tests"
        print
        return    
    spec_ri = oiio.ImageSpec() #this is tested in ImageSpec test
    desc_ri = spec_ri.format
    pic_ri.open("../../../oiio-images/tahoe-gps.jpg", spec_ri)
    
    arr1 = array.array("B", "\0" * spec_ri.image_bytes())
    # test 1
    if (pic_ri.read_image(desc_ri, arr1)):
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    # test 2
    test2 = "failed"
    try:
        pic_ri.read_image(arr1)
    except:
        test2 = "passed"
    print "Test 2", test2
    # test 3
    autostride = spec_ri.nchannels*desc_ri.size()
    if(pic_ri.read_image(desc_ri, arr1, autostride, autostride, autostride)):
        print "Test 3 passed"
    else:
        print "Test 3 failed"
    # test 4
    if (pic_ri.read_image(desc_ri, arr1, autostride, autostride, autostride, None)):
        print "Test 4 passed"
    else:
        print "Test 4 failed"
    # test 5 - segfaults!
    """ 
    test5 = "passed"
    try:
        pic_ri.read_image(desc_ri, arr1, autostride+1)
    except:
        test5 = "failed "
    print "Test 5", test5
    """
    # test 6
    if (pic_ri.read_image(desc_ri, arr1, autostride, autostride, autostride, callback)):
        print "Test 6 passed"  
    else:
        print "Test 6 failed"
    # test 7 
    a = "not a function which can be used as callback"
    test7 = "failed"
    try:
        pic_ri.read_image(desc_ri, arr1, autostride, autostride, autostride, a)
    except:
        test7 = "passed"
    print "Test 7", test7
    # test 8 (a buffer of insufficient size is passed)
    test8 = "failed"
    arr2 = array.array("B", "\0" * (spec_ri.image_bytes() -10))
    try:
        pic_ri.read_image(desc_ri, arr2)
    except IndexError:
        test8 = "passed"

    print "Test 8", test8
    # test 9 (an object without the buffer interface is passed)
    list1 = []
    test9 = "failed"
    try:
        pic_ri.read_image(desc_ri, list1)
    except:
        test9 = "passed"
    print "Test 9", test9
    print
# TODO: read_image_simple() test

# make_write_buffer() is implicitly tested with tests in ii_read_image_test()
    

    
def ii_read_scanline_test():
    print "Starting ImageInput::read_scanline() tests..."
    pic_rs = oiio.ImageInput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    if pic_rs == None:
        print "Can't open test image, skipping read_scanline() tests"
        print
        return
    spec_rs = oiio.ImageSpec()
    pic_rs.open("../../../oiio-images/tahoe-gps.jpg", spec_rs)
    arr_rs = array.array("B", "\0" * spec_rs.scanline_bytes())
    # test 1
    if (pic_rs.read_scanline(0, 0, spec_rs.format, arr_rs)):
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    # test 2 (check if method returns False when given a non-existing scanline -1)
    if (not pic_rs.read_scanline(-1, 0, spec_rs.format, arr_rs)):
        print "Test 2 passed"
    else:
        print "Test 2 failed"
    # test 3
    test3 = "failed"
    a = "this is not a proper argument"
    try:
        pic_rs.read_scanline(a, 0, spec_rs.format, arr_rs)
    except:
        test3 = "passed"
    print "Test 3", test3
    # test 4  (a buffer of insufficient size is passed)
    test4 = "failed"
    arr_rs = array.array("B", "\0" * (spec_rs.scanline_bytes() -1))
    try:
        pic_rs.read_scanline(0, 0, spec_rs.format, arr_rs)
    except IndexError:
        test4 = "passed"
    print "Test 4", test4
       
    print



def ii_read_scanline_simple_test():
    # This method reads to continuous float pixels.
    # The wrapper is missing a check which would make sure that the opened image
    # contains float pixels. The first test segfaults when it tries to read
    # a wrong image.
    print "Starting ImageInput::read_scanline() (overload) tests..."
    pic_rss = oiio.ImageInput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    if pic_rss == None:
        print "Can't open test image, skipping read_scanline() (overload) tests"
        print
        return
    spec_rss = oiio.ImageSpec()
    pic_rss.open("../../../oiio-images/tahoe-gps.jpg", spec_rss)
    arr_rss = array.array("B", "1234" * spec_rss.scanline_bytes()) # '1234' == sizeof(float)
    # test 1
    if (pic_rss.read_scanline(0, 0, arr_rss)):
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    # test 1.1 
    # This test passes a buffer much larger than is actually needed.
    arr_rss2 = array.array("B", "\0" * spec_rss.image_bytes())
    if (pic_rss.read_scanline(0, 0, arr_rss2)):
        print "Test 1.1 passed"
    else:
        print "Test 1.1 failed"
    # test 2 (check if method returns False when given a non-existing scanline -1)
    try:
        pic_rss.read_scanline(-1, 0, arr_rss)
        print "Test 2 failed"
    except IndexError:
        print "Test 2 passed"
    # test 3
    test3 = "failed"
    a = "this is not a proper argument"
    try:
        pic_rss.read_scanline(a, 0, arr_rss)
    except:
        test3 = "passed"
    print "Test 3", test3
    # test 4  (a buffer of insufficient size is passed)
    test4 = "failed"
    arr_rss = array.array("B", "\0" * (spec_rss.scanline_bytes() -1))
    try:
        pic_rss.read_scanline(0, 0, arr_rss)
    except IndexError:
        test4 = "passed"
    print "Test 4", test4
       
    print



def ii_read_native_scanline_test():
    print "Starting ImageInput::read_native_scanline() tests..."
    pic_rns = oiio.ImageInput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    if pic_rns == None:
        print "Can't open test image, skipping read_native_scanline() tests"
        print
        return
    spec_rns = oiio.ImageSpec()
    pic_rns.open("../../../oiio-images/tahoe-gps.jpg", spec_rns)
    arr_rns = array.array("B", "\0" * spec_rns.scanline_bytes())    
    # test 1
    if (pic_rns.read_native_scanline(0, 0, arr_rns)):
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    # test 1.1 
    # This test passes a buffer much larger than is actually needed.
    arr_rns2 = array.array("B", "\0" * spec_rns.image_bytes())
    if (pic_rns.read_native_scanline(0, 0, arr_rns2)):
        print "Test 1.1 passed"
    else:
        print "Test 1.1 failed"
    # test 2 (check if method returns False when given a non-existing scanline -1)
    if (not pic_rns.read_native_scanline(-1, 0, arr_rns)):
        print "Test 2 passed"
    else:
        print "Test 2 failed"
    # test 3
    test3 = "failed"
    a = "this is not a proper argument"
    try:
        pic_rns.read_native_scanline(a, 0, arr_rns)
    except:
        test3 = "passed"
    print "Test 3", test3
    # test 4  (a buffer of insufficient size is passed)
    test4 = "failed"
    arr_rns = array.array("B", "\0" * (spec_rns.scanline_bytes() -1))
    try:
        pic_rns.read_native_scanline(0, 0, arr_rns)
    except IndexError:
        test4 = "passed"
    print "Test 4", test4
       
    print

# TODO: read_tile(), read_tile_simple(), read_native_tile()
# note to self: ask David about tiled images
    

def run_ii_tests():
    ii_create_test()
    ii_open_test()
    ii_open_with_config_test()
    ii_spec_test()
    ii_close_test()
    ii_current_subimage_test()
    ii_seek_subimage_test()
    ii_read_image_test()
    ii_read_scanline_test()
    ii_read_scanline_simple_test()
    ii_read_native_scanline_test()

run_ii_tests()
















