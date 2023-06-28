..
  Copyright Contributors to the OpenImageIO project.
  SPDX-License-Identifier: CC-BY-4.0


Converting Image Formats With `iconvert`
########################################

Overview
========

The `iconvert` program will read an image (from any file format for which an
ImageInput plugin can be found) and then write the image to a new file (in
any format for which an ImageOutput plugin can be found). In the process,
`iconvert` can optionally change the file format or data format (for
example, converting floating-point data to 8-bit integers), apply gamma
correction, switch between tiled and scanline orientation, or alter or add
certain metadata to the image.

The `iconvert` utility is invoked as follows:

    `iconvert` *optiions input output*

Where *input* and *output* name the input image and desired output filename.
The image files may be of any format recognized by OpenImageIO (i.e., for
which ImageInput plugins are available).  The file format of the output
image will be inferred from the file extension of the output filename (e.g.,
:file:`foo.tif` will write a TIFF file).

Alternately, any number of files may be specified as follows:

    `iconvert --inplace` [*options*] *file1* *file2* ...

When the `--inplace` option is used, any number of file names :math:`\ge 1`
may be specified, and the image conversion commands are applied to each file
in turn, with the output being saved under the original file name.  This is
useful for applying the same conversion to many files, or simply if you want
to replace the input with the output rather than create a new file with a
different name.



`iconvert` Recipes
==================

This section will give quick examples of common uses of `iconvert`.

Converting between file formats
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

It's a snap to converting among image formats supported by OpenImageIO
(i.e., for which ImageInput and ImageOutput plugins can be found). The
`iconvert` utility will simply infer the file format from the file
extension. The following example converts a PNG image to JPEG::

    iconvert lena.png lena.jpg

Changing the data format or bit depth
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Just use the `-d` option to specify a pixel data format.  For example,
assuming that :file:`in.tif` uses 16-bit unsigned integer pixels, the
following will convert it to an 8-bit unsigned pixels::

    iconvert -d uint8 in.tif out.tif

Changing the compression
^^^^^^^^^^^^^^^^^^^^^^^^

The following command converts writes a TIFF file, specifically using zip
compression::

    iconvert --compression zip in.tif out.tif

The following command writes its results as a JPEG file at a compression
quality of 50 (pretty severe compression), illustrating how some compression
methods allow a quality metric to be optionally appended to the name::

    iconvert --compression jpeg:50 50 big.jpg small.jpg

Gamma-correcting an image
^^^^^^^^^^^^^^^^^^^^^^^^^

The following gamma-corrects the pixels, raising all pixel values to
:math:`x^{1/2.2}` upon writing::

    iconvert -g 2.2 in.tif out.tif

Converting between scanline and tiled images
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Convert a scanline file to a tiled file with $16 x 16$ tiles::

    iconvert --tile 16 16 s.tif t.tif

Convert a tiled file to scanline::

    iconvert --scanline t.tif s.tif

Converting images in place
^^^^^^^^^^^^^^^^^^^^^^^^^^

You can use the `--inplace` flag to cause the output to
\emph{replace} the input file, rather than create a new file with a
different name.  For example, this will re-compress all of your 
TIFF files to use ZIP compression (rather than whatever they currently
are using)::

    iconvert --inplace --compression zip *.tif

Change the file modification time to the image capture time
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Many image formats (including JPEGs from digital cameras) contain an
internal time stamp indicating when the image was captured.  But the
time stamp on the file itself (what you'd see in a directory listing
from your OS) most likely shows when the file was last copied, not when
it was created or captured.  You can use the following command to
re-stamp your files so that the file system modification time matches
the time that the digital image was originally captured::

    iconvert --inplace --adjust-time *.jpg

Add captions, keywords, IPTC tags
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

For formats that support it, you can add a caption/image description,
keywords, or arbitrary string metadata::

    iconvert --inplace --adjust-time --caption "Hawaii vacation" *.jpg

    iconvert --inplace --adjust-time --keyword "John" img18.jpg img21.jpg

    iconvert --inplace --adjust-time --attrib IPTC:State "HI" \
              --attrib IPTC:City "Honolulu" *.jpg



`iconvert` command-line options
===================================

.. describe:: --help

    Prints usage information to the terminal.

.. describe:: -v

    Verbose status messages.

.. describe:: --threads n

    Use *n* execution threads if it helps to speed up image operations. The
    default (also if :math:`n=0`) is to use as many threads as there are
    cores present in the hardware.

.. describe:: --inplace

    Causes the output to *replace* the input file, rather than create a new
    file with a different name.
    
    Without this flag, `iconvert` expects two file names, which will be used
    to specify the input and output files, respectively.
    
    But when `--inplace` option is used, any number of file names :math:`\ge 1`
    may be specified, and the image conversion commands are applied to each
    file in turn, with the output being saved under the original file name.
    This is useful for applying the same conversion to many files.
    
    For example, the following example will add the caption "Hawaii
    vacation" to all JPEG files in the current directory::

            iconvert --inplace --adjust-time --caption "Hawaii vacation" *.jpg

.. describe:: -d datatype

    Attempt to sets the output pixel data type to one of: `UINT8`, `sint8`,
    `uint16`, `sint16`, `half`, `float`, `double`.
    
    The types `uint10` and `uint12` may be used to request 10- or 12-bit
    unsigned integers.  If the output file format does not support them,
    `uint16` will be substituted.
    
    If the `-d` option is not supplied, the output data type will be the
    same as the data format of the input file, if possible.
    
    In any case, if the output file type does not support the requested data
    type, it will instead use whichever supported data type results in the
    least amount of precision lost.


.. describe:: -g gamma

    Applies a gamma correction of :math:`1/\mathrm{gamma}` to the pixels as they
    are output.

.. describe:: --sRGB

    Explicitly tags the image as being in sRGB color space.  Note that this
    does not alter pixel values, it only marks which color space those
    values refer to (and only works for file formats that understand such
    things).  An example use of this command is if you have an image that is
    not explicitly marked as being in any particular color space, but you
    know that the values are sRGB.

.. describe:: --tile x y

    Requests that the output file be tiled, with the given :math:`x \times y`
    tile size, if tiled images are supported by the output format.
    By default, the output file will take on the tiledness and tile size
    of the input file.

.. describe:: --scanline

    Requests that the output file be scanline-oriented (even if the input
    file was tile-oriented), if scanline orientation is supported by the
    output file format.  By default, the output file will be scanline if the
    input is scanline, or tiled if the input is tiled.

.. describe:: --separate
              --contig

    Forces either "separate" (e.g., RRR...GGG...BBB) or "contiguous" (e.g.,
    RGBRGBRGB...) packing of channels in the file.  If neither of these
    options are present, the output file will have the same kind of channel
    packing as the input file.  Of course, this is ignored if the output
    file format does not support a choice or does not support the particular
    choice requested.

.. describe:: --compression method
              --compression method:quality

    Sets the compression method, and optionally a quality setting, for the
    output image.  Each ImageOutput plugin will have its own set of methods
    that it supports.
    
    By default, the output image will use the same compression technique as
    the input image (assuming it is supported by the output format,
    otherwise it will use the default compression method of the output
    plugin).

.. describe:: --quality q

    Sets the compression quality to *q*, on a 1--100 floating-point scale.
    This only has an effect if the particular compression method supports
    a quality metric (as JPEG does).
    
    DEPRECATED(2.1): 
    This is considered deprecated, and in general we now recommend just
    appending the quality metric to the `compression name:qual`.

.. describe:: --no-copy-image

    Ordinarily, `iconvert` will attempt to use `ImageOutput::copy_image`
    underneath to avoid de/recompression or alteration of pixel values,
    unless other settings clearly contradict this (such as any settings that
    must alter pixel values).  The use of `--no-copy-image` will force all
    pixels to be decompressed, read, and compressed/written, rather than
    copied in compressed form.  We're not exactly sure when you would need
    to do this, but we put it in just in case.

.. describe:: --adjust-time

    When this flag is present, after writing the output, the resulting
    file's modification time will be adjusted to match any `"DateTime"`
    metadata in the image.  After doing this, a directory listing will show
    file times that match when the original image was created or captured,
    rather than simply when `iconvert` was run.  This has no effect on
    image files that don't contain any `"DateTime"` metadata.

.. describe:: --caption text

    Sets the image metadata `"ImageDescription"`. This has no effect if the
    output image format does not support some kind of title, caption, or
    description metadata field. Be careful to enclose *text* in quotes if
    you want your caption to include spaces or certain punctuation!

.. describe:: --keyword text

    Adds a keyword to the image metadata `"Keywords"`.  Any existing
    keywords will be preserved, not replaced, and the new keyword will not
    be added if it is an exact duplicate of existing keywords.  This has no
    effect if the output image format does not support some kind of keyword
    field.
    
    Be careful to enclose *text* in quotes if you want your keyword to
    include spaces or certain punctuation.  For image formats that have only
    a single field for keywords, OpenImageIO will concatenate the keywords,
    separated by semicolon (`;`), so don't use semicolons within your
    keywords.

.. describe:: --clear-keywords

    Clears all existing keywords in the image.

.. describe:: --attrib text

    Sets the named image metadata attribute to a string given by *text*.
    For example, you could explicitly set the IPTC location metadata fields
    with::

        iconvert --attrib "IPTC:City" "Berkeley" in.jpg out.jpg

.. describe:: --orientation orient

    Explicitly sets the image's `"Orientation"` metadata to a numeric value
    (see :ref:`sec-metadata-orientation` for the numeric codes). This
    only changes the metadata field that specifies how the image should be
    displayed, it does NOT alter the pixels themselves, and so has no effect
    for image formats that don't support some kind of orientation metadata.

.. describe:: --rotcw
              --rotccw
              --rot180

    Adjusts the image's `"Orientation"` metadata by rotating it
    :math:`90^\circ` clockwise, :math:`90^\circ` degrees counter-clockwise,
    or :math:`180^\circ`, respectively, compared to its current setting.
    This only changes the metadata field that specifies how the image should
    be displayed, it does NOT alter the pixels themselves, and so has no
    effect for image formats that don't support some kind of orientation
    metadata.
