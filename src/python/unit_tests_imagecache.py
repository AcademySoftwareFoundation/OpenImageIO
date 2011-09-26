# unit tests for ImageCache

def ic_create_test():
    print "Starting ImageCache.create() tests..."
    print "Every other test requires these tests to pass."
    # test 1
    try:
        cache = None
        cache = oiio.ImageCache.create()
        if cache != None:
            print "Test 1 passed"
        else:
            print "Test 1 failed"
    except:
        print "Test 1 failed"

    # test 2
    try:
        cache = None
        cache = oiio.ImageCache.create(False)
        if cache != None:
            print "Test 2 passed"
        else:
            print "Test 2 failed"
    except:
        print "Test 2 failed"
    # test 3
    try:
        cache = None
        cache = oiio.ImageCache.create(True)
        if cache != None:
            print "Test 3 passed"
        else:
            print "Test 3 failed"
    except:
        print "Test 3 failed"
    # test 4
    try:
        cache = None
        cache = oiio.ImageCache.create("not a proper argument")
        if cache != None:
            print "Test 4 passed"
        else:
            print "Test 4 failed"
    except:
        print "Test 4 failed"

    print


def ic_clear_test():
    # create an instance on which we can test
    # if ic_create_test() failed, this can't be done
    cache = oiio.ImageCache.create()

    print "Starting ImageCache.clear() tests..."

    # test 1
    try:
        cache.clear("not a proper argument")
        print "Test 1 failed"
    except:
        print "Test 1 passed"

    # test 2
    try:
        cache.clear()
        print "Test 2 passed"
    except:
        print "Test 2 failed"

    print         


def ic_attribute_test():
    # create an instance on which we can test
    # if ic_create_test() failed, this can't be done
    cache = oiio.ImageCache.create()

    print "Starting ImageCache.attribute() tests..."

    # test 1
    try:
        cache.attribute("max_open_files", 2)
        print "Test 1 passed"
    except:
        print "Test 1 failed"

    # test 2
    try:
        cache.attribute("max_memory_MB", 3.14)
        print "Test 2 passed"
    except:
        print "Test 2 failed"       

    # test 3
    try:
        cache.attribute("searchpath", "")
        print "Test 3 passed"
    except:
        print "Test 3 failed"

    # test 4
    try:
        att_name = "a_string_attribute"
        att_value = "should return false"
        cache.attribute(att_name, att_value)
        print "Test 4 passed"
    except:
        print "Test 4 failed"

    print

def ic_getattribute_test():
    # create an instance on which we can test
    # if ic_create_test() failed, this can't be done
    cache = oiio.ImageCache.create()

    print "Starting ImageCache.getattribute() tests..."

    # test 1    
    try:
        val = None
        cache.getattribute("max_open_tiles", val)
        print "Test 1 passed"
    except:
        print "Test 1 failed"

    # test 2
    try:
        val = None
        cache.getattribute("max_memory_mb", val)
        print "Test 2 passed"
    except:
        print "Test 2 failed"

    # test 3
    try:
        val = None
        cache.getattribute("searchpath", val)
        print "Test 3 passed"
    except:
        print "Test 3 failed"

    # test 4
    try:
        val = None
        cache.getattribute("autotile", val)
        print "Test 4 passed"
    except:
        print "Test 4 failed"

    # test 5
    try:
        val = None
        cache.getattribute("automip", val)
        print "Test 5 passed"
    except:
        print "Test 5 failed"      

    # test 6
    try:
        val = None
        cache.getattribute("some_attribute", val)
        print "Test 6 passed"
    except:
        print "Test 6 failed"    
        
    print


def ic_resolve_filename_test():
    # create an instance on which we can test
    # if ic_create_test() failed, this can't be done
    cache = oiio.ImageCache.create()

    print "Starting ImageCache.resolve_filename() tests..."

    # test 1    
    pic_path = "../../../oiio-images/tahoe-gps.jpg"
    try:
        cache.resolve_filename(pic_path)
        print "Test 1 passed"
    except:
        print "Test 1 failed"

    # test 2
    try:
        cache.resolve_filename("nonexisting_pic.jpg")
        print "Test 2 passed"
    except:
        print "Test 2 failed"

    print


def ic_get_image_info_test():
    # create an instance on which we can test
    # if ic_create_test() failed, this can't be done
    cache = oiio.ImageCache.create()
    
    print "Starting ImageCache.get_image_info() tests..."
    print "Needs more tests!"

    desc = oiio.TypeDesc()
    # test 1    
    data = None 
    # not sure what filename and dataname are supposed so we test
    # only if get_image_info() returns False when given dummy data
    # TODO: expand the tests with proper data, so we can see when
    #       get_image_info() actually does what it's supposed to.
    filename = "filename"
    dataname = "dataname"
    try:
        cache.get_image_info(filename, dataname, desc, data)
        print "Test 1 passed"
    except:
        print "Test 1 failed"

    # test 2
   
    print

 
def ic_get_imagespec_test():
    # create an instance on which we can test
    # if ic_create_test() failed, this can't be done
    cache = oiio.ImageCache.create()
    
    print "Starting ImageCache.get_imagespec() tests..." 
    filename = "filename"
    spec = oiio.ImageSpec()

    # test 1
    try:
        cache.get_imagespec(filename, spec, 0)
        print "Test 1 passed"
    except:
        print "Test 1 failed"

    # test 2
    try:
        # third argument is 0 by default
        cache.get_imagespec(filename, spec)
        print "Test 2 passed"
    except:
        print "Test 2 failed"

    # test 3
    try:
        cache.get_imagespec(filename, spec, 2)
        print "Test 3 passed"
    except:
        print "Test 3 failed"

    print


def ic_get_pixels_test():
    # create an instance on which we can test
    # if ic_create_test() failed, this can't be done
    cache = oiio.ImageCache.create()
    
    print "Starting ImageCache.get_pixels() tests..."     

    filename = "filename"
    desc = oiio.TypeDesc()
    subimage = 0
    xbegin = ybegin = 0
    xend = yend = 1
    zbegin = zend = 0
    
    # test 1
    try:
        # this array will have to be modified to be an appropriate 
        # buffer once it's determined how the buffers are to be passed
        # from Python
        result = array.array("B")
        cache.get_pixels(filename, subimage, xbegin, xend, ybegin, yend, \
                        zbegin, zend, desc, result)
        print "Test 1 passed"
    except:
        print "Test 1 failed"

    # test 2 - pass an impossible pixel location as xbegin (-1)
    # Not sure if get_pixels() is supposed to return False or raise
    # an exception if given an impossible xbegin (or any other arg).
    # Currently, I supposed it should return False.
    try:
        # this array will have to be modified to be an appropriate 
        # buffer once it's determined how the buffers are to be passed
        # from Python
        result = array.array("B")
        cache.get_pixels(filename, subimage, -1, xend, ybegin, yend, \
                        zbegin, zend, desc, result)
        print "Test 2 passed (check the comments!)"
    except:
        print "Test 2 failed (check the comments!)"
    
    print


def ic_get_tile_test():
    # create an instance on which we can test
    # if ic_create_test() failed, this can't be done
    cache = oiio.ImageCache.create()
    
    print "Starting ImageCache.get_tile() tests..."     
    print "These tests are required to pass before we can test release_tile() and tile_pixels()"

    filename = "filename"    
    subimage = 0
    x = y = z = 0

    # test 1
    try:
        tile = None
        tile = cache.get_tile(filename, subimage, x, y, z)
        if tile != None:
            print "Test 1 passed"
        else: 
            print "Test 1 failed"
    except:
        print "Test 1 failed"

    # test 2 - pass an impossible value for x (-1)
    # same issue as with get_pixels_test() #1
    try:
        tile = None
        tile = cache.get_tile(filename, subimage, -1, y, z)
        if tile != None:
            print "Test 2 passed (check the comments!)"
        else: 
            print "Test 2 failed (check the comments!)"
    except:
        print "Test 2 failed (check the comments!)"

    print


def ic_release_tile_test():
    # create an instance on which we can test
    # if ic_create_test() failed, this can't be done
    cache = oiio.ImageCache.create()
    # run get_tile() so we have something to release
    filename = "filename"    
    subimage = 0
    x = y = z = 0
    tile = cache.get_tile(filename, subimage, x, y, z)
    
    print "Starting ImageCache.get_tile() tests..."      

    # test 1
    try:
        cache.release_tile(tile)
        print "Test 1 passed"
    except:
        print "Test 1 failed"

    print


def ic_tile_pixels_test():
    # create an instance on which we can test
    # if ic_create_test() failed, this can't be done
    cache = oiio.ImageCache.create()
    # run get_tile() so we have something to fetch
    filename = "filename"    
    subimage = 0
    x = y = z = 0
    tile = cache.get_tile(filename, subimage, x, y, z)    
    # the format of fetched pixels, currently set to "Unknown"
    desc = TypeDesc()
    
    # test 1
    try:
        pixels = None    
        pixels = cache.tile_pixels(tile, desc)
        if pixels != None:
            print "Test 1 passed"
        else:
            print "Test 1 failed (nothing returned)"
            # OTH, the data given is very likely to be wrong so that
            # can be the reason for failing
    except:
        print "Test 1 failed (exception raised)"

    print


def ic_geterror_test():
    # create an instance on which we can test
    # if ic_create_test() failed, this can't be done
    cache = oiio.ImageCache.create()    
    
    # test 1
    try:
        error_message = None
        error_message = cache.geterror()
        if len(error_message) >= 0:
            print "Test 1 passed"
    except:
        print "Test 1 failed"

    print


def ic_getstats_test():
    # create an instance on which we can test
    # if ic_create_test() failed, this can't be done
    cache = oiio.ImageCache.create()
    
    # test 1
    try:
        stats = None
        stats = cache.getstats() #default arg is 1
        if type(stats) == str:
            print "Test 1 passed"   
        else:
            print "Test 1 failed (nothing returned)"
    except:
        print "Test 1 failed (exception raised)" 

    print


def ic_invalidate_test():
    # create an instance on which we can test
    # if ic_create_test() failed, this can't be done
    cache = oiio.ImageCache.create()
    filename = "filename"
    # test 1    
    try:
        cache.invalidate(filename)
        print "Test 1 passed"
    except:
        print "Test 1 failed"

    print

def ic_invalidate_all_test():
    # create an instance on which we can test
    # if ic_create_test() failed, this can't be done
    cache = oiio.ImageCache.create()
    
    # test 1
    try:
        cache.invalidate_all() # default arg is force=False
        print "Test 1 passed"
    except:
        print "Test 1 failed"

    # test 2
    try:
        cache.invalidate_all(True)
        print "Test 2 passed"
    except:
        print "Test 2 failed"

    print


###

def run_tests():
    ic_create_test()
    ic_clear_test()
    ic_attribute_test()
    ic_getattribute_test()
    ic_resolve_filename_test()
    ic_get_image_info_test()
    ic_get_imagespec_test()
    ic_get_pixels_test()
    ic_get_tile_test()
    ic_release_tile_test()
    ic_tile_pixels_test()
    ic_geterror_test()
    ic_getstats_test()
    ic_invalidate_test()
    ic_invalidate_all_test()

###

import OpenImageIO as oiio
import array

plugin_path = raw_input("Enter the oiio plugin search path (probably <trunk>/dist/ARCH/lib) :\n ")

run_tests()
