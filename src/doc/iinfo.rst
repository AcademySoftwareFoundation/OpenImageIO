..
  Copyright Contributors to the OpenImageIO project.
  SPDX-License-Identifier: CC-BY-4.0


.. _chap-iinfo:

Getting Image information With `iinfo`
######################################

The :program:`iinfo` program will print either basic information (name,
resolution, format) or detailed information (including all metadata) found
in images.  Because :program:`iinfo` is built on top on OpenImageIO, it will
print information about images of any formats readable by ImageInput plugins
on hand.



Using `iinfo`
=============

The :program:`iinfo` utility is invoked as follows:

    `iinfo` *options* *filename*

Where *filename* (and any following strings) names the image file(s) whose
information should be printed.  The image files may be of any format
recognized by OpenImageIO (i.e., for which ImageInput plugins are
available).

In its most basic usage, it simply prints the resolution, number of
channels, pixel data type, and file format type of each of the files
listed::

    $ iinfo img_6019m.jpg grid.tif lenna.png

    img_6019m.jpg : 1024 x  683, 3 channel, uint8 jpeg
    grid.tif      :  512 x  512, 3 channel, uint8 tiff
    lenna.png     :  120 x  120, 4 channel, uint8 png


The `-s` flag also prints the uncompressed sizes of each image
file, plus a sum for all of the images::

    $ iinfo -s img_6019m.jpg grid.tif lenna.png

    img_6019m.jpg : 1024 x  683, 3 channel, uint8 jpeg (2.00 MB)
    grid.tif      :  512 x  512, 3 channel, uint8 tiff (0.75 MB)
    lenna.png     :  120 x  120, 4 channel, uint8 png (0.05 MB)
    Total size: 2.81 MB


The `-v` option turns on \emph{verbose mode}, which exhaustively
prints all metadata about each image::

    img_6019m.jpg : 1024 x  683, 3 channel, uint8 jpeg
        channel list: R, G, B
        Color space: sRGB
        ImageDescription: "Family photo"
        Make: "Canon"
        Model: "Canon EOS DIGITAL REBEL XT"
        Orientation: 1 (normal)
        XResolution: 72
        YResolution: 72
        DateTime: "2008:05:04 19:51:19"
        Exif:YCbCrPositioning: 2
        ExposureTime: 0.004
        FNumber: 11
        Exif:ExposureProgram: 2 (normal program)
        Exif:ISOSpeedRatings: 400
        Exif:DateTimeOriginal: "2008:05:04 19:51:19"
        Exif:DateTimeDigitized: "2008:05:04 19:51:19"
        Exif:ShutterSpeedValue: 7.96579 (1/250 s)
        Exif:ApertureValue: 6.91887 (f/11)
        Exif:ExposureBiasValue: 0
        Exif:MeteringMode: 5 (pattern)
        Exif:Flash: 16 (no flash, flash suppression)
        Exif:FocalLength: 27 (27 mm)
        Exif:ColorSpace: 1
        Exif:PixelXDimension: 2496
        Exif:PixelYDimension: 1664
        Exif:FocalPlaneXResolution: 2855.84
        Exif:FocalPlaneYResolution: 2859.11
        Exif:FocalPlaneResolutionUnit: 2 (inches)
        Exif:CustomRendered: 0 (no)
        Exif:ExposureMode: 0 (auto)
        Exif:WhiteBalance: 0 (auto)
        Exif:SceneCaptureType: 0 (standard)
        Keywords: "Carly; Jack"


If the input file has multiple subimages, extra information summarizing
the subimages will be printed::

    $ iinfo img_6019m.tx

    img_6019m.tx : 1024 x 1024, 3 channel, uint8 tiff (11 subimages)

    $ iinfo -v img_6019m.tx

    img_6019m.tx : 1024 x 1024, 3 channel, uint8 tiff
        11 subimages: 1024x1024 512x512 256x256 128x128 64x64 32x32 16x16 8x8 4x4 2x2 1x1
        channel list: R, G, B
        tile size: 64 x 64
        ...

Furthermore, the `-a` option will print information about all individual
subimages::

    $ iinfo -a ../sample-images/img_6019m.tx

    img_6019m.tx : 1024 x 1024, 3 channel, uint8 tiff (11 subimages)
     subimage  0: 1024 x 1024, 3 channel, uint8 tiff
     subimage  1:  512 x  512, 3 channel, uint8 tiff
     subimage  2:  256 x  256, 3 channel, uint8 tiff
     subimage  3:  128 x  128, 3 channel, uint8 tiff
     subimage  4:   64 x   64, 3 channel, uint8 tiff
     subimage  5:   32 x   32, 3 channel, uint8 tiff
     subimage  6:   16 x   16, 3 channel, uint8 tiff
     subimage  7:    8 x    8, 3 channel, uint8 tiff
     subimage  8:    4 x    4, 3 channel, uint8 tiff
     subimage  9:    2 x    2, 3 channel, uint8 tiff
     subimage 10:    1 x    1, 3 channel, uint8 tiff


    $ iinfo -v -a img_6019m.tx
    img_6019m.tx : 1024 x 1024, 3 channel, uint8 tiff
        11 subimages: 1024x1024 512x512 256x256 128x128 64x64 32x32 16x16 8x8 4x4 2x2 1x1 
     subimage  0: 1024 x 1024, 3 channel, uint8 tiff
        channel list: R, G, B
        tile size: 64 x 64
        ...
     subimage  1:  512 x  512, 3 channel, uint8 tiff
        channel list: R, G, B
        ...
    ...


`iinfo` command-line options
=====================================

.. describe:: --help

    Prints usage information to the terminal.

.. option:: --version

    Prints the version designation of the OIIO library.

.. describe:: -v

    Verbose output --- prints all metadata of the image files.

.. describe:: -a

    Print information about all subimages in the file(s).

.. describe:: -f

    Print the filename as a prefix to every line.  For example::
    
        $ iinfo -v -f img_6019m.jpg
    
        img_6019m.jpg : 1024 x  683, 3 channel, uint8 jpeg
        img_6019m.jpg : channel list: R, G, B
        img_6019m.jpg : Color space: sRGB
        img_6019m.jpg : ImageDescription: "Family photo"
        img_6019m.jpg : Make: "Canon"
        ...

.. describe:: -m pattern

    Match the *pattern* (specified as an extended regular expression)
    against data metadata field names and print only data fields whose names
    match.  The default is to print all data fields found in the file (if
    `-v` is given).
    
    For example::
    
        $ iinfo -v -f -m ImageDescription test*.jpg
    
        test3.jpg :     ImageDescription: "Birthday party"
        test4.jpg :     ImageDescription: "Hawaii vacation"
        test5.jpg :     ImageDescription: "Bob's graduation"
        test6.jpg :     ImageDescription: <unknown>
    
    Note: the `-m` option is probably not very useful without also using
    the `-v` and `-f` options.

.. describe:: --hash

    Displays a SHA-1 hash of the pixel data of the image (and of each
    subimage if combined with the `-a` flag).

.. describe:: -s

    Show the image sizes, including a sum of all the listed images.

