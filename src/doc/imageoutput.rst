.. _chap-imageoutput:

ImageOutput: Writing Images
###########################

.. _sec-image-output-made-simple:

Image Output Made Simple
========================

Here is the simplest sequence required to write the pixels of a 2D image
to a file::

    #include <OpenImageIO/imageio.h>
    using namespace OIIO;
    ...

    const char *filename = "foo.jpg";
    const int xres = 640, yres = 480;
    const int channels = 3;  // RGB
    unsigned char pixels[xres*yres*channels];

    std::unique_ptr<ImageOutput> out = ImageOutput::create (filename);
    if (! out)
        return;
    ImageSpec spec (xres, yres, channels, TypeDesc::UINT8);
    out->open (filename, spec);
    out->write_image (TypeDesc::UINT8, pixels);
    out->close ();

This little bit of code does a surprising amount of useful work:

* Search for an ImageIO plugin that is capable of writing the file
  :file:`foo.jpg`), deducing the format from the file extension.  When it
  finds such a plugin, it creates a subclass instance of ``ImageOutput``
  that writes the right kind of file format.
  ::
      std::unique_ptr<ImageOutput> out = ImageOutput::create (filename);

* Open the file, write the correct headers, and in all other important ways
  prepare a file with the given dimensions (640 x 480), number of color
  channels (3), and data format (unsigned 8-bit integer).
  ::
      ImageSpec spec (xres, yres, channels, TypeDesc::UINT8);
      out->open (filename, spec);

* Write the entire image, hiding all details of the encoding of image data
  in the file, whether the file is scanline- or tile-based, or what is the
  native format of data in the file (in this case, our in-memory data is
  unsigned 8-bit and we've requested the same format for disk storage, but
  if they had been different, ``write_image()`` would do all the conversions
  for us).
  ::
      out->write_image (TypeDesc::UINT8, &pixels);

* Close the file.
  ::
      out->close ();


**What happens when the file format doesn't support the spec?**

The ``open()`` call will fail (returning an empty pointer and set an
appropriate error message) if the output format cannot accommodate what is
requested by the ``ImageSpec``. This includes:

* Dimensions (width, height, or number of channels) exceeding the limits
  supported by the file format.  [#]_
* Volumetric (depth > 1) if the format does not support volumetric data.
* Tile size >1 if the format does not support tiles.
* Multiple subimages or MIP levels if not supported by the format.

.. [#] One exception to the rule about
       number of channels is that a file format that supports only RGB, but
       not alpha, is permitted to silently drop the alpha channel without
       considering that to be an error.

However, several other mismatches between requested ``ImageSpec`` and file
format capabilities will be silently ignored, allowing ``open()`` to
succeed:

* If the pixel data format is not supported (for example, a request for
  ``half`` pixels when writing a JPEG/JFIF file), the format writer
  may substitute another data format (generally, whichever commonly-used
  data format supported by the file type will result in the least reduction
  of precision or range).
* If the ``ImageSpec`` requests different per-channel data formats, but
  the format supports only a single format for all channels, it may just
  choose the most precise format requested and use it for all channels.
* If the file format does not support arbitrarily-named channels, the
  channel names may be lost when saving the file.
* Any other metadata in the ``ImageSpec`` may be summarily dropped if not
  supported by the file format.



Advanced Image Output
=============================

Let's walk through many of the most common things you might want to do, but
that are more complex than the simple example above.

Writing individual scanlines, tiles, and rectangles
---------------------------------------------------

The simple example of Section :ref:`sec-image-output-made-simple` wrote an entire
image with one call.  But sometimes you are generating output a little at a
time and do not wish to retain the entire image in memory until it is time
to write the file.  OpenImageIO allows you to write images one scanline at a
time, one tile at a time, or by individual rectangles.

Writing individual scanlines
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Individual scanlines may be written using the ``writescanline()`` API call::

    ...
    unsigned char scanline[xres*channels];
    out->open (filename, spec);
    int z = 0;   // Always zero for 2D images
    for (int y = 0;  y < yres;  ++y) {
        ... generate data in scanline[0..xres*channels-1] ...
        out->write_scanline (y, z, TypeDesc::UINT8, scanline);
    }
    out->close ();
    ...

The first two arguments to ``writescanline()`` specify which scanline is
being written by its vertical (*y*) scanline number (beginning with 0)
and, for volume images, its slice (*z*) number (the slice number should
be 0 for 2D non-volume images).  This is followed by a `TypeDesc`
describing the data you are supplying, and a pointer to the pixel data
itself.  Additional optional arguments describe the data stride, which
can be ignored for contiguous data (use of strides is explained in
Section :ref:`sec-datastrides`).

All ``ImageOutput`` implementations will accept scanlines in strict order
(starting with scanline 0, then 1, up to ``yres-1``, without skipping
any).  See Section :ref:`sec-imageoutput-random-access-pixels` for details
on out-of-order or repeated scanlines.

The full description of the ``writescanline()`` function may be found
in Section :ref:`sec-imageoutput-class-reference`.

Writing individual tiles
^^^^^^^^^^^^^^^^^^^^^^^^

Not all image formats (and therefore not all ``ImageOutput``
implementations) support tiled images.  If the format does not support
tiles, then ``writetile()`` will fail.  An application using OpenImageIO
should gracefully handle the case that tiled output is not available for
the chosen format.

Once you ``create()`` an ``ImageOutput``, you can ask if it is capable
of writing a tiled image by using the ``supports("tiles")`` query::

    ...
    std::unique_ptr<ImageOutput> out = ImageOutput::create (filename);
    if (! out->supports ("tiles")) {
        // Tiles are not supported}
    

Assuming that the ``ImageOutput`` supports tiled images, you need to
specifically request a tiled image when you ``open()`` the file.  This
is done by setting the tile size in the ``ImageSpec`` passed
to ``open()``.  If the tile dimensions are not set, they will default
to zero, which indicates that scanline output should be used rather than
tiled output.

.. code-block:: cpp

    int tilesize = 64;
    ImageSpec spec (xres, yres, channels, TypeDesc::UINT8);
    spec.tile_width = tilesize;
    spec.tile_height = tilesize;
    out->open (filename, spec);
    ...

In this example, we have used square tiles (the same number of pixels
horizontally and vertically), but this is not a requirement of OpenImageIO.
However, it is possible that some image formats may only support square
tiles, or only certain tile sizes (such as restricting tile sizes to
powers of two).  Such restrictions should be documented by each
individual plugin.

.. code-block:: cpp

    unsigned char tile[tilesize*tilesize*channels];
    int z = 0;   // Always zero for 2D images
    for (int y = 0;  y < yres;  y += tilesize) {
        for (int x = 0;  x < xres;  x += tilesize) {
            ... generate data in tile[] ..
            out->write_tile (x, y, z, TypeDesc::UINT8, tile);
        }
    }
    out->close ();
    ...

The first three arguments to ``writetile()`` specify which tile is being
written by the pixel coordinates of any pixel contained in the tile: *x*
(column), *y* (scanline), and *z* (slice, which should always be 0 for 2D
non-volume images).  This is followed by a `TypeDesc` describing the data
you are supplying, and a pointer to the tile's pixel data itself, which
should be ordered by increasing slice, increasing scanline within each
slice, and increasing column within each scanline. Additional optional
arguments describe the data stride, which can be ignored for contiguous data
(use of strides is explained in Section :ref:`sec-datastrides`).

All ``ImageOutput`` implementations that support tiles will accept tiles in
strict order of increasing *y* rows, and within each row, increasing *x*
column, without missing any tiles.  See

The full description of the ``writetile()`` function may be found
in Section :ref:`sec-imageoutput-class-reference`.

Writing arbitrary rectangles
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Some ``ImageOutput`` implementations --- such as those implementing an
interactive image display, but probably not any that are outputting
directly to a file --- may allow you to send arbitrary rectangular pixel
regions.  Once you ``create()`` an ``ImageOutput``, you can ask if it is
capable of accepting arbitrary rectangles by using the
``supports("rectangles")`` query::

    ...
    std::unique_ptr<ImageOutput> out = ImageOutput::create (filename);
    if (! out->supports ("rectangles")) {
        // Rectangles are not supported
    }

If rectangular regions are supported, they may be sent using
the ``write_rectangle()`` API call::

    unsigned int rect[...];
    ... generate data in rect[] ..
    out->write_rectangle (xbegin, xend, ybegin, yend, zbegin, zend,
                          TypeDesc::UINT8, rect);

The first six arguments to ``write_rectangle()`` specify the region of
pixels that is being transmitted by supplying the minimum and one-past-maximum
pixel indices in *x* (column), *y* (scanline), and *z* (slice, always 0
for 2D non-volume images).

.. note:: OpenImageIO nearly always follows the C++ STL convention of
          specifying ranges as ``[begin,end)``, that is, ``begin, begin+1,
          ..., end-1.``

The total number of pixels being transmitted is therefore::

        (xend-xbegin) * (yend-ybegin) * (zend-zbegin)

This is followed by a `TypeDesc` describing the data you are supplying,
and a pointer to the rectangle's pixel data itself, which should be ordered
by increasing slice, increasing scanline within each slice, and increasing
column within each scanline.  Additional optional arguments describe the
data stride, which can be ignored for contiguous data (use of strides is
explained in Section :ref:`sec-datastrides`).


Converting pixel data types
---------------------------

The code examples of the previous sections all assumed that your
internal pixel data is stored as unsigned 8-bit integers (i.e., 0-255
range).  But OpenImageIO is significantly more flexible.

You may request that the output image pixels be stored in any of several
data types.  This is done by setting the ``format`` field of the
``ImageSpec`` prior to calling ``open``.  You can do this upon
construction of the ``ImageSpec``, as in the following example
that requests a spec that stores pixel values as 16-bit unsigned integers::

    ImageSpec spec (xres, yres, channels, TypeDesc::UINT16);

Or, for an ``ImageSpec`` that has already been constructed, you may reset
its format using the ``set_format()`` method.

.. code-block:: cpp

    ImageSpec spec (...);
    spec.set_format (TypeDesc::UINT16);

Note that resetting the pixel data type must be done *before* passing the
spec to ``open()``, or it will have no effect on the file.

Individual file formats, and therefore ``ImageOutput`` implementations, may
only support a subset of the pixel data types understood by the OpenImageIO
library. Each ``ImageOutput`` plugin implementation should document which
data formats it supports.  An individual ``ImageOutput`` implementation is
expected to always succeed, but if the file format does not support the
requested pixel data type, it is expected to choose a data type that is
supported, usually the data type that best preserves the precision and range
of the originally-requested data type.

The conversion from floating-point formats to integer formats (or from
higher to lower integer, which is done by first converting to float) is
always done by rescaling the value so that 0.0 maps to integer 0 and 1.0 to
the maximum value representable by the integer type, then rounded to an
integer value for final output.  Here is the code that implements this
transformation (``T`` is the final output integer type)::

    float value = quant_max * input;
    T output = (T) clamp ((int)(value + 0.5), quant_min, quant_max);

Quantization limits for each integer type is as follows:

============== ============= ============
  Data Format    **min**       **max**
============== ============= ============
  ``UINT8``               0          255
  ``INT8``             -128          127
  ``UINT16``              0        65535
  ``INT16``          -32768        32767
  ``UINT``                0   4294967295
  ``INT``       -2147483648   2147483647
============== ============= ============


Note that the default is to use the entire positive range of each integer
type to represent the floating-point (0.0 - 1.0) range. Floating-point types
do not attempt to remap values, and do not clamp (except to their full
floating-point range).


It is not required that the pixel data passed to ``writeimage()``,
``writescanline()``, ``writetile()``, or ``write_rectangle()`` actually be
in the same data type as that requested as the native pixel data type of the
file. You can fully mix and match data you pass to the various "write"
routines and OpenImageIO will automatically convert from the internal format
to the native file format.  For example, the following code will open a TIFF
file that stores pixel data as 16-bit unsigned integers (values ranging from
0 to 65535), compute internal pixel values as floating-point values, with
``writeimage()`` performing the conversion automatically::

    std::unique_ptr<ImageOutput> out = ImageOutput::create ("myfile.tif");
    ImageSpec spec (xres, yres, channels, TypeDesc::UINT16);
    out->open (filename, spec);
    ...
    float pixels [xres*yres*channels];
    ...
    out->write_image (TypeDesc::FLOAT, pixels);


Note that ``writescanline()``, ``writetile()``, and ``write_rectangle()``
have a parameter that works in a corresponding manner.


.. _sec-datastrides:

Data Strides
------------

In the preceeding examples, we have assumed that the block of data being
passed to the "write" functions are *contiguous*, that is:

* each pixel in memory consists of a number of data values equal to
  the declared number of channels that are being written to the file;
* successive column pixels within a row directly follow each other in
  memory, with the first channel of pixel *x* immediately following
  last channel of pixel ``x-1`` of the same row;
* for whole images, tiles or rectangles, the data for each row
  immediately follows the previous one in memory (the first pixel of row
  *y* immediately follows the last column of row ``y-1``);
* for 3D volumetric images, the first pixel of slice *z* immediately
  follows the last pixel of of slice ``z-1``.

Please note that this implies that data passed to ``writetile()`` be
contiguous in the shape of a single tile (not just an offset into a whole
image worth of pixels), and that data passed to ``write_rectangle()`` be
contiguous in the dimensions of the rectangle.

The ``writescanline()`` function takes an optional ``xstride`` argument, and
the ``writeimage()``, ``writetile()``, and ``write_rectangle()`` functions
take optional ``xstride``, ``ystride``, and ``zstride`` values that describe
the distance, in *bytes*, between successive pixel columns, rows, and
slices, respectively, of the data you are passing. For any of these values
that are not supplied, or are given as the special constant ``AutoStride``,
contiguity will be assumed.

By passing different stride values, you can achieve some surprisingly
flexible functionality.  A few representative examples follow:

* Flip an image vertically upon writing, by using negative *y* stride::

    unsigned char pixels[xres*yres*channels];
    int scanlinesize = xres * channels * sizeof(pixels[0]);
    ...
    out->write_image (TypeDesc::UINT8,
                      (char *)pixels+(yres-1)*scanlinesize, // offset to last
                      AutoStride,                  // default x stride
                      -scanlinesize,               // special y stride
                      AutoStride);                 // default z stride

* Write a tile that is embedded within a whole image of pixel data, rather
  than having a one-tile-only memory layout::

    unsigned char pixels[xres*yres*channels];
    int pixelsize = channels * sizeof(pixels[0]);
    int scanlinesize = xres * pixelsize;
    ...
    out->write_tile (x, y, 0, TypeDesc::UINT8,
                     (char *)pixels + y*scanlinesize + x*pixelsize,
                     pixelsize,
                     scanlinesize);

* Write only a subset of channels to disk.  In this example, our internal
  data layout consists of 4 channels, but we write just channel 3 to disk as
  a one-channel image::

    // In-memory representation is 4 channel
    const int xres = 640, yres = 480;
    const int channels = 4;  // RGBA
    const int channelsize = sizeof(unsigned char);
    unsigned char pixels[xres*yres*channels];

    // File representation is 1 channel
    std::unique_ptr<ImageOutput> out = ImageOutput::create (filename);
    ImageSpec spec (xres, yres, 1, TypeDesc::UINT8);
    out->open (filename, spec);

    // Use strides to write out a one-channel "slice" of the image
    out->write_image (TypeDesc::UINT8,
                      (char *)pixels+3*channelsize, // offset to chan 3
                      channels*channelsize,         // 4 channel x stride
                      AutoStride,                   // default y stride
                      AutoStride);                  // default z stride
    ...


Please consult Section :ref:`sec-imageoutput-class-reference` for detailed
descriptions of the stride parameters to each "write" function.


Writing a crop window or overscan region
----------------------------------------

The ``ImageSpec`` fields ``width``, ``height``, and ``depth``
describe the dimensions of the actual pixel data.

At times, it may be useful to also describe an abstract *full* or
*display* image window, whose position and size may not correspond
exactly to the data pixels.  For example, a pixel data window that is a
subset of the full display window might indicate a *crop* window; a
pixel data window that is a superset of the full display window might
indicate *overscan* regions (pixels defined outside the eventual
viewport).

The ``ImageSpec`` fields ``full_width``, ``full_height``, and
``full_depth`` describe the dimensions of the full display
window, and ``full_x``, ``full_y``, ``full_z`` describe its
origin (upper left corner).  The fields ``x``, ``y``, ``z``
describe the origin (upper left corner)
of the pixel data.

These fields collectively describe an abstract full display image ranging
from [``full_x`` ... ``full_x+full_width-1``] horizontally, [``full_y`` ...
``full_y+full_height-1``] vertically, and [``full_z`` ...
``full_z+full_depth-1``] in depth (if it is a 3D volume), and actual pixel
data over the pixel coordinate range [``x`` ... ``x+width-1``] horizontally,
[``y`` ... ``y+height-1``] vertically, and [``z`` ... ``z+depth-1``] in
depth (if it is a volume).

Not all image file formats have a way to describe display windows.  An
``ImageOutput`` implementation that cannot express display windows will
always write out the ``width * height`` pixel data, may
upon writing lose information about offsets or crop windows.

Here is a code example that opens an image file that will contain a 32x32
pixel crop window within an abstract 640 x 480 full size image.
Notice that the pixel indices (column, scanline, slice) passed to the
"write" functions are the coordinates relative to the full image, not
relative to the crop widow, but the data pointer passed to the "write"
functions should point to the beginning of the actual pixel data being
passed (not the the hypothetical start of the full data, if it was all
present).

.. code-block:: cpp

    int fullwidth = 640, fulllength = 480; // Full display image size
    int cropwidth = 16, croplength = 16;  // Crop window size
    int xorigin = 32, yorigin = 128;      // Crop window position
    unsigned char pixels [cropwidth * croplength * channels]; // Crop size!
    ...
    std::unique_ptr<ImageOutput> out = ImageOutput::create (filename);
    ImageSpec spec (cropwidth, croplength, channels, TypeDesc::UINT8);
    spec.full_x = 0;
    spec.full_y = 0;
    spec.full_width = fullwidth;
    spec.full_length = fulllength;
    spec.x = xorigin;
    spec.y = yorigin;
    out->open (filename, spec);
    ...
    int z = 0;   // Always zero for 2D images
    for (int y = yorigin;  y < yorigin+croplength;  ++y) {
        out->write_scanline (y, z, TypeDesc::UINT8,
                             (y-yorigin)*cropwidth*channels);
    }
    out->close ();



Writing metadata
----------------

The ``ImageSpec`` passed to ``open()`` can specify all the common
required properties that describe an image: data format, dimensions,
number of channels, tiling.  However, there may be a variety of
additional *metadata* that should be carried along with the
image or saved in the file.

.. note:: *Metadata* refers to data about data, in this case, data about the
          image that goes beyond the pixel values and description thereof.

The remainder of this section explains how to store additional metadata
in the ``ImageSpec``.  It is up to the ``ImageOutput`` to store these
in the file, if indeed the file format is able to accept the data.
Individual ``ImageOutput`` implementations should document which metadata
they respect.

Channel names
^^^^^^^^^^^^^

In addition to specifying the number of color channels, it is also possible
to name those channels.  Only a few ``ImageOutput`` implementations have a
way of saving this in the file, but some do, so you may as well do it if you
have information about what the channels represent.

By convention, channel names for red, green, blue, and alpha (or a main
image) should be named ``"R"``, ``"G"``, ``"B"``, and ``"A"``,
respectively.  Beyond this guideline, however, you can use any names you
want.

The ``ImageSpec`` has a vector of strings called ``channelnames``.  Upon
construction, it starts out with reasonable default values.  If you use it
at all, you should make sure that it contains the same number of strings as
the number of color channels in your image.  Here is an example::

        int channels = 4;
        ImageSpec spec (width, length, channels, TypeDesc::UINT8);
        spec.channelnames.clear ();
        spec.channelnames.push_back ("R");
        spec.channelnames.push_back ("G");
        spec.channelnames.push_back ("B");
        spec.channelnames.push_back ("A");

Here is another example in which custom channel names are used to label the
channels in an 8-channel image containing beauty pass RGB, per-channel
opacity, and texture s,t coordinates for each pixel.

.. code-block::

        int channels = 8;
        ImageSpec spec (width, length, channels, TypeDesc::UINT8);
        spec.channelnames.clear ();
        spec.channelnames.push_back ("R");
        spec.channelnames.push_back ("G");
        spec.channelnames.push_back ("B");
        spec.channelnames.push_back ("opacityR");
        spec.channelnames.push_back ("opacityG");
        spec.channelnames.push_back ("opacityB");
        spec.channelnames.push_back ("texture_s");
        spec.channelnames.push_back ("texture_t");

The main advantage to naming color channels is that if you are saving to
a file format that supports channel names, then any application that
uses OpenImageIO to read the image back has the option to retain those
names and use them for helpful purposes.  For example, the :file:`iv`
image viewer will display the channel names when viewing individual
channels or displaying numeric pixel values in "pixel view" mode.


Specially-designated channels
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ``ImageSpec`` contains two fields, ``alpha_channel`` and ``z_channel``,
which can be used to designate which channel indices are used for alpha and
*z* depth, if any.  Upon construction, these are both set to ``-1``,
indicating that it is not known which channels are alpha or depth.  Here is
an example of setting up a 5-channel output that represents RGBAZ::

    int channels = 5;
    ImageSpec spec (width, length, channels, format);
    spec.channelnames.clear();
    spec.channelnames.push_back ("R");
    spec.channelnames.push_back ("G");
    spec.channelnames.push_back ("B");
    spec.channelnames.push_back ("A");
    spec.channelnames.push_back ("Z");
    spec.alpha_channel = 3;
    spec.z_channel = 4;

There are advantages to designating the alpha and depth channels in this
manner: Some file formats may require that these channels be stored in a
particular order, with a particular precision, or the ``ImageOutput`` may in
some other way need to know about these special channels.

Arbitrary metadata
^^^^^^^^^^^^^^^^^^

For all other metadata that you wish to save in the file, you can attach the
data to the ``ImageSpec`` using the ``attribute()`` methods. These come in
polymorphic varieties that allow you to attach an attribute name and a value
consisting of a single `int`, ``unsigned int``, `float`, ``char*``, or
``std::string``, as shown in the following examples::

        ImageSpec spec (...);
        ...

        unsigned int u = 1;
        spec.attribute ("Orientation", u);

        float x = 72.0;
        spec.attribute ("dotsize", f);

        std::string s = "Fabulous image writer 1.0";
        spec.attribute ("Software", s);

These are convenience routines for metadata that consist of a single value
of one of these common types.  For other data types, or more complex
arrangements, you can use the more general form of ``attribute()``, which
takes arguments giving the name, type (as a `TypeDesc`), number of values
(1 for a single value, >1 for an array), and then a pointer to the data
values.  For example,

.. code-block::

        ImageSpec spec (...);

        // Attach a 4x4 matrix to describe the camera coordinates
        float mymatrix[16] = { ... };
        spec.attribute ("worldtocamera", TypeMatrix, &mymatrix);

        // Attach an array of two floats giving the CIE neutral color
        float neutral[2] = { ... };
        spec.attribute ("adoptedNeutral", TypeDesc(TypeDesc::FLOAT, 2), &neutral);


In general, most image file formats (and therefore most ``ImageOutput``
implementations) are aware of only a small number of name/value pairs
that they predefine and will recognize.  Some file formats (OpenEXR,
notably) do accept arbitrary user data and save it in the image file.
If an ``ImageOutput`` does not recognize your metadata and does not support
arbitrary metadata, that metadatum will be silently ignored and will not
be saved with the file.

Each individual ``ImageOutput`` implementation should document the names,
types, and meanings of all metadata attributes that they understand.


Color space hints
^^^^^^^^^^^^^^^^^

We certainly hope that you are using only modern file formats that
support high precision and extended range pixels (such as OpenEXR) and
keeping all your images in a linear color space.  But you may have to
work with file formats that dictate the use of nonlinear color values.
This is prevalent in formats that store pixels only as 8-bit values,
since 256 values are not enough to linearly represent colors without
banding artifacts in the dim values.

Since this can (and probably will) happen, we have a convention
for explaining what color space your image pixels are
in.  Each individual ``ImageOutput`` should document how it uses this (or
not).

The ``ImageSpec::extra_attribs`` field should store metadata that reveals
the color space of the pixels you are sending the ImageOutput (see Section
`Color information metadata` for explanations of particular values).

The color space hints only describe color channels.  You should always pass
alpha, depth, or other non-color channels with linear values.

Here is a simple example of setting up the ``ImageSpec`` when you know that
the pixel values you are writing are linear::

    ImageSpec spec (width, length, channels, format);
    spec.attribute ("oiio:ColorSpace", "Linear");
    ...


If a particular ``ImageOutput`` implementation is required (by the rules of
the file format it writes) to have pixels in a particular color space,
then it should try to convert the color values of your image to the right color
space if it is not already in that space.  For example, JPEG images
must be in sRGB space, so if you declare your pixels to be ``"Linear"``,
the JPEG ``ImageOutput`` will convert to sRGB.

If you leave the ``"oiio:ColorSpace"`` unset, the values will not be
transformed, since the plugin can't be sure that it's not in the correct
space to begin with.



.. _sec-imageoutput-random-access-pixels:

Random access and repeated transmission of pixels
-------------------------------------------------

All ``ImageOutput`` implementations that support scanlines and tiles should
write pixels in strict order of increasing *z* slice, increasing *y*
scanlines/rows within each slice, and increasing *x* column within each row.
It is generally not safe to skip scanlines or tiles, or transmit them out of
order, unless the plugin specifically advertises that it supports random
access or rewrites, which may be queried using::

    std::unique_ptr<ImageOutput> out = ImageOutput::create (filename);
    if (out->supports ("random_access"))
        ...

Similarly, you should assume the plugin will not correctly
handle repeated transmissions of a scanline or tile that has already
been sent, unless it advertises that it supports rewrites, which may be
queried using::

    if (out->supports ("rewrite"))
        ...



Multi-image files
-----------------

Some image file formats support storing multiple images within a single
file.  Given a created ``ImageOutput``, you can query whether multiple
images may be stored in the file::

        std::unique_ptr<ImageOutput> out = ImageOutput::create (filename);
        if (out->supports ("multiimage"))
            ...

Some image formats allow you to do the initial ``open()`` call without
declaring the specifics of the subimages, and simply append subimages as you
go.  You can detect this by checking

.. code-block::

        if (out->supports ("appendsubimage"))
            ...


In this case, all you have to do is, after writing all the pixels of one
image but before calling ``close()``, call ``open()`` again for the next
subimage and pass ``AppendSubimage`` as the value for the *mode* argument
(see Section :ref:`sec-imageoutput-class-reference` for the full technical
description of the arguments to ``open()``).  The ``close()`` routine is
called just once, after all subimages are completed.  Here is an example::

    const char *filename = "foo.tif";
    int nsubimages;     // assume this is set
    ImageSpec specs[];  // assume these are set for each subimage
    unsigned char *pixels[]; // assume a buffer for each subimage

    // Create the ImageOutput
    std::unique_ptr<ImageOutput> out = ImageOutput::create (filename);

    // Be sure we can support subimages
    if (subimages > 1 &&  (! out->supports("multiimage") ||
                           ! out->supports("appendsubimage"))) {
        std::cerr << "Does not support appending of subimages\n";
        return;
    }

    // Use Create mode for the first level.
    ImageOutput::OpenMode appendmode = ImageOutput::Create;

    // Write the individual subimages
    for (int s = 0;  s < nsubimages;  ++s) {
        out->open (filename, specs[s], appendmode);
        out->write_image (TypeDesc::UINT8, pixels[s]);
        // Use AppendSubimage mode for subsequent levels
        appendmode = ImageOutput::AppendSubimage;
    }
    out->close ();

On the other hand, if ``out->supports("appendsubimage")`` returns
`false`, then you must use a different ``open()`` variety that
allows you to declare the number of subimages and their specifications
up front.

Below is an example of how to write a multi-subimage file, assuming that
you know all the image specifications ahead of time.  This should be
safe for any file format that supports multiple subimages, regardless of
whether it supports appending, and thus is the preferred method for
writing subimages, assuming that you are able to know the number and
specification of the subimages at the time you first open the file.

.. code-block::

    const char *filename = "foo.tif";
    int nsubimages;     // assume this is set
    ImageSpec specs[];  // assume these are set for each subimage
    unsigned char *pixels[]; // assume a buffer for each subimage

    // Create the ImageOutput
    std::unique_ptr<ImageOutput> out = ImageOutput::create (filename);

    // Be sure we can support subimages
    if (subimages > 1 && ! out->supports ("multiimage")) {
        std::cerr << "Cannot write multiple subimages\n";
        return;
    }

    // Open and declare all subimages
    out->open (filename, nsubimages, specs);

    // Write the individual subimages
    for (int s = 0;  s < nsubimages;  ++s) {
        if (s > 0)  // Not needed for the first, which is already open
            out->open (filename, specs[s], ImageInput::AppendSubimage);
        out->write_image (TypeDesc::UINT8, pixels[s]);
    }
    out->close ();


In both of these examples, we have used ``writeimage()``, but of course
``writescanline()``, ``writetile()``, and ``write_rectangle()`` work as you
would expect, on the current subimage.


.. _sec-imageoutput-mipmap:

MIP-maps
--------

Some image file formats support multiple copies of an image at successively
lower resolutions (MIP-map levels, or an "image pyramid").  Given a created
``ImageOutput``, you can query whether MIP-maps may be stored in the file::

    std::unique_ptr<ImageOutput> out = ImageOutput::create (filename);
    if (out->supports ("mipmap"))
        ...

If you are working with an ``ImageOutput`` that supports MIP-map levels, it
is easy to write these levels.  After writing all the pixels of one MIP-map
level, call ``open()`` again for the next MIP level and pass
``ImageInput::AppendMIPLevel`` as the value for the *mode* argument, and
then write the pixels of the subsequent MIP level. (See Section
:ref:`sec-imageoutput-class-reference` for the full technical description of
the arguments to ``open()``.)  The ``close()`` routine is called just once,
after all subimages and MIP levels are completed.

Below is pseudocode for writing a MIP-map (a multi-resolution image
used for texture mapping)::

    const char *filename = "foo.tif";
    const int xres = 512, yres = 512;
    const int channels = 3;  // RGB
    unsigned char *pixels = new unsigned char [xres*yres*channels];

    // Create the ImageOutput
    std::unique_ptr<ImageOutput> out = ImageOutput::create (filename);

    // Be sure we can support either mipmaps or subimages
    if (! out->supports ("mipmap") && ! out->supports ("multiimage")) {
        std::cerr << "Cannot write a MIP-map\n";
        return;
    }
    // Set up spec for the highest resolution
    ImageSpec spec (xres, yres, channels, TypeDesc::UINT8);

    // Use Create mode for the first level.
    ImageOutput::OpenMode appendmode = ImageOutput::Create;

    // Write images, halving every time, until we're down to
    // 1 pixel in either dimension
    while (spec.width >= 1 && spec.height >= 1) {
        out->open (filename, spec, appendmode);
        out->write_image (TypeDesc::UINT8, pixels);
        // Assume halve() resamples the image to half resolution
        halve (pixels, spec.width, spec.height);
        // Don't forget to change spec for the next iteration
        spec.width /= 2;
        spec.height /= 2;

        // For subsequent levels, change the mode argument to
        // open().  If the format doesn't support MIPmaps directly,
        // try to emulate it with subimages.
        if (out->supports("mipmap"))
            appendmode = ImageOutput::AppendMIPLevel;
        else
            appendmode = ImageOutput::AppendSubimage;
    }
    out->close ();


In this example, we have used ``writeimage()``, but of course
``writescanline()``, ``writetile()``, and ``write_rectangle()`` work as you
would expect, on the current MIP level.


Per-channel formats
-------------------

Some image formats allow separate per-channel data formats (for example,
``half`` data for colors and `float` data for depth).  When this
is desired, the following steps are necessary:

1. Verify that the writer supports per-channel formats by checking
   ``supports ("channelformats")``.
2. The ``ImageSpec`` passed to ``open()`` should have its
   ``channelformats`` vector filled with the types for each channel.
3. The call to ``write_scanline()``, ``read_scanlines()``, ``write_tile()``,
   ``write_tiles()``, or ``write_image()`` should pass a ``data`` pointer
   to the raw data, already in the native per-channel format of the file and
   contiguously packed, and specify that the data is of type ``TypeUnknown``.

For example, the following code fragment will write a 5-channel image
to an OpenEXR file, consisting of R/G/B/A channels in ``half`` and
a Z channel in `float`::

        // Mixed data type for the pixel
        struct Pixel { half r,g,b,a; float z; };
        Pixel pixels[xres*yres];

        std::unique_ptr<ImageOutput> out = ImageOutput::create ("foo.exr");

        // Double check that this format accepts per-channel formats
        if (! out->supports("channelformats")) {
            return;
        }

        // Prepare an ImageSpec with per-channel formats
        ImageSpec spec (xres, yres, 5, TypeDesc::FLOAT);
        spec.channelformats.push_back (TypeDesc::HALF);
        spec.channelformats.push_back (TypeDesc::HALF);
        spec.channelformats.push_back (TypeDesc::HALF);
        spec.channelformats.push_back (TypeDesc::HALF);
        spec.channelformats.push_back (TypeDesc::FLOAT);
        spec.channelnames.clear ();
        spec.channelnames.push_back ("R");
        spec.channelnames.push_back ("G");
        spec.channelnames.push_back ("B");
        spec.channelnames.push_back ("A");
        spec.channelnames.push_back ("Z");

        out->open (filename, spec);
        out->write_image (TypeDesc::UNKNOWN, /* use channel formats */
                          pixels,            /* data buffer */
                          sizeof(Pixel));    /* pixel stride */



Writing "deep" data
-------------------

Some image file formats (OpenEXR only, at this time) support the concept
of "deep" pixels -- those containing multiple samples per pixel (and a
potentially differing number of them in each pixel).  You can tell
if a format supports deep images by checking ``supports("deepdata")``,
and you can specify a deep data in an ``ImageSpec`` by setting its ``deep``
field will be `true`.

Deep files cannot be written with the usual ``write_scanline()``,
``write_scanlines()``, ``write_tile()``, ``write_tiles()``, ``write_image()``
functions, due to the nature of their variable number of samples per
pixel.  Instead, ``ImageOutput`` has three special member functions used
only for writing deep data::

    bool write_deep_scanlines (int ybegin, int yend, int z,
                               const DeepData &deepdata);

    bool write_deep_tiles (int xbegin, int xend, int ybegin, int yend,
                           int zbegin, int zend, const DeepData &deepdata);

    bool write_deep_image (const DeepData &deepdata);

It is only possible to write "native" data types to deep files; that
is, there is no automatic translation into arbitrary data types as there
is for ordinary images.  All three of these functions are passed
deep data in a special DeepData structure, described in
detail in Section :ref:`sec-DeepData`.


Here is an example of using these methods to write a deep image::

    // Prepare the spec for 'half' RGBA, 'float' z
    int nchannels = 5;
    ImageSpec spec (xres, yres, nchannels);
    TypeDesc channeltypes[] = { TypeDesc::HALF, TypeDesc::HALF,
          TypeDesc::HALF, TypeDesc::HALF, TypeDesc::FLOAT };
    spec.z_channel = 4;
    spec.channelnames[spec.z_channel] = "Z";
    spec.channeltypes.assign (channeltypes+0, channeltypes+nchannels);
    spec.deep = true;

    // Prepare the data (sorry, complicated, but need to show the gist)
    DeepData deepdata;
    deepdata.init (spec);
    for (int y = 0;  y < yres;  ++y)
        for (int x = 0;  x < xres;  ++x)
            deepdata.set_samples(y*xres+x, ...samples for that pixel...);
    deepdata.alloc ();  // allocate pointers and data
    int pixel = 0;
    for (int y = 0;  y < yres;  ++y)
        for (int x = 0;  x < xres;  ++x, ++pixel)
            for (int chan = 0;  chan < nchannels;  ++chan)
                for (int samp = 0; samp < deepdata.samples(pixel); ++samp)
                    deepdata.set_deep_value (pixel, chan, samp, ...value...);


    // Create the output
    std::unique_ptr<ImageOutput> out = ImageOutput::create (filename);
    if (! out)
        return;
    // Make sure the format can handle deep data and per-channel formats
    if (! out->supports("deepdata") || ! out->supports("channelformats"))
        return;

    // Do the I/O (this is the easy part!)
    out->open (filename, spec);
    out->write_deep_image (deepdata);
    out->close ();



Copying an entire image
-----------------------

Suppose you want to copy an image, perhaps with alterations to the metadata
but not to the pixels.  You could open an ``ImageInput`` and perform a
``read_image()``, and open another ``ImageOutput`` and call
``write_image()`` to output the pixels from the input image. However, for
compressed images, this may be inefficient due to the unnecessary
decompression and subsequent re-compression.  In addition, if the
compression is *lossy*, the output image may not contain pixel values
identical to the original input.

A special ``copy_image()`` method of ``ImageOutput`` is available that
attempts to copy an image from an open ``ImageInput`` (of the same format)
to the output as efficiently as possible with without altering pixel values,
if at all possible.

Not all format plugins will provide an implementation of ``copy_image()``
(in fact, most will not), but the default implemenatation simply copies
pixels one scanline or tile at a time (with decompression/recompression) so
it's still safe to call.  Furthermore, even a provided ``copy_image()`` is
expected to fall back on the default implementation if the input and output
are not able to do an efficient copy.  Nevertheless, this method is
recommended for copying images so that maximal advantage will be taken in
cases where savings can be had.

The following is an example use of ``copy_image()`` to transfer pixels
without alteration while modifying the image description metadata::

    // Open the input file
    const char *input = "input.jpg";
    std::unique_ptr<ImageInput> in = ImageInput::open (input);

    // Make an output spec, identical to the input except for metadata
    ImageSpec out_spec = in->spec();
    out_spec.attribute ("ImageDescription", "My Title");

    // Create the output file and copy the image
    const char *output = "output.jpg";
    std::unique_ptr<ImageOutput> out = ImageOutput::create (output);
    out->open (output, out_spec);
    out->copy_image (in);

    // Clean up
    out->close ();
    in->close ();



.. _sec-imageoutput-writefiletomemory:

Custom I/O proxies (and writing the file to a memory buffer)
------------------------------------------------------------

Some file format writers allow you to supply a custom I/O proxy object that
can allow bypassing the usual file I/O with custom behavior, including the
ability to fill an in-memory buffer with a byte-for-byte representation of
the correctly formatted file that would have been written to disk.

Only some output format writers support this feature. To find out if a
particular file format supports this feature, you can create an ``ImageOutput``
of the right type, and check if it supports the feature name ``"ioproxy"``::

    auto out = ImageOutput::create (filename);
    if (! out  ||  ! out->supports ("ioproxy")) {
        return;
    }

``ImageOutput`` writers that support ``"ioproxy"`` will respond to a special
attribute, ``"oiio:ioproxy"``, which passes a pointer to a
``Filesystem::IOProxy*`` (see OpenImageIO's :file:`filesystem.h` for this
type and its subclasses). ``IOProxy`` is an abstract type, and concrete
subclasses include ``IOFile`` (which wraps I/O to an open ``FILE*``) and
``IOVecOutput`` (which sends output to a ``std::vector<unsigned char>``).

Here is an example of using a proxy that writes the "file" to a
``std::vector<unsigned char>``::

    // ImageSpec describing the image we want to write.
    ImageSpec spec (xres, yres, channels, TypeDesc::UINT8);

    std::vector<unsigned char> file_buffer;  // bytes will go here
    Filesystem::IOVecOutput vecout (file_buffer);  // I/O proxy object

    auto out = ImageOutput::create ("out.exr", &vecout);
    out->open ("out.exr", spec);
    out->write_image (...);
    out->close ();

    // At this point, file_buffer will contain the "file"



Custom search paths for plugins
-------------------------------

When you call ``ImageOutput::create()``, the OpenImageIO library will try to
find a plugin that is able to write the format implied by your filename.
These plugins are alternately known as DLL's on Windows (with the ``.dll``
extension), DSO's on Linux (with the ``.so`` extension), and dynamic
libraries on Mac OS X (with the ``.dylib`` extension).

OpenImageIO will look for matching plugins according to *search paths*,
which are strings giving a list of directories to search, with each
directory separated by a colon ``:``.  Within a search path, any substrings
of the form ``{$FOO}`` will be replaced by the value of environment variable
``FOO``.  For example, the searchpath ``"${HOME}/plugins:/shared/plugins"``
will first check the directory :file:`/home/tom/plugins` (assuming the
user's home directory is :file:`/home/tom`), and if not found there, will
then check the directory :file:`/shared/plugins`.

The first search path it will check is that stored in the environment
variable ``OIIO_LIBRARY_PATH``.  It will check each directory in turn, in
the order that they are listed in the variable.  If no adequate plugin is
found in any of the directories listed in this environment variable, then it
will check the custom searchpath passed as the optional second argument to
``ImageOutput::create()``, searching in the order that the directories are
listed.  Here is an example::

    char *mysearch = "/usr/myapp/lib:${HOME}/plugins";
    std::unique_ptr<ImageOutput> out = ImageOutput::create (filename, mysearch);
    ...



Error checking
--------------

Nearly every ``ImageOutput`` API function returns a ``bool`` indicating
whether the operation succeeded (`true`) or failed (`false`). In the
case of a failure, the ``ImageOutput`` will have saved an error message
describing in more detail what went wrong, and the latest error message is
accessible using the ``ImageOutput`` method ``geterror()``, which returns
the message as a `std::string`.

The exception to this rule is ``ImageOutput::create()``, which returns
``NULL`` if it could not create an appropriate ``ImageOutput``.  And in this
case, since no ``ImageOutput`` exists for which you can call its
``geterror()`` function, there exists a global ``geterror()`` function (in
the ``OpenImageIO`` namespace) that retrieves the latest error message
resulting from a call to ``create()``.

Here is another version of the simple image writing code from Section
:ref:`sec-image-output-made-simple`, but this time it is fully elaborated with
error checking and reporting::

    #include <OpenImageIO/imageio.h>
    using namespace OIIO;
    ...

    const char *filename = "foo.jpg";
    const int xres = 640, yres = 480;
    const int channels = 3;  // RGB
    unsigned char pixels[xres*yres*channels];

    std::unique_ptr<ImageOutput> out = ImageOutput::create (filename);
    if (! out) {
        std::cerr << "Could not create an ImageOutput for "
                  << filename << ", error = "
                  << OpenImageIO::geterror() << "\n";
        return;
    }
    ImageSpec spec (xres, yres, channels, TypeDesc::UINT8);

    if (! out->open (filename, spec)) {
        std::cerr << "Could not open " << filename
                  << ", error = " << out->geterror() << "\n";
        return;
    }

    if (! out->write_image (TypeDesc::UINT8, pixels)) {
        std::cerr << "Could not write pixels to " << filename
                  << ", error = " << out->geterror() << "\n";
        return;
    }

    if (! out->close ()) {
        std::cerr << "Error closing " << filename
                  << ", error = " << out->geterror() << "\n";
        return;
    }




.. _sec-imageoutput-class-reference:

ImageOutput Class Reference
=============================

.. doxygenclass:: OIIO::ImageOutput
    :members:
