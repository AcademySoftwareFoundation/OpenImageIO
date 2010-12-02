#!/usr/bin/env python

# Parse the options the user provided from the command line
# -i takes three arguments: x and y coordinates and filename;
# -o takes one argument (the name of to file where the stitched image will be written)
# -p takes one argument (path to the oiio plugin library)
def option_parser():
    parser = OptionParser()
    parser.add_option("-i", nargs=3, action="append", dest="arguments")
    parser.add_option("-o", dest="output_file")    
    parser.add_option("-p", dest="plugin_path", help="OIIO plugin path")    
    (options, args) = parser.parse_args()
    coordinates = []
    images = []
    for i in options.arguments:
        xy = (int(i[0]), int(i[1]))
        filepath = i[2]
        coordinates.append(xy)
        images.append(filepath)
    plugin_path = options.plugin_path
    if not plugin_path:
        plugin_path = ""
    filename = options.output_file
    return (coordinates, images, plugin_path, filename)

# If the user didn't provide any arguments from the command line, or he ran
# the script as a module from Python, an interactive mode will be used to 
# get the needed information. 
# The function stops when the user enters an empty path to the image (just hits enter)
def user_prompt():
    coordinates = []
    images = []
    filepath = raw_input("Enter the path to the image: ")
    while filepath != "":
        if not os.path.exists(filepath):
            return False
        xy = raw_input("Specify position of image inside the stitched image (x y): ")
        xy = (int(xy[0]), int(xy[2]))
        images.append(filepath)
        coordinates.append(xy)
        filepath = raw_input("Enter the path to the image: ")
    return (coordinates, images)

# Check if the coordinates our user provided make sense. This means that there
# can't be any "holes" in the provided coordinates. Examples of wrong coordinates:
# 0 0, 2 0 (needs 1 0)
# 0 0, 1 0, 0 1, 0 3 (needs 0 2)
def validate_coordinates(images_info):
    coordinates = images_info[0]
    zipped = zip(*coordinates)
    xmax = max(zipped[0])
    ymax = max(zipped[1])
    for x in range(xmax):
        if list(zipped[0]).count(x) == 0:
            return False
    for y in range(ymax):
        if list(zipped[1]).count(y) == 0:
            return False
    return True
    
# Open every image in the list of filepaths, return a list of opened 
# ImageInput instances.
# TODO: A function which would close (or delete?) all the instances
def open_images(filepaths):
    images = []
    for i in filepaths:
        inp = o.ImageInput.create(i, path)
        spec = o.ImageSpec()
        if inp.open(i, spec):
            images.append(inp)
        else:
            return False
    return images
            
# Takes a list consisting of two sublists: [0] is a list of coordinates (which 
# are stored as tuples), [1] is a list of opened ImageInput instances; and puts 
# them in a dictionary (coordinates as keys). The dictionary makes using coordinates
# very easy, since grid((x,y)) returns a matching image.
def convert_to_grid(images_data):
    grid = {}
    for i in range(len(images_data[0])):
        grid[images_data[0][i]] = images_data[1][i]
    return grid   

# Check if all the images which should form a row have the same heights.
# Returns False (if they don't) or the width of the row they would form.
# This isn't used in this example, but might be useful.
# TODO: check_constraints() actually does this, since it can check both a single row
#       and the whole grid. Is there any reason to keep check_row()?
#       
def check_row(images):
    spec_0 = images[0].spec()
    row_width = spec_0.width
    for i in images[1:]:
        spec_i = i.spec()
        if spec_i.height != spec_0.height:
            return False
        row_width += spec_i.width
    return row_width

# It uses check_row() to check if the whole grid can be merged (images in a row
# must have matching heights, rows should have matching widths)
# TODO: Also not used anywhere. It uses multiple calls to check_row() to achieve
#       what the more powerful check_constraints() can do by itself. Delete it?
def can_stitch(images_table):
    width = check_row(images_table[0])
    if width:
        for row in images_table[1:]:
            row_width = check_row(row)
            if row_width != width:
                return False
        return True
    else:
        return False


# stitch the images which make a row to each other.
# it takes a list of opened ImageInput instances
def stitch_row(images):
    arr = array.array("B")
    row_width = check_row(images)
    if row_width:
        spec_0 = images[0].spec()
        for row in range(spec_0.height):
            for i in images:
                spec_i = i.spec()
                arr_i = array.array("B", "\0" * spec_i.scanline_bytes())
                if i.read_scanline(row, 0, spec_i.format, arr_i):
                    arr += arr_i
                else:
                    return False
        return arr
    else:
        return False

        
# it takes a list of rows. Each row is actually an array, so the joining
# is trivial.
def join_rows(rows):
    arr = array.array("B")
    for i in rows:
        arr += i
    return arr

# This function takes a dictionary of opened
# ImageInputs and checks can they be stitched together according to the
# coordinates provided (as dictionary keys).
def check_constraints(images):
    # Extract the number of rows and the number of images in the row with
    # the most images. Rows can have different number of images as long
    # as their widths add up to the same number.
    # Add +1 to both so we can properly use range() in for loops which iterate
    # over rows and columns.
    coordinates = images.keys()
    zipped = zip(*coordinates)
    n_columns_max = int(max(zipped[0])) + 1
    n_rows = int(max(zipped[1])) + 1
    # this will be the height of the final, stitched image
    height = 0
    for y in range(n_rows):        
        row_height = images[(0,y)].spec().height
        row_width = 0        
        for x in range(n_columns_max):
            if row_height == images[(x,y)].spec().height:
                row_width += images[(x,y)].spec().width
            else:
                return False            
            # if the current row has less images than n_columns_max,
            # break the inner for loop when you reach the end
            if not images.get((x+1,y)):
                break
        height += row_height
        if y == 0:
            base_width = row_width
        else:
            if base_width != row_width:
                return False
    return (row_width, height)


# Given a dictionary of opened ImageInput instances, 
# the function stitches them and returns a single
# array.array which can then be written to a file.
def stitch_images(images, name):
    # Let us first call check_constraints() in case the user forgot
    # to do so. Also, we'll get the resolution of the image.
    xy = check_constraints(images)
    if not xy:
        return False
    # Extract the number of rows and the number of images in the row with
    # the most images. Rows can have different number of images as long
    # as their widths add up to the same number.
    coordinates = images.keys()
    zipped = zip(*coordinates)
    n_columns_max = max(zipped[0]) + 1
    n_rows = max(zipped[1]) + 1
    # Form a row from the images with the same y coordinate.    
    # Stitch the images in a row by calling stitch_row(row), which returns
    # an array representing the data of what is now a single image. 
    # 
    rows = []
    for y in range(n_rows):
        row = []
        for x in range(n_columns_max):
            row.append(images[(x,y)])
            if images.get((x+1,y)) == None:
                break
        stitched_row = stitch_row(row)
        rows.append(stitched_row)
    data = join_rows(rows)
    if data:
        desc = images[(0,0)].spec().format
        spec = o.ImageSpec(xy[0], xy[1], images[(0,0)].spec().nchannels, desc)
        out = o.ImageOutput.create(name, path)
        out.open(name, spec, False)
        out.write_image(desc, data)
        out.close()
    else:
        return False
    


###################################
# TODO: A function which would check if the user gave the proper xy coordinates,
#       i.e. an image at 1 1 is not possible without images at 0 0 and 1 0. Be careful
#       to give proper coordinates for now (though the order in which the images are
#       given does not matter).
# TODO: Currently, the coordinates should be entered as "x y", without the "". 
#       So, 0 0 works for now, and inputs like (0,0) or "0, 0" (with the "") will
#       be supported.
# main()
if __name__ == "__main__":
    import OpenImageIO as o
    import array
    import os
    import sys
    from optparse import OptionParser


    if len(sys.argv) > 1:
        parsed = option_parser()
        filename = parsed[3]
        path = parsed[2]
        images_info = (parsed[0], parsed[1])
    else:
        filename = raw_input("Enter the desired name of the final stitched image: ")
        path = raw_input("Enter the path to the oiio plugin dir: ")
        images_info = user_prompt()
    if not validate_coordinates(images_info):
        print "Coordinates not valid"
    else:
        ii_instances = open_images(images_info[1])
        if not ii_instances:
            print "Can't open given images"
        else:
            images_info = (images_info[0], ii_instances)
            grid = convert_to_grid(images_info)
            # the main part of the stitcher. Check if the images can be stitched, and
            # stitch them if so.
            print "Checking whether the images can be merged..."
            if check_constraints(grid):
                print "Check ok, merging images..."
                stitch_images(grid, filename)
                print "The merging is complete." 
            else:
                print "Can't stitch the images"

###################################



# this was for testing only
def stitch_halves(left, right):
    
    spec_left = o.ImageSpec()
    spec_right = o.ImageSpec()
    
    inp_left = o.ImageInput.create("jpg", "/home/dgaletic/code/oiio-trunk/dist/linux/lib")
    inp_right = o.ImageInput.create("jpg", "/home/dgaletic/code/oiio-trunk/dist/linux/lib")
    inp_left.open(left, spec_left)
    inp_right.open(right, spec_right)

    if spec_right.height == spec_left.height:
        xres = spec_left.width + spec_right.width
        yres = spec_left.height
        desc = spec_left.format

        spec_new = o.ImageSpec(xres, yres, spec_left.nchannels, desc)

        out = o.ImageOutput.create("jpg", "/home/dgaletic/code/oiio-trunk/dist/linux/lib")
        out.open("test-join.jpg", spec_new, False)

        for scanline in range(0, spec_left.height):
            arr_l = array.array("B", "\0" * spec_left.scanline_bytes())
            arr_r = array.array("B", "\0" * spec_right.scanline_bytes())
            arr_new = array.array("B")
            inp_left.read_scanline(scanline, 0, spec_left.format, arr_l)
            inp_right.read_scanline(scanline, 0, spec_right.format, arr_r)
            arr_new = arr_l + arr_r
            out.write_scanline(scanline, 0, spec_new.format, arr_new)
        out.close()
    else:
        print "Heights do not match"

# stitches together an image split into three parts
# this was also for testing only
def test_stitch_thirds():           
    pic_third1 = "/home/dgaletic/code/oiio-testimages/stitch_me/tahoe-0.jpg"
    pic_third2 = "/home/dgaletic/code/oiio-testimages/stitch_me/tahoe-1.jpg"
    pic_third3 = "/home/dgaletic/code/oiio-testimages/stitch_me/tahoe-2.jpg"
    path = "/home/dgaletic/code/oiio-trunk/dist/linux/lib"
    spec1 = o.ImageSpec()
    spec2 = o.ImageSpec()
    spec3 = o.ImageSpec()
    pic1 = o.ImageInput.create("jpg", path)
    pic2 = o.ImageInput.create("jpg", path)
    pic3 = o.ImageInput.create("jpg", path)
    pic1.open(pic_third1, spec1)
    pic2.open(pic_third2, spec2)
    pic3.open(pic_third3, spec3)
    spec_stitched = o.ImageSpec(spec1.width + spec2.width + spec3.width, spec1.height, spec1.nchannels, spec1.format)
    # create a list of opened ImageInput instances
    images = [pic1, pic2, pic3]
    # stitch the images next to each other, return the resulting array
    arr = stitch_row(images)
    if arr:
        out = o.ImageOutput.create("jpg", path)
        out.open("/home/dgaletic/code/branch/src/python/thirds.jpg", spec_stitched, False)  
        out.write_image(spec_stitched.format, arr)
        out.close()
    #return True
#else:
    #return False


"""
inp00 = o.ImageInput.create("jpg", path)
spec00 = o.ImageSpec()
inp00.open("/home/dgaletic/code/oiio-testimages/stitch_me/tahoe00.jpg", spec00)

inp10 = o.ImageInput.create("jpg", path)
spec10 = o.ImageSpec()
inp10.open("/home/dgaletic/code/oiio-testimages/stitch_me/tahoe10.jpg", spec10)

inp20 = o.ImageInput.create("jpg", path)
spec20 = o.ImageSpec()
inp20.open("/home/dgaletic/code/oiio-testimages/stitch_me/tahoe20.jpg", spec20)

inp01 = o.ImageInput.create("jpg", path)
spec01 = o.ImageSpec()
inp01.open("/home/dgaletic/code/oiio-testimages/stitch_me/tahoe01.jpg", spec01)

inp11 = o.ImageInput.create("jpg", path)
spec11 = o.ImageSpec()
inp11.open("/home/dgaletic/code/oiio-testimages/stitch_me/tahoe11.jpg", spec11)

inp02 = o.ImageInput.create("jpg", path)
spec02 = o.ImageSpec()
inp02.open("/home/dgaletic/code/oiio-testimages/stitch_me/tahoe02.jpg", spec02)

images = {(0,0):inp00, (1,0):inp10, (2,0):inp20, (0,1):inp01, (1,1):inp11, (0,2):inp02}

"""
