..
  Copyright Contributors to the OpenImageIO project.
  SPDX-License-Identifier: CC-BY-4.0


Comparing Images With `idiff`
#############################


Overview
========

The `idiff` program compares two images, printing a report about how
different they are and optionally producing a third image that records the
pixel-by-pixel differences between them.  There are a variety of options and
ways to compare (absolute pixel difference, various thresholds for warnings
and errors, and also an optional perceptual difference metric).

Because `idiff` is built on top on OpenImageIO, it can compare two images of
any formats readable by ImageInput plugins on hand.  They may have any (or
different) file formats, data formats, etc.


Using `idiff`
=============

The `idiff` utility is invoked as follows:

    `idiff` [*options*] *input1* *input2|directory*

Where *input1* and *input2* are the names of two image files that should be
compared.  They may be of any format recognized by OpenImageIO (i.e., for
which image-reading plugins are available).

When a *directory* is specified instead of *input2* then `idiff` will use
the same-named file as *input1* in the specified directory.

If the two input images are not the same resolutions, or do not have the
same number of channels, the comparison will return FAILURE immediately and
will not attempt to compare the pixels of the two images.  If they are the
same dimensions, the pixels of the two images will be compared, and a report
will be printed including the mean and maximum error, how many pixels were
above the warning and failure thresholds, and whether the result is `PASS`,
`WARNING`, or `FAILURE`. For example::

    $ idiff a.jpg b.jpg

    Comparing "a.jpg" and "b.jpg"
      Mean error = 0.00450079
      RMS error = 0.00764215
      Peak SNR = 42.3357
      Max error  = 0.254902 @ (700, 222, B)
      574062 pixels (82.1%) over 1e-06
      574062 pixels (82.1%) over 1e-06
    FAILURE

The "mean error" is the average difference (per channel, per pixel). The
"max error" is the largest difference in any pixel channel, and will point
out on which pixel and channel it was found. It will also give a count of
how many pixels were above the warning and failure thresholds.

The metadata of the two images (e.g., the comments) are not currently
compared; only differences in pixel values are taken into consideration.

Raising the thresholds
^^^^^^^^^^^^^^^^^^^^^^

By default, if any pixels differ between the images, the comparison will
fail.  You can allow *some* differences to still pass by raising the failure
thresholds.  The following example will allow images to pass the comparison
test, as long as no more than 10% of the pixels differ by 0.004 (just above
a 1/255 threshold)::

    idiff -fail 0.004 -failpercent 10 a.jpg b.jpg

But what happens if a just a few pixels are very different?  Maybe you want
that to fail, also.  The following adjustment will fail if at least 10% of
pixels differ by 0.004, or if *any* pixel differs by more than 0.25::

    idiff -fail 0.004 -failpercent 10 -hardfail 0.25 a.jpg b.jpg

If none of the failure criteria are met, and yet some pixels are still
different, it will still give a WARNING.  But you can also raise the warning
threshold in a similar way::

    idiff -fail 0.004 -failpercent 10 -hardfail 0.25 \
             -warn 0.004 -warnpercent 3 a.jpg b.jpg

The above example will PASS as long as fewer than 3% of pixels differ by
more than 0.004.  If it does, it will be a WARNING as long as no more than
10% of pixels differ by 0.004 and no pixel differs by more than 0.25,
otherwise it is a FAILURE.



Output a difference image
^^^^^^^^^^^^^^^^^^^^^^^^^

Ordinary text output will tell you how many pixels failed or were warnings,
and which pixel had the biggest difference.  But sometimes you need to see
visually where the images differ.  You can get `idiff` to save an image of
the differences between the two input images::

    idiff -o diff.tif -abs a.jpg b.jpg

The `-abs` flag saves the absolute value of the differences (i.e., all
positive values or zero).  If you omit the `-abs`, pixels in which
:file:`a.jpg` have smaller values than :file:`b.jpg` will be negative in the
difference image (be careful in this case of using a file format that
doesn't support negative values).

You can also scale the difference image with the `-scale`, making them
easier to see.  And the `-od` flag can be used to output a difference image
only if the comparison fails, but not if the images pass within the
designated threshold (thus saving you the trouble and space of saving a
black image).


`idiff` Command Line Argument Reference
=======================================

The various command-line options are discussed below:

General options
^^^^^^^^^^^^^^^

.. describe:: --help

    Prints usage information to the terminal.

.. option:: --version

    Prints the version designation of the OIIO library.

.. describe:: -v

    Verbose output --- more detail about what it finds when comparing
    images, even when the comparison does not fail.

.. describe:: -q

    Quiet mode -- output nothing for successful match), output only minimal
    error messages to stderr for failure / no match.  The shell return code
    also indicates success or failure (successful match returns 0, failure
    returns nonzero).

.. describe:: -a

    Compare all subimages.  Without this flag, only the first subimage of
    each file will be compared.


Thresholds and comparison options
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. describe:: -fail A
              -failrelative R
              -failpercent P
              -hardfail H

    Sets the threshold for FAILURE: if more than *P* % of pixels (on a 0-100
    floating point scale) are greater than *A* different absolutely or *R*
    relatively (to the mean of the two values), or if *any* pixels are more
    than *H* different absolutely.  The defaults are to fail if more than 0%
    (any) pixels differ by more than 0.00001 (1e-6), and *H* is infinite.

.. describe:: -warn A
              -warnrelative R
              -warnpercent P
              -hardwarn H

    Sets the threshold for WARNING: if more than *P* % of pixels (on a 0-100
    floating point scale) are greater than *A* different absolutely or *R*
    different relatively (to the mean of the two values), or if *any* pixels
    are more than *H* different absolutely.  The defaults are to warn if more
    than 0% (any) pixels differ by more than 0.00001 (1e-6), and *H* is
    infinite.

.. describe:: --allowfailures N

    Allows up to *N* pixels to differ by any amount, and still consider it
    a matching image.

    This option was added in OIIO 2.3.19.

.. describe:: -p

    Does an additional test on the images to attempt to see if they are
    *perceptually* different (whether you are likely to discern a difference
    visually), using Hector Yee's metric.  If this option is enabled, the
    statistics will additionally show a report on how many pixels failed the
    perceptual test, and the test overall will fail if more than the "fail
    percentage" failed the perceptual test.

Difference image output
^^^^^^^^^^^^^^^^^^^^^^^

.. describe:: -o outputfile

    Outputs a *difference image* to the designated file. This difference
    image pixels consist are each of the value of the corresponding pixel
    from *image1* minus the value of the pixel *image2*.

    The file extension of the output file is used to determine the file
    format to write (e.g., :file:`out.tif` will write a TIFF file,
    :file:`out.jpg` will write a JPEG, etc.).  The data format of the output
    file will be format of whichever of the two input images has higher
    precision (or the maximum precision that the designated output format is
    capable of, if that is less than either of the input imges).

    Note that pixels whose value is lower in *image1* than in *image2*, this
    will result in negative pixels (which may be clamped to zero if the
    image format does not support negative values)), unless the `-abs`
    option is also used.

.. describe:: -abs

    Will cause the output image to consist of the *absolute value* of the
    difference between the two input images (so all values in the difference
    image :math:`\ge 0`.

.. describe:: -scale factor

    Scales the values in the difference image by the given (floating point)
    factor.  The main use for this is to make small actual differences more
    visible in the resulting difference image by giving a large scale
    factor.

.. describe:: -od

    Causes a difference image to be produce *only* if the image comparison
    fails.  That is, even if the `-o` option is used, images that are within
    the comparison threshold will not write out a useless black (or nearly
    black) difference image.


Process return codes
^^^^^^^^^^^^^^^^^^^^

The `idiff` program will return a code that can be used by scripts to
indicate the results:

======== ==================================================================
0        OK: the images match within the warning and error thresholds.
1        Warning: the errors differ a little, but within error thresholds.
2        Failure: the errors differ a lot, outside error thresholds.
3        The images weren't the same size and couldn't be compared.
4        File error: could not find or open input files, etc.
======== ==================================================================

