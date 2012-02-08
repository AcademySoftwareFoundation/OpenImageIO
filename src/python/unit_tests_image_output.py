# unit tests for Python bindings: ImageOutput

import OpenImageIO as oiio
import array

def callback(fl):
    pass

print "Running this test supposes basic ImageInput tests passed"
print
 
plugin_path = raw_input("Enter the oiio plugin search path (probably <trunk>/dist/ARCH/lib) :\n")

#plugin_path = "/home/dgaletic/code/oiio-trunk/dist/linux/lib"


def io_create_test():
    print "Running ImageOutput::create() tests..."
    spec = oiio.ImageSpec() 
    # test 1
    out1 = oiio.ImageOutput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    if out1 == None:
        print "Test 1 failed: check your plugin path and whether you ran the script from <trunk>/src/python. The oiio-images folder is supposed to be in the same folder as <trunk>. \n This error will cause all the write_<something> tests to fail."
    else:
        print "Test 1 passed"
    # test 2
    out2 = oiio.ImageOutput.create("", plugin_path)
    if out2 == None:    
        print "Test 2 passed"
    else:
        print "Test 2 error"        
    # test 3
    test3 = "failed"
    a = 42 # not a proper argument for create()
    try:
        out3 = oiio.ImageOutput.create(a , plugin_path)
    except:
        test3 = "passed"
    print "Test 3", test3
    # test 4
    out4 = oiio.ImageOutput.create(plugin_path)
    if out4 == None:
        print "Test 4 passed"
    else:
        print "Test 4 failed"

    print



def io_spec_test():
    print "Running ImageOutput::spec() tests..."
    out_s = oiio.ImageOutput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    if out_s == None:
        print "Can't opet test image, skipping spec() tests"
        print
        return
    # test 1
    test1 = "passed"
    try:
        spec = out_s.spec()
    except:
        test1 = "failed"
    print "Test 1", test1
    # test 2
    test2 = "failed"
    a = "this can't be used as an argument"
    try:
        spec = out_s.spec(a)
    except:
        test2 = "passed"
    print "Test 2", test2    
    
    print



# spec_o is an ImageSpec instance based on the test image
# Every test of the open() method will create an empty black image since
# open() creates the image, but we don't write to it at this point.
def io_open_test(spec_o):
    print "Running ImageOutput::open() tests..."
    out_o = oiio.ImageOutput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    if out_o == None:
        print "Can't opet test image, skipping open() tests"
        print
        return 
    
    # test 1
    if (out_o.open("../../../oiio-images/tahoe-gps-test.jpg", spec_o, oiio.ImageOutputOpenMode.Create)):
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    # test 2
    if (not out_o.open("", spec_o, oiio.ImageOutputOpenMode.Create)):
        print "Test 2 passed"
    else:
        print "Test 2 failed"
    # test 3
    test3 = "failed"
    try:
        out_o.open(spec_o)
    except:
        test3 = "passed"
    print "Test 3", test3

    # test 4 - same as test 1, but the "append" argument is set to True. 
    # Not sure how it's supposed to be used, though.
    if (out_o.open("../../../oiio-images/tahoe-gps-test.jpg", spec_o, oiio.ImageOutputOpenMode.AppendSubimage)):
        print "Test 4 passed"
    else:
        print "Test 4 failed"

    print



def io_close_test(spec_c):
    print "Running ImageOutput::close() tests..."
    out_cl = oiio.ImageOutput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)

    # The created image will be black since no writing is done at this point.
    out_cl.open("../../../oiio-images/tahoe-gps-test.jpg", spec_c, oiio.ImageOutputOpenMode.Create)
    if out_cl == None:
        print "Can't opet test image, skipping close() tests"
        print
        return
    # test 1 
    if (out_cl.close()):
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    # test 2
    test2 = "failed"
    a = "this can't be used as an argument"
    try:
        out_cl.close(a)
    except:
        test2 = "passed"
    print "Test 2", test2
        
    print


def io_write_image_test(arr, spec_wi):    
    print "Running ImageOutput::write_image() tests..."
    out_wi = oiio.ImageOutput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    out_wi.open("../../../oiio-images/tahoe-gps-test1.jpg", spec_wi, False)
    if out_wi == None:
        print "Can't opet test image, skipping write_image() tests"
        print
        return

    # test 1 - write the data which was passed, no modifications
    if (out_wi.write_image(spec_wi.format, arr)):
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    out_wi.close()    

    # test 2 - brighten the pixels by +30, up to 255
    out_wi.open("../../../oiio-images/tahoe-gps-test2-brighten.jpg", spec_wi, False)
    arr2 = arr[:]
    for i in range(len(arr2)):
        if arr2[i] < 225: arr2[i] += 30
        else: arr2[i] = 255
    if (out_wi.write_image(spec_wi.format, arr2)):
        print "Test 2 passed"
    else:
        print "Test 2 failed"
    out_wi.close()

    # test 3 - wrong argument passed
    # the created image is supposed to be black (no writing)
    test3 = "failed"
    a = "this is not a proper argument"
    out_wi.open("../../../oiio-images/tahoe-gps-test3.jpg", spec_wi, False)        
    try:
        out_wi.write_image(a, arr)    
    except:
        test3 = "passed"
    print "Test 3", test3
    out_wi.close()

    # test 4 - the data buffer is read backwards
    out_wi.open("../../../oiio-images/tahoe-gps-test4.jpg", spec_wi, False)
    arr4 = arr[::-1]
    if out_wi.write_image(spec_wi.format, arr4):
        print "Test 4 passed"
    else:
        print "Test 4 failed"
    out_wi.close()

    # test 5
    out_wi.open("../../../oiio-images/tahoe-gps-test5-strides.jpg", spec_wi, False)
    if out_wi.write_image(spec_wi.format, arr, oiio.AutoStride, oiio.AutoStride, oiio.AutoStride, None):
        print "Test 5 passed"
    else:
        print "Test 5 failed"
    out_wi.close()
    
    # test 6
    out_wi.open("../../../oiio-images/tahoe-gps-test6-callback.jpg", spec_wi, False)
    if out_wi.write_image(spec_wi.format, arr, oiio.AutoStride, oiio.AutoStride, oiio.AutoStride, callback):
        print "Test 6 passed"
    else:
        print "Test 6 failed"
    out_wi.close()
    
    # test 7    
    # issues with AutoStride are the same as in ImageInput
    
    # TODO: a test where an array with insufficient amount of data is passed to
    # the write_image() method.
    print
    

def io_copy_image_test(image_input, spec_ci):
    print "Running ImageOutput::copy_image() tests..."
    out_ci = oiio.ImageOutput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    out_ci.open("../../../oiio-images/tahoe-gps-copy.jpg", spec_ci, False)
    if out_ci == None:
        print "Can't opet test image, skipping write_image() tests"
        print
        return

    # test 1    
    if out_ci.copy_image(image_input): 
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    # test 2
    test2 = "failed"
    try:
        out_ci.copy_image()
    except:
        test2 = "passed"
    print "Test 2", test2
    out_ci.close()

    print


def io_format_name_test(spec_fn):
    print "Running ImageOutput::format_name() tests..."
    out_fn = oiio.ImageOutput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    out_fn.open("../../../oiio-images/tahoe-gps-fn.jpg", spec_fn, False)
    # test 1
    if out_fn.format_name() == "jpeg":
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    out_fn.close()

    print


def io_supports_test(spec_su):
    print "Running ImageOutput::supports() tests..."
    out_su = oiio.ImageOutput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    out_su.open("../../../oiio-images/tahoe-gps-fn.jpg", spec_su, False)
    # test 1
    if (not out_su.supports("tiles")):
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    out_su.close()

    print


def io_error_message_test(spec_em):
    print "Running ImageOutput::error_message() tests..."
    out_em = oiio.ImageOutput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    out_em.open("../../../oiio-images/tahoe-gps-fn.jpg", spec_em, False)
    # test 1
    test1 = "passed"
    try:
        out_em.error_message()
    except:
        test1 = "failed"
    print "Test 1", test1
    out_em.close()

    print


# this test has its own ImageInput instance so it can read scanlines on its own
def io_write_scanline_test():
    print "Running ImageInput::write_scanline() tests..."

    # create II instance and open a file
    spec_ws = oiio.ImageSpec()
    pic_ws = oiio.ImageInput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    pic_ws.open("../../../oiio-images/tahoe-gps.jpg", spec_ws)
    
    #create IO instance and open a file for writing
    out_ws = oiio.ImageOutput.create("../../../oiio-images/tahoe-gps-scanline.jpg", plugin_path)
    out_ws.open("../../../oiio-images/tahoe-gps-scanline.jpg", spec_ws, False)
    desc_ws = spec_ws.format
    
    # test 1 (reads each scanline and writes it to a new file)
    # this tests several things, so it's quite good if it passed
    try:
        for i in range(spec_ws.height):
            arr_ws = array.array("B", "\0" * spec_ws.scanline_bytes())
            pic_ws.read_scanline(i, 0, desc_ws, arr_ws)
            out_ws.write_scanline(i, 0, desc_ws, arr_ws)
        print "Test 1 passed"
    except:
        print "Test 1 failed (raised exception)"
    out_ws.close()    

    # test 2 (same as test 1, but will pass a nonexisting coordinate)    
    try:
        for i in range(spec_ws.height):
            arr_ws = array.array("B", "\0" * spec_ws.scanline_bytes())
            pic_ws.read_scanline(i, 0, desc_ws, arr_ws)
            out_ws.write_scanline(i, 0, desc_ws, arr_ws)
        print "Test 1 passed"
    except:
        print "Test 1 failed (raised exception)"
    out_ws.close()    
    
    print


# TODO: write_tile(), write_rectangle()
# note to self: ask David about tiled images


def run_io_tests():
    # first we use ImageInput to produce data ImageOutput uses
    spec = oiio.ImageSpec()
    inp = oiio.ImageInput.create("../../../oiio-images/tahoe-gps.jpg", plugin_path)
    inp.open("../../../oiio-images/tahoe-gps.jpg", spec)
    desc = spec.format
    arr = array.array("B", "\0" * spec.image_bytes(True))
    inp.read_image(desc, arr)

    io_create_test()
    io_spec_test()
    io_open_test(spec)
    io_close_test(spec)
    io_write_image_test(arr, spec) # lengthy operation
    io_copy_image_test(inp, spec)
    io_format_name_test(spec)
    io_supports_test(spec)
    io_error_message_test(spec)
    io_write_scanline_test()

run_io_tests()




