#!/usr/bin/python 

# FIXME -- eventually, we want more (all?) of these to work

imagedir = parent + "/libtiffpic/depth"
files = [
    "flower-minisblack-02.tif",   #  73x43 2-bit minisblack gray image
    "flower-minisblack-04.tif",   #  73x43 4-bit minisblack gray image
    "flower-minisblack-06.tif",   #  73x43 6-bit minisblack gray image
    "flower-minisblack-08.tif",   #  73x43 8-bit minisblack gray image
    "flower-minisblack-10.tif",   #  73x43 10-bit minisblack gray image
    "flower-minisblack-12.tif",   #  73x43 12-bit minisblack gray image
    "flower-minisblack-14.tif",   #  73x43 14-bit minisblack gray image
    "flower-minisblack-16.tif",   # 73x43 16-bit minisblack gray image
    #FIXME "flower-minisblack-24.tif",   #  73x43 24-bit minisblack gray image
    #FIXME "flower-minisblack-32.tif",   #  73x43 32-bit minisblack gray image
    "flower-palette-02.tif",   #73x43 4-entry colormapped image
    "flower-palette-04.tif",   #73x43 16-entry colormapped image
    "flower-palette-08.tif",   #73x43 256-entry colormapped image
    #FIXME "flower-palette-16.tif",   # 73x43 65536-entry colormapped image
    "flower-rgb-contig-02.tif",   #  73x43 2-bit contiguous RGB image
    "flower-rgb-contig-04.tif",   #  73x43 4-bit contiguous RGB image
    "flower-rgb-contig-08.tif",   #  73x43 8-bit contiguous RGB image
    "flower-rgb-contig-10.tif",   #  73x43 10-bit contiguous RGB image
    "flower-rgb-contig-12.tif",   #  73x43 12-bit contiguous RGB image
    "flower-rgb-contig-14.tif",   #  73x43 14-bit contiguous RGB image
    "flower-rgb-contig-16.tif",   #  73x43 16-bit contiguous RGB image
    #FIXME "flower-rgb-contig-24.tif",   #  73x43 24-bit contiguous RGB image
    #FIXME "flower-rgb-contig-32.tif",   #  73x43 32-bit contiguous RGB image
    "flower-rgb-planar-02.tif",   #  73x43 2-bit seperated RGB image
    "flower-rgb-planar-04.tif",   #  73x43 4-bit seperated RGB image
    "flower-rgb-planar-08.tif",   #  73x43 8-bit seperated RGB image
    "flower-rgb-planar-10.tif",   #  73x43 10-bit seperated RGB image
    "flower-rgb-planar-12.tif",   #  73x43 12-bit seperated RGB image
    "flower-rgb-planar-14.tif",   #  73x43 14-bit seperated RGB image
    "flower-rgb-planar-16.tif"    #  73x43 16-bit seperated RGB image
    #FIXME "flower-rgb-planar-24.tif",   #  73x43 24-bit seperated RGB image
    #FIXME "flower-rgb-planar-32.tif",   #  73x43 32-bit seperated RGB image
    #FIXME "flower-separated-contig-08.tif",  # 73x43 8-bit contiguous CMYK image
    #FIXME "flower-separated-contig-16.tif",  # 73x43 16-bit contiguous CMYK image
    #FIXME "flower-separated-planar-08.tif",  # 73x43 8-bit separated CMYK image
    #FIXME "flower-separated-planar-16.tif",  # 73x43 16-bit separated CMYK image
    ]

for f in files:
    command += rw_command (imagedir, f)

print ("COMMAND= " + command)
