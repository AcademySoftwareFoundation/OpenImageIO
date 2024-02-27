..
  Copyright Contributors to the OpenImageIO project.
  SPDX-License-Identifier: CC-BY-4.0


.. _chap-imageinput:

ImageInput: Reading Images
##############################


.. _sec-imageinput-made-simple:

Image Input Made Simple
===========================

Here is the simplest sequence required to open an image file, find out its
resolution, and read the pixels (converting them into 8-bit values in
memory, even if that's not the way they're stored in the file):

.. tabs::

    .. tab:: C++
        .. literalinclude:: ../../testsuite/docs-examples-cpp/src/docs-examples-imageinput.cpp
            :language: c++
            :start-after: BEGIN-imageinput-simple
            :end-before: END-imageinput-simple

    .. tab:: Python
        .. literalinclude:: ../../testsuite/docs-examples-python/src/docs-examples-imageinput.py
            :language: py
            :start-after: BEGIN-imageinput-simple
            :end-before: END-imageinput-simple

Here is a breakdown of what work this code is doing:

* Search for an ImageIO plugin that is capable of reading the file
  (:file:`foo.jpg`), first by trying to deduce the correct plugin from the
  file extension, but if that fails, by opening every ImageIO plugin it can
  find until one will open the file without error.  When it finds the right
  plugin, it creates a subclass instance of ImageInput that reads the right
  kind of file format, and tries to fully open the file. The ``open()``
  method returns a ``std::unique_ptr<ImageInput>`` that will be
  automatically freed when it exits scope.

  .. tabs::

     .. code-tab:: c++

        auto inp = ImageInput::open (filename);

     .. code-tab:: py

        inp = ImageInput.open (filename)

* The specification, accessible as ``inp->spec()``, contains vital
  information such as the dimensions of the image, number of color channels,
  and data type of the pixel values.  This is enough to allow us to allocate
  enough space for the image in C++ (for Python, this is unnecessary, since
  `read_image()` will return a NumPy array to us).

  .. tabs::

     .. code-tab:: c++

        const ImageSpec &spec = inp->spec();
        int xres = spec.width;
        int yres = spec.height;
        int nchannels = spec.nchannels;
        auto pixels = std::unique_ptr<unsigned char[]>(new unsigned char[xres * yres * nchannels]);

     .. code-tab:: py

        spec = inp.spec()
        xres = spec.width
        yres = spec.height
        channels = spec.nchannels

  Note that in this example, we don't care what data format is used for the
  pixel data in the file --- we will request unsigned 8 bit integers and rely
  on OpenImageIO's ability to convert to our requested format from the native
  data format of the file.

* Read the entire image, hiding all details of the encoding of image data in
  the file, whether the file is scanline- or tile-based, or what is the
  native format of the data in the file (in this case, we request that it be
  automatically converted to unsigned 8-bit integers). Note that the exact set
  of channels are specified as well as the explicit subimage and  miplevel for
  efficiency and thread-safety.

  .. tabs::

     .. code-tab:: c++

        inp->read_image(0, 0, 0, nchannels, TypeDesc::UINT8, &pixels[0]);

     .. code-tab:: py

        pixels = inp->read_image(0, 0, 0, nchannels, "uint8")
        # Note: pixels now contains a NumPy array of the image data.

* Close the file.

  .. tabs::

     .. code-tab:: c++

        inp->close();

     .. code-tab:: py

        inp.close()

* When ``inp`` exits its scope, the ImageInput will automatically be destroyed
  and any resources used by the plugin will be released.



Advanced Image Input
===========================

Let's walk through some of the most common things you might want to do,
but that are more complex than the simple example above.


Reading individual scanlines and tiles
--------------------------------------

The simple example of Section :ref:`sec-imageinput-made-simple` read an entire
image with one call.  But sometimes you want to read a large image a little
at a time and do not wish to retain the entire image in memory as you
process it.  OpenImageIO allows you to read images one scanline at a time or
one tile at a time.

Examining the ImageSpec reveals whether the file is scanline or
tile-oriented: a scanline image will have ``spec.tile_width`` and
``spec.tile_height`` set to 0, whereas a tiled images will have nonzero
values for the tile dimensions.


Reading scanlines
^^^^^^^^^^^^^^^^^^^^^^^^

Individual scanlines may be read using the ``read_scanline()`` API call:

.. tabs::

    .. code-tab:: c++

        auto inp = ImageInput::open (filename);
        const ImageSpec &spec = inp->spec();
        if (spec.tile_width == 0) {
            auto scanline = std::unique_ptr<unsigned char[]>(new unsigned char[spec.width * spec.nchannels]);
            for (int y = 0;  y < yres;  ++y) {
                inp->read_scanline (y, 0, TypeDesc::UINT8, &scanline[0]);
                // ... process data in scanline[0..width*channels-1] ...
            }
        } else {
             //... handle tiles, or reject the file ...
        }
        inp->close ();

    .. code-tab:: py

        inp = ImageInput.open (filename)
        spec = inp.spec()
        if spec.tile_width == 0 :
            for y in range(yres) :
                scanline = inp.read_scanline (y, 0, "uint8")
                # ... process data in scanline[0..width*channels-1] ...
        else :
            # ... handle tiles, or reject the file ...
        inp.close ()

The first two arguments to ``read_scanline()`` specify which scanline
is being read by its vertical (``y``) scanline number (beginning with 0)
and, for volume images, its slice (``z``) number (the slice number should
be 0 for 2D non-volume images).  This is followed by a `TypeDesc`
describing the data type of the pixel buffer you are supplying, and a
pointer to the pixel buffer itself.  Additional optional arguments
describe the data stride, which can be ignored for contiguous data (use
of strides is explained in Section :ref:`sec-imageinput-data-strides`).

Nearly all ImageInput implementations will be most efficient reading
scanlines in strict order (starting with scanline 0, then 1, up to
``yres-1``, without skipping any).  An ImageInput is required to accept
``read_scanline()`` requests in arbitrary order, but depending on the file
format and reader implementation, out-of-order scanline reads may be
inefficient.

There is also a ``read_scanlines()`` function that operates similarly,
except that it takes a ``ybegin`` and ``yend`` that specify a range,
reading all scanlines ``ybegin <= y < yend``.  For most image
format readers, this is implemented as a loop over individual scanlines,
but some image format readers may be able to read a contiguous block of
scanlines more efficiently than reading each one individually.

The full descriptions of the ``read_scanline()`` and ``read_scanlines()``
functions may be found in Section :ref:`sec-imageinput-class-reference`.

Reading tiles
^^^^^^^^^^^^^^^^^^^^^^^^

Once you ``open()`` an image file, you can find out if it is a tiled image
(and the tile size) by examining the ImageSpec's ``tile_width``,
``tile_height``, and ``tile_depth`` fields. If they are zero, it's a
scanline image and you should read pixels using ``read_scanline()``, not
``read_tile()``.

.. tabs::

    .. code-tab:: c++

        auto inp = ImageInput::open(filename);
        const ImageSpec &spec = inp->spec();
        if (spec.tile_width == 0) {
            // ... read scanline by scanline ...
        } else {
            // Tiles
            int tilesize = spec.tile_width * spec.tile_height;
            auto tile = std::unique_ptr<unsigned char[]>(new unsigned char[tilesize * spec.nchannels]);
            for (int y = 0;  y < yres;  y += spec.tile_height) {
                for (int x = 0;  x < xres;  x += spec.tile_width) {
                    inp->read_tile(x, y, 0, TypeDesc::UINT8, &tile[0]);
                    // ... process the pixels in tile[] ..
                }
            }
        }
        inp->close ();

    .. code-tab:: py

        inp = ImageInput.open(filename)
        spec = inp.spec()
        if spec.tile_width == 0 :
            # ... read scanline by scanline ...
        else :
            # Tiles
            tilesize = spec.tile_width * spec.tile_height;
            for y in range(0, yres, spec.tile_height) :
                for x in range(0, xres, spec.tile_width) :
                    tile = inp.read_tile (x, y, 0, "uint8")
                    # ... process the pixels in tile[][] ..
        inp.close ();

The first three arguments to ``read_tile()`` specify which tile is
being read by the pixel coordinates of any pixel contained in the
tile: ``x`` (column), ``y`` (scanline), and ``z`` (slice, which should always
be 0 for 2D non-volume images).  This is followed by a `TypeDesc`
describing the data format of the pixel buffer you are supplying, and a
pointer to the pixel buffer.  Pixel data will be written to your buffer
in order of increasing slice, increasing
scanline within each slice, and increasing column within each scanline.
Additional optional arguments describe the data stride, which can be
ignored for contiguous data (use of strides is explained in
Section :ref:`sec-imageinput-data-strides`).

All ImageInput implementations are required to support reading tiles in
arbitrary order (i.e., not in strict order of increasing ``y`` rows, and
within each row, increasing ``x`` column, without missing any tiles).

The full description of the ``read_tile()`` function may be found
in Section :ref:`sec-imageinput-class-reference`.


Converting formats
--------------------------------

The code examples of the previous sections all assumed that your internal
pixel data is stored as unsigned 8-bit integers (i.e., 0-255 range).  But
OpenImageIO is significantly more flexible.

You may request that the pixels be stored in any of several formats. This is
done merely by passing the ``read`` function the data type of your pixel
buffer, as one of the enumerated type `TypeDesc`.

It is not required that the pixel data buffer passed to ``read_image()``,
``read_scanline()``, or ``read_tile()`` actually be in the same data format
as the data in the file being read.  OpenImageIO will automatically convert
from native data type of the file to the internal data format of your
choice. For example, the following code will open a TIFF and read pixels
into your internal buffer represented as `float` values.  This will work
regardless of whether the TIFF file itself is using 8-bit, 16-bit, or float
values.

.. tabs::

    .. code-tab:: c++

        std::unique_ptr<ImageInput> inp = ImageInput::open ("myfile.tif");
        const ImageSpec &spec = inp->spec();

        int numpixels = spec.image_pixels();
        int nchannels = spec.nchannels;
        auto pixels = std::unique_ptr<float[]>(new float[numpixels * nchannels]);

        inp->read_image (0, 0, 0, nchannels, TypeDesc::FLOAT, &pixels[0]);

    .. code-tab:: py

        inp = ImageInput.open("myfile.tif")
        pixels = inp.read_image(0, 0, 0, nchannels, "float")


Note that ``read_scanline()`` and ``read_tile()`` have a parameter that
works in a corresponding manner.

You can, of course, find out the native type of the file simply by examining
``spec.format``.  If you wish, you may then allocate a buffer big enough for
an image of that type and request the native type when reading, therefore
eliminating any translation among types and seeing the actual numerical
values in the file.


.. _sec-imageinput-data-strides:

Data Strides
--------------------------------

In the preceding examples, we have assumed that the buffer passed to
the ``read`` functions (i.e., the place where you want your pixels
to be stored) is *contiguous*, that is:

* each pixel in memory consists of a number of data values equal to the
  number of channels in the file;
* successive column pixels within a row directly follow each other in
  memory, with the first channel of pixel ``x`` immediately following last
  channel of pixel ``x-1`` of the same row;
* for whole images or tiles, the data for each row immediately follows the
  previous one in memory (the first pixel of row ``y`` immediately follows
  the last column of row ``y-1``);
* for 3D volumetric images, the first pixel of slice ``z`` immediately
  follows the last pixel of of slice ``z-1``.

Please note that this implies that ``read_tile()`` will write pixel data into
your buffer so that it is contiguous in the shape of a single tile, not
just an offset into a whole image worth of pixels.

The ``read_scanline()`` function takes an optional ``xstride`` argument, and
the ``read_image()`` and ``read_tile()`` functions take optional
``xstride``, ``ystride``, and ``zstride`` values that describe the distance,
in *bytes*, between successive pixel columns, rows, and slices,
respectively, of your pixel buffer.  For any of these values that are not
supplied, or are given as the special constant ``AutoStride``, contiguity
will be assumed.

By passing different stride values, you can achieve some surprisingly
flexible functionality.  A few representative examples follow:

* Flip an image vertically upon reading, by using *negative* ``y`` stride::

    auto pixels = std::unique_ptr<unsigned char[]>(new unsigned char[spec.width * spec.height * spec.nchannels]);
    int scanlinesize = spec.width * spec.nchannels * sizeof(pixels[0]);
    ...
    in->read_image (0, 0, 0, spec.nchannels,
                    TypeDesc::UINT8,
                    (char *)pixels+(yres-1)*scanlinesize, // offset to last
                    AutoStride,                  // default x stride
                    -scanlinesize,               // special y stride
                    AutoStride);                 // default z stride

* Read a tile into its spot in a buffer whose layout matches a whole image
  of pixel data, rather than having a one-tile-only memory layout::

    int pixelsize = spec.nchannels * sizeof(pixels[0]);
    int scanlinesize = xpec.width * pixelsize;
    ...
    in->read_tile (x, y, 0, TypeDesc::UINT8,
                   (char *)pixels + y*scanlinesize + x*pixelsize,
                   pixelsize,
                   scanlinesize);

Please consult Section :ref:`sec-imageinput-class-reference` for detailed
descriptions of the stride parameters to each ``read`` function.


Reading channels to separate buffers
------------------------------------

While specifying data strides allows writing entire pixels to buffers with
arbitrary layouts, it is not possible to separate those pixels into multiple
buffers (i.e. to write image data to a separate or planar memory layout:
RRRRGGGGBBBB instead of the interleaved RGBRGBRGBRGB).

A workaround for this is to call ``read_scanlines``, ``read_tiles`` or
``read_image`` repeatedly with arguments ``chbegin`` and ``chend`` of
``0 <= chbegin < spec.nchannels`` and ``chend == chbegin + 1``:

.. tabs::

    .. code-tab:: c++

        // one buffer for all three channels
        auto pixels = std::unique_ptr<unsigned char[]>(new unsigned char[spec.width * spec.height * spec.nchannels]);

        for (int channel = 0; channel < spec.nchannels; ++channel) {
            file->read_image(
                0, 0,
                // reading one channel at a time
                channel, channel + 1,
                TypeDesc::UINT8,
                // writing the data to offsets spaced `spec.width * spec.height`
                // apart
                &pixels[spec.width * spec.height * channel]);
        }

    .. code-tab:: py

        pixels = numpy.zeros((spec.nchannels, spec.height, spec.width), "uint8")
        for channel in range(spec.nchannels) :
            pixels[channel] = file.read_image(0, 0, channel, channel + 1, "uint8")

For many formats, this is nearly as fast as reading the image with
interleaved pixel data if the format stores the pixels in an interleaved
layout and even slightly faster if the pixels are stored in separate planes
in the file.


Reading metadata
--------------------------------

The ImageSpec that is filled in by ``ImageInput::open()`` specifies all the
common properties that describe an image: data format, dimensions, number of
channels, tiling.  However, there may be a variety of additional *metadata*
that are present in the image file and could be queried by your application.

The remainder of this section explains how to query additional metadata in
the ImageSpec.  It is up to the ImageInput to read these from the file, if
indeed the file format is able to carry additional data.  Individual
ImageInput implementations should document which metadata they read.

Channel names
^^^^^^^^^^^^^^^^^^^^^^^^

In addition to specifying the number of color channels, the ImageSpec also
stores the names of those channels in its ``channelnames`` field, which is a
``std::vector<std::string>`` in C++, or a tuple of strings in Python.  Its
length should always be equal to the number of channels (it's the
responsibility of the ImageInput to ensure this).

Only a few file formats (and thus ImageInput implementations) have a way of
specifying custom channel names, so most of the time you will see that the
channel names follow the default convention of being named ``"R"``, ``"G"``,
``"B"``, and ``"A"``, for red, green, blue, and alpha, respectively.

Here is example code that prints the names of the channels in an image:

.. tabs::

    .. code-tab:: c++

        auto inp = ImageInput::open (filename);
        const ImageSpec &spec = inp->spec();
        for (int i = 0;  i < spec.nchannels;  ++i)
            std::cout << "Channel " << i << " is "
                      << spec.channelnames[i] << "\n";

    .. code-tab:: py

        inp = ImageInput.open (filename)
        spec = inp.spec()
        for i in range(spec.nchannels) :
            print("Channel", i, "is", spec.channelnames[i])


Specially-designated channels
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ImageSpec contains two fields, ``alpha_channel`` and ``z_channel``,
which designate which channel numbers represent alpha and ``z`` depth, if
any.  If either is set to ``-1``, it indicates that it is not known which
channel is used for that data.

If you are doing something special with alpha or depth, it is probably safer
to respect the ``alpha_channel`` and ``z_channel`` designations (if not set
to ``-1``) rather than merely assuming that, for example, channel 3 is
always the alpha channel.

Arbitrary metadata
^^^^^^^^^^^^^^^^^^^^^^^^

All other metadata found in the file will be stored in the ImageSpec's
``extra_attribs`` field, which is a ParamValueList, which is itself
essentially a vector of ParamValue instances.  Each ParamValue stores one
meta-datum consisting of a name, type (specified by a `TypeDesc`), number
of values, and data pointer.

If you know the name and type of a specific piece of metadata you want to use,
you can retrieve it using the ``ImageSpec::getattribute()`` method. In C++,
this copies the value into your variable and returns ``true`` if the attribute
was found, or ``false`` if it was not.  In Python, ``getattribute()`` simply
returns the value of the attribute itself, or ``None`` if no match was found.

.. tabs::

    .. code-tab:: c++

        auto in; = ImageInput::open(filename);
        const ImageSpec &spec = inp->spec();
        ...
        int orientation = 0;
        bool ok = spec.getattribute("Orientation", TypeInt, &orientation);
        if (!ok) {
            std::cout << "No integer orientation in the file\n";
        }

    .. code-tab:: py

        inp = ImageInput.open (filename)
        spec = in.spec()

        orientation = spec.getattribute("Orientation")
        if orientation is None :
            print("No integer orientation in the file")


By convention, ImageInput plugins will save all integer metadata as 32-bit
integers (``TypeDesc::INT`` or ``TypeDesc::UINT``), even if the file format
dictates that a particular item is stored in the file as a 8- or 16-bit
integer.  This is just to keep client applications from having to deal with
all the types.  Since there is relatively little metadata compared to pixel
data, there's no real memory waste of promoting all integer types to int32
metadata.  Floating-point metadata and string metadata may also exist, of
course.

For certain common types, there is an even simpler method for retrieving
the metadata:

.. tabs::

    .. code-tab:: c++

        int i = spec.get_int_attribute ("Orientation", 0);
        float f = spec.get_float_attribute ("PixelAspectRatio", 1.0f);
        std::string s = spec.get_string_attribute ("ImageDescription", "");

    .. code-tab:: py

        i = spec.get_int_attribute ("Orientation", 0)
        f = spec.get_float_attribute ("PixelAspectRatio", 1.0)
        s = spec.get_string_attribute ("ImageDescription", "")

This method simply returns the value.  The second argument is the default
value to use if the attribute named is not found.  These versions will do
automatic type conversion as well --- for example, if you ask for a float
and the attribute is really an int, it will return the proper float for it;
or if the attribute is a UINT16 and you call ``get_int_attribute()``, it
will succeed, promoting to an int.

And finally, another convenience method lets you treat the spec itself
as an associative array or dictionary:

.. tabs::

    .. code-tab:: c++

        // spec["key"].get<TYPE> tries to retrieve that type, or a default
        // value (generally 0 or empty string) if not found.
        int i = spec["Orientation"].get<int>();
        float f = spec["PixelAspectRatio"].get<float>();
        std::string s = spec["ImageDescription"].get<std::string>();

        // An optional argument to get() lets you specify a different default
        // value to return if the attribute is not found.
        float f = spec["PixelAspectRatio"].get<float>(1.0f);

    .. code-tab:: py

        # spec["key"] returns the attribute if present, or raises KeyError
        # if not found.
        i = spec["Orientation"]
        f = spec["PixelAspectRatio"]
        s = spec["ImageDescription"]

        # spec.get("key", default=None) returns the attribute if present,
        # or the default value if not found.
        val = spec.get("Orientation", 1)

Note that when retrieving with this "dictionary" syntax, the C++ and
Python behaviors are different: C++ requires a `get<TYPE>()` call to
retrieve the full value, and a missing key will return a default value.
Python will return the value directly (no `get()` needed), and a missing
key will raise a `KeyError` exception.

It is also possible to step through all the metadata, item by item.
This can be accomplished using the technique of the following example:

.. tabs::

    .. code-tab:: c++

        for (const auto &p : spec.extra_attribs) {
            printf ("    %s: %s\n", p.name().c_str(), p.get_string().c_str());
        }

    .. code-tab:: py

        for p in spec.attribs :
            printf ("    ", p.name, ":", p.value)

Each individual ImageInput implementation should document the names,
types, and meanings of all metadata attributes that they understand.


Color space hints
^^^^^^^^^^^^^^^^^^^^^^^^

We certainly hope that you are using only modern file formats that support
high precision and extended range pixels (such as OpenEXR) and keeping all
your images in a linear color space.  But you may have to work with file
formats that dictate the use of nonlinear color values. This is prevalent in
formats that store pixels only as 8-bit values, since 256 values are not
enough to linearly represent colors without banding artifacts in the dim
values.

The ``ImageSpec::extra_attribs`` field may store metadata that reveals the
color space the image file in the ``"oiio:ColorSpace"`` attribute (see
Section :ref:`sec-metadata-color` for explanations of particular values).

The ImageInput sets the ``"oiio:ColorSpace"`` metadata in a purely advisory
capacity --- the ``read`` will not convert pixel values among color spaces.
Many image file formats only support nonlinear color spaces (for example,
JPEG/JFIF dictates use of sRGB). So your application should intelligently
deal with gamma-corrected and sRGB input, at the very least.

The color space hints only describe color channels.  You should assume that
alpha or depth (``z``) channels (designated by the ``alpha_channel`` and
``z_channel`` fields, respectively) always represent linear values and
should never be transformed by your application.



Multi-image files and MIP-maps
--------------------------------

Some image file formats support multiple discrete subimages to be stored
in one file, and/or miltiple resolutions for each image to form a
MIPmap.  When you ``open()`` an ImageInput, it will by default point
to the first (i.e., number 0) subimage in the file, and the highest
resolution (level 0) MIP-map level.  You can switch to viewing another
subimage or MIP-map level using the ``seek_subimage()`` function:

.. tabs::

    .. code-tab:: c++

        auto inp = ImageInput::open (filename);
        int subimage = 1;
        int miplevel = 0;
        if (inp->seek_subimage (subimage, miplevel)) {
            ... process the subimage/miplevel ...
        } else {
            ... no such subimage/miplevel ...
        }

    .. code-tab:: py

        inp = ImageInput.open(filename)
        subimage = 1
        miplevel = 0
        if inp.seek_subimage (subimage, miplevel) :
            # ... process the subimage/miplevel ...
        else :
            # ... no such subimage/miplevel ...

The ``seek_subimage()`` function takes three arguments: the index of the
subimage to switch to (starting with 0), the MIPmap level (starting with 0
for the highest-resolution level), and a reference to an ImageSpec, into
which will be stored the spec of the new subimage/miplevel.  The
``seek_subimage()`` function returns `true` upon success, and `false` if
no such subimage or MIP level existed.  It is legal to visit subimages and
MIP levels out of order; the ImageInput is responsible for making it work
properly.  It is also possible to find out which subimage and MIP level is
currently being viewed, using the ``current_subimage()`` and
``current_miplevel()`` functions, which return the index of the current
subimage and MIP levels, respectively.

Below is pseudocode for reading all the levels of a MIP-map (a
multi-resolution image used for texture mapping) that shows how to read
multi-image files:

.. tabs::

    .. code-tab:: c++

        auto inp = ImageInput::open (filename);
        int miplevel = 0;
        while (inp->seek_subimage (0, miplevel)) {
            const ImageSpec &spec = inp->spec();
            int npixels = spec.width * spec.height;
            int nchannels = spec.nchannels;
            auto pixels = std::unique_ptr<unsigned char[]>(new unsigned char[npixels * nchannels]);
            inp->read_image(0, miplevel, 0, nchannels, TypeDesc::UINT8, pixels);

            // ... do whatever you want with this level, in pixels ...

            ++miplevel;
        }
        // Note: we break out of the while loop when seek_subimage fails
        // to find a next MIP level.

        inp->close();

    .. code-tab:: py

        inp = ImageInput::open (filename)
        miplevel = 0
        while inp.seek_subimage(0, miplevel) :
            spec = inp.spec()
            nchannels = spec.nchannels
            pixels = inp.read_image (0, miplevel, 0, nchannels, "uint8")

            # ... do whatever you want with this level, in pixels ...

            miplevel += 1
        }
        # Note: we break out of the while loop when seek_subimage fails
        # to find a next MIP level.

        inp.close()

In this example, we have used ``read_image()``, but of course
``read_scanline()`` and ``read_tile()`` work as you would expect, on the
current subimage and MIP level.


Per-channel formats
--------------------------------

Some image formats allow separate per-channel data formats (for example,
``half`` data for colors and `float` data for depth).  If you want to read
the pixels in their true native per-channel formats, the following steps are
necessary:

1. Check the ImageSpec's ``channelformats`` vector.  If non-empty, the
   channels in the file do not all have the same format.
2. When calling ``read_scanline``, ``read_scanlines``, ``read_tile``,
   ``read_tiles``, or ``read_image``, pass a format of ``TypeUnknown`` to
   indicate that you would like the raw data in native per-channel format of
   the file written to your ``data`` buffer.

For example, the following code fragment will read a 5-channel image to an
OpenEXR file, consisting of R/G/B/A channels in ``half`` and a Z channel in
`float`::


        auto inp = ImageInput::open (filename);
        const ImageSpec &spec = inp->spec();

        // Allocate enough space
        auto pixels = std::unique_ptr<unsigned char[]>(new unsigned char[spec.image_bytes(true)]);
        int nchannels = spec.nchannels;
        inp->read_image(0, 0, 0, nchannels,
                        TypeDesc::UNKNOWN, /* use native channel formats */
                        pixels);           /* data buffer */

        if (spec.channelformats.size() > 0) {
            ... the buffer contains packed data in the native 
                per-channel formats ...
        } else {
            ... the buffer contains all data per spec.format ...
        }



.. _sec-imageinput-deepdata:

Reading "deep" data
--------------------------------

Some image file formats (OpenEXR only, at this time) support the concept of
"deep" pixels -- those containing multiple samples per pixel (and a
potentially differing number of them in each pixel). You can tell an image
is "deep" from its ImageSpec: the ``deep`` field will be `true`.

Deep files cannot be read with the usual ``read_scanline()``,
``read_scanlines()``, ``read_tile()``, ``read_tiles()``, ``read_image()``
functions, due to the nature of their variable number of samples per pixel.
Instead, ImageInput has three special member functions used only for reading
deep data:

.. tabs::

    .. code-tab:: c++

        bool read_native_deep_scanlines (int subimage, int miplevel,
                                         int ybegin, int yend, int z,
                                         int chbegin, int chend,
                                         DeepData &deepdata);
    
        bool read_native_deep_tiles (int subimage, int miplevel,
                                     int xbegin, int xend, int ybegin int yend,
                                     int zbegin, int zend,
                                     int chbegin, int chend, DeepData &deepdata);
    
        bool read_native_deep_image (int subimage, int miplevel,
                                     DeepData &deepdata);

    .. code-tab:: py

        ImageInput.read_native_deep_scanlines (subimage, miplevel,
                                         ybegin, yend, z, chbegin, chend)
    
        ImageInput.read_native_deep_tiles (subimage, miplevel, xbegin, xend,
                                     ybegin yend, zbegin, zend, chbegin, chend)
    
        ImageInput.read_native_deep_image (subimage, miplevel)


It is only possible to read "native" data types from deep files; that is,
there is no automatic translation into arbitrary data types as there is for
ordinary images.  All three of these functions store the resulting deep data
in a special DeepData structure, described in detail in
Section :ref:`sec-imageinput-deepdata`.

Here is an example of using these methods to read a deep image from a file
and print all its values:

.. tabs::

    .. code-tab:: c++

        auto inp = ImageInput::open (filename);
        if (! inp)
            return;
        const ImageSpec &spec = inp.spec();
        if (spec.deep) {
            DeepData deepdata;
            inp.read_native_deep_image (0, 0, deepdata);
            int p = 0;  // absolute pixel number
            for (int y = 0; y < spec.height;  ++y) {
                for (int x = 0;  x < spec.width;  ++x, ++p) {
                    std::cout << "Pixel " << x << "," << y << ":\n";
                    if (deepdata.samples(p) == 0)
                        std::cout << "  no samples\n";
                    else
                        for (int c = 0;  c < spec.nchannels;  ++c) {
                            TypeDesc type = deepdata.channeltype(c);
                            std::cout << "  " << spec.channelnames[c] << ": ";
                            void *ptr = deepdata.pointers[p*spec.nchannels+c]
                            for (int s = 0; s < deepdata.samples(p); ++s) {
                                if (type.basetype == TypeDesc::FLOAT ||
                                    type.basetype == TypeDesc::HALF)
                                    std::cout << deepdata.deep_value(p, c, s) << ' ';
                                else if (type.basetype == TypeDesc::UINT32)
                                    std::cout << deepdata.deep_value_uint(p, c, s) << ' ';
                            }
                            std::cout << "\n";
                        }
                }
            }
        }
        inp.close ();

    .. code-tab:: py

        inp = ImageInput::open (filename)
        if inp is None :
            return
        spec = inp.spec()
        if spec.deep :
            deepdata = inp.read_native_deep_image (0, 0)
            p = 0  # absolute pixel number
            for y in range(spec.height) :
                for x in range(spec.width) :
                    print ("Pixel", x, ",", y, ":")
                    if deepdata.samples(p) == 0 :
                        print("  no samples)
                    else :
                        for c in range(spec.nchannels) :
                            type = deepdata.channeltype(c)
                            print ("  ", spec.channelnames[c], ":")
                            for s in range(deepdata.samples(p) :
                                print (deepdata.deep_value(p, c, s), end="")
                            print("")
        inp.close()


.. _sec-input-with-config:

Opening for input with configuration settings/hints
---------------------------------------------------

Sometimes you will want to give the image file reader hints or requests for
how to open the file or present its data, hints which must be made in time for
the initial opening of the file. For example, in specific circumstances, you
might want to request that an image with unassociated alpha *not* be
automatically converted to associated alpha by multiplying the color channel
values by the alpha (as would be customary by OIIO convention).

This is accomplished by using the ``ImageInput::open()`` or
``ImageInput::create()`` method varieties that take an additional ``config``
parameter. This is an ``ImageSpec`` object whose metadata contains the
configuration hints.

Configuration hints are optional and advisory only -- meaning that not all
image file readers will respect them (and indeed, many of them are only
sensible for certain file formats).

Some common input configuration hints that tend to be respected across many
readers (but not all, check Chapter :ref:`chap-bundledplugins` to see what
hints are supported by each reader) are:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Input Configuration Attribute
     - Type
     - Meaning
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by reading from memory rather than the file system.
   * - ``oiio:RawColor``
     - int
     - If nonzero, reading images with non-RGB color models (such as YCbCr)
       will return unaltered pixel values (versus the default OIIO behavior
       of automatically converting to RGB).
   * - ``oiio:UnassociatedAlpha``
     - int
     - If nonzero, and the file contains unassociated alpha, this will
       cause the reader to leave alpha unassociated (versus the default of
       premultiplying color channels by alpha if the alpha channel is
       unassociated).
   * - ``oiio:reorient``
     - int
     - If zero, disables any automatic reorientation that the reader may
       ordinarily do to present te pixels in the preferred display orientation.

Examples:

    Below is an example where we wish to read in an RGBA image in a format
    that tends to store it as unassociated alpha, but we DON'T want it to
    automatically be converted to associated alpha.

    .. tabs::
    
       .. code-tab:: c++
    
          // Set up an ImageSpec that holds the configuration hints.
          ImageSpec config;
          config["oiio:UnassociatedAlpha"] = 1;
    
          // Open the file, passing in the config.
          auto inp = ImageInput::open (filename, config);
          const ImageSpec &spec = inp->spec();
          auto pixels = std::unique_ptr<unsigned char[]>(new unsigned char[spec.image_pixels() * spec.nchannels]);
          inp->read_image (0, 0, 0, spec.nchannels, TypeDesc::UINT8, pixels.data());
          if (spec.get_int_attribute("oiio:UnassociatedAlpha"))
              printf("pixels holds unassociated alpha\n");
          else
              printf("pixels holds associated alpha\n");

       .. code-tab:: py
    
          # Set up an ImageSpec that holds the configuration hints.
          config = ImageSpec()
          config["oiio:UnassociatedAlpha"] = 1
    
          # Open the file, passing in the config.
          inp = ImageInput.open (filename, config)
          spec = inp.spec()
          pixels = inp.read_image (0, 0, 0, spec.nchannels, "uint8")
          if (spec["oiio:UnassociatedAlpha"] == 1)
              print("pixels holds unassociated alpha")
          else
              print("pixels holds associated alpha")

.. _sec-imageinput-ioproxy:

Custom I/O proxies (and reading the file from a memory buffer)
--------------------------------------------------------------

Some file format readers allow you to supply a custom I/O proxy object that
can allow bypassing the usual file I/O with custom behavior, including the
ability to read the file form an in-memory buffer rather than reading from
disk.

Only some input format readers support this feature. To find out if a
particular file format supports this feature, you can create an ImageInput
of the right type, and check if it supports the feature name ``"ioproxy"``::

    auto in = ImageInput::create(filename);
    if (! in  ||  ! in->supports ("ioproxy")) {
        return;
    }


ImageInput readers that support ``"ioproxy"`` will respond to a special
attribute, ``"oiio:ioproxy"``, which passes a pointer to a
``Filesystem::IOProxy*`` (see OpenImageIO's :file:`filesystem.h` for this
type and its subclasses). IOProxy is an abstract type, and concrete
subclasses include ``IOFile`` (which wraps I/O to an open ``FILE*``) and
``IOMemReader`` (which reads input from a block of memory).

Here is an example of using a proxy that reads the "file" from a memory
buffer::

    const void *buf = ...;   // pointer to memory block
    size_t size = ...;       // length of memory block
    Filesystem::IOMemReader memreader (buf, size);  // I/O proxy object

    auto in = ImageInput::open ("in.exr", nullptr, &memreader);
    in->read_image (...);
    in->close();

    // That will have read the "file" from the memory buffer



Custom search paths for plugins
--------------------------------

Please see Section :ref:`sec-globalattribs` for discussion about setting the
plugin search path via the ``attribute()`` function. For example:

.. tabs::
    
   .. code-tab:: c++
    
        std::string mysearch = "/usr/myapp/lib:/home/jane/plugins";
        OIIO::attribute ("plugin_searchpath", mysearch);
        auto inp = ImageInput::open (filename);
        // ...

   .. code-tab:: py
    
        mysearch = "/usr/myapp/lib:/home/jane/plugins"
        OpenImageIO.attribute ("plugin_searchpath", mysearch)
        inp = ImageInput.open(filename)
        # ...


Error checking
--------------------------------

Nearly every ImageInput API function returns a ``bool`` indicating whether
the operation succeeded (`true`) or failed (`false`). In the case of a
failure, the ImageInput will have saved an error message describing in more
detail what went wrong, and the latest error message is accessible using the
ImageInput method ``geterror()``, which returns the message as a
``std::string``.

The exceptions to this rule are static methods such as the static
``ImageInput::open()`` and ``ImageInput::create()``, which return an empty
pointer if it could not create an appropriate ImageInput (and open it, in
the case of ``open()``.  In such a case, since no ImageInput is returned for
which you can call its ``geterror()`` function, there exists a global
``geterror()`` function (in the ``OpenImageIO`` namespace) that retrieves
the latest error message resulting from a call to static ``open()`` or
``create()``.

Here is another version of the simple image reading code from
Section :ref:`sec-imageinput-made-simple`, but this time it is fully
elaborated with error checking and reporting:

.. tabs::
    
   .. code-tab:: c++
    
        #include <OpenImageIO/imageio.h>
        using namespace OIIO;
        ...

        const char *filename = "foo.jpg";
        auto inp = ImageInput::open (filename);
        if (! inp) {
            std::cerr << "Could not open " << filename
                      << ", error = " << OIIO::geterror() << "\n";
            return;
        }
        const ImageSpec &spec = inp->spec();
        int xres = spec.width;
        int yres = spec.height;
        int nchannels = spec.nchannels;
        auto pixels = std::unique_ptr<unsigned char[]>(new unsigned char[xres * yres * nchannels]);

        if (! inp->read_image(0, 0, 0, nchannels, TypeDesc::UINT8, &pixels[0])) {
            std::cerr << "Could not read pixels from " << filename
                      << ", error = " << inp->geterror() << "\n";
            return;
        }

        if (! inp->close ()) {
            std::cerr << "Error closing " << filename
                      << ", error = " << inp->geterror() << "\n";
            return;
        }

   .. code-tab:: py
    
        import OpenImageIO as oiio
        import numpy as np

        filename = "foo.jpg"
        inp = ImageInput::open(filename)
        if inp is None :
            print("Could not open", filename, ", error =", oiio.geterror())
            return
        spec = inp.spec()
        xres = spec.width
        yres = spec.height
        nchannels = spec.nchannels

        pixels = inp.read_image(0, 0, 0, nchannels, "uint8")
        if pixels is None :
            print("Could not read pixels from", filename, ", error =", inp.geterror())
            return

        if not inp.close() :
            print("Error closing", filename, ", error =", inp.geterror())
            return


.. _sec-imageinput-class-reference:

ImageInput Class Reference
===========================

.. doxygenclass:: OIIO::ImageInput
    :members:


