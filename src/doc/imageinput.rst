.. _chap-imageinput:

ImageInput: Reading Images
##############################


Image Input Made Simple
===========================

Here is the simplest sequence required to open an image file, find out its
resolution, and read the pixels (converting them into 8-bit values in
memory, even if that's not the way they're stored in the file)::

        #include <OpenImageIO/imageio.h>
        using namespace OIIO;
        ...

        auto in = ImageInput::open (filename);
        if (! in)
            return;
        const ImageSpec &spec = in->spec();
        int xres = spec.width;
        int yres = spec.height;
        int channels = spec.nchannels;
        std::vector<unsigned char> pixels (xres*yres*channels);
        in->read_image (TypeDesc::UINT8, &pixels[0]);
        in->close ();

Here is a breakdown of what work this code is doing:

* Search for an ImageIO plugin that is capable of reading the file
  (:file:`foo.jpg`), first by trying to deduce the correct plugin from the
  file extension, but if that fails, by opening every ImageIO plugin it can
  find until one will open the file without error.  When it finds the right
  plugin, it creates a subclass instance of ImageInput that reads the right
  kind of file format, and tries to fully open the file. The ``open()``
  method returns a ``std::unique_ptr<ImageInput>`` that will be
  automatically freed when it exits scope.

  .. code-block::

        auto in = ImageInput::open (filename);

* The specification, accessible as ``in->spec()``, contains vital
  information such as the dimensions of the image, number of color channels,
  and data type of the pixel values.  This is enough to allow us to allocate
  enough space for the image.

  .. code-block::

        const ImageSpec &spec = in->spec();
        int xres = spec.width;
        int yres = spec.height;
        int channels = spec.nchannels;
        std::vector<unsigned char> pixels (xres*yres*channels);

  Note that in this example, we don't care what data format is used for the
  pixel data in the file --- we allocate enough space for unsigned 8-bit
  integer pixel values, and will rely on OpenImageIO's ability to convert to
  our requested format from the native data format of the file.

* Read the entire image, hiding all details of the encoding of image data in
  the file, whether the file is scanline- or tile-based, or what is the
  native format of the data in the file (in this case, we request that it be
  automatically converted to unsigned 8-bit integers).

  .. code-block::

        in->read_image (TypeDesc::UINT8, &pixels[0]);

* Close the file.

  .. code-block::

        in->close ();

* When ``in`` exits its scope, the ImageInput will automatically be destroyed
  and any resources used by the plugin will be released.



Advanced Image Input
===========================

Let's walk through some of the most common things you might want to do,
but that are more complex than the simple example above.


Reading individual scanlines and tiles
--------------------------------------

The simple example of Section :ref:`Image Input Made Simple` read an entire
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

Individual scanlines may be read using the ``read_scanline()`` API call::

        ...
        auto in = ImageInput::open (filename);
        const ImageSpec &spec = in->spec();
        if (spec.tile_width == 0) {
            std::vector<unsigned char> scanline (spec.width*spec.channels);
            for (int y = 0;  y < yres;  ++y) {
                in->read_scanline (y, 0, TypeDesc::UINT8, &scanline[0]);
                ... process data in scanline[0..width*channels-1] ...
            }
        } else {
            ... handle tiles, or reject the file ...
        }
        in->close ();
        ...

The first two arguments to ``read_scanline()`` specify which scanline
is being read by its vertical (``y``) scanline number (beginning with 0)
and, for volume images, its slice (``z``) number (the slice number should
be 0 for 2D non-volume images).  This is followed by a `TypeDesc`
describing the data type of the pixel buffer you are supplying, and a
pointer to the pixel buffer itself.  Additional optional arguments
describe the data stride, which can be ignored for contiguous data (use
of strides is explained in Section :ref:`Data Strides`).

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
functions may be found in Section :ref:`ImageInput Class Reference`.

Reading tiles
^^^^^^^^^^^^^^^^^^^^^^^^

Once you ``open()`` an image file, you can find out if it is a tiled image
(and the tile size) by examining the ImageSpec's ``tile_width``,
``tile_height``, and ``tile_depth`` fields. If they are zero, it's a
scanline image and you should read pixels using ``read_scanline()``, not
``read_tile()``.

.. code-block::

        ...
        auto in = ImageInput::open (filename);
        const ImageSpec &spec = in->spec();
        if (spec.tile_width == 0) {
            ... read by scanline ...
        } else {
            // Tiles
            int tilesize = spec.tile_width * spec.tile_height;
            std::vector<unsigned char> tile (tilesize * spec.channels);
            for (int y = 0;  y < yres;  y += spec.tile_height) {
                for (int x = 0;  x < xres;  x += spec.tile_width) {
                    in->read_tile (x, y, 0, TypeDesc::UINT8, &tile[0]);
                    ... process the pixels in tile[] ..
                }
            }
        }
        in->close ();
        ...


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
Section :ref:`Data Strides`).

All ImageInput implementations are required to support reading tiles in
arbitrary order (i.e., not in strict order of increasing ``y`` rows, and
within each row, increasing ``x`` column, without missing any tiles).

The full description of the ``read_tile()`` function may be found
in Section :ref:`ImageInput Class Reference`.


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

.. code-block::

        std::unique_ptr<ImageInput> in = ImageInput::open ("myfile.tif");
        const ImageSpec &spec = in->spec();
        ...
        int numpixels = spec.width * spec.height;
        float pixels = new float [numpixels * channels];
        ...
        in->read_image (TypeDesc::FLOAT, pixels);


Note that ``read_scanline()`` and ``read_tile()`` have a parameter that
works in a corresponding manner.

You can, of course, find out the native type of the file simply by examining
``spec.format``.  If you wish, you may then allocate a buffer big enough for
an image of that type and request the native type when reading, therefore
eliminating any translation among types and seeing the actual numerical
values in the file.



Data Strides
--------------------------------

In the preceeding examples, we have assumed that the buffer passed to
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

    unsigned char pixels[spec.width * spec.height * spec.nchannels];
    int scanlinesize = spec.width * spec.nchannels * sizeof(pixels[0]);
    ...
    in->read_image (TypeDesc::UINT8,
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

Please consult Section :ref:`ImageInput Class Reference` for detailed
descriptions of the stride parameters to each ``read`` function.


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
``std::vector<std::string>``.  Its length should always be equal to the
number of channels (it's the responsibility of the ImageInput to ensure
this).

Only a few file formats (and thus ImageInput implementations) have a way of
specifying custom channel names, so most of the time you will see that the
channel names follow the default convention of being named ``"R"``, ``"G"``,
``"B"``, and ``"A"``, for red, green, blue, and alpha, respectively.

Here is example code that prints the names of the channels in an image::

        auto in = ImageInput::open (filename);
        const ImageSpec &spec = in->spec();
        for (int i = 0;  i < spec.nchannels;  ++i)
            std::cout << "Channel " << i << " is "
                      << spec.channelnames[i] << "\n";


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

If you know the name of a specific piece of metadata you want to use, you
can find it using the ``ImageSpec::find_attribute()`` method, which
returns a pointer to the matching ParamValue, or ``nullptr`` if no match was
found.  An optional `TypeDesc` argument can narrow the search to only
parameters that match the specified type as well as the name.  Below is an
example that looks for orientation information, expecting it to consist of a
single integer::

        auto in = ImageInput::open (filename);
        const ImageSpec &spec = in->spec();
        ...
        ParamValue *p = spec.find_attribute ("Orientation", TypeInt);
        if (p) {
            int orientation = * (int *) p->data();
        } else {
            std::cout << "No integer orientation in the file\n";
        }


By convention, ImageInput plugins will save all integer metadata as 32-bit
integers (``TypeDesc::INT`` or ``TypeDesc::UINT``), even if the file format
dictates that a particular item is stored in the file as a 8- or 16-bit
integer.  This is just to keep client applications from having to deal with
all the types.  Since there is relatively little metadata compared to pixel
data, there's no real memory waste of promoting all integer types to int32
metadata.  Floating-point metadata and string metadata may also exist, of
course.

For certain common types, there is an even simpler method for retrieving
the metadata::


    int i = spec.get_int_attribute ("Orientation", 0);
    float f = spec.get_float_attribute ("PixelAspectRatio", 1.0f);
    std::string s = spec.get_string_attribute ("ImageDescription", "");

This method simply returns the value.  The second argument is the default
value to use if the attribute named is not found.  These versions will do
automatic type conversion as well --- for example, if you ask for a float
and the attribute is really an int, it will return the proper float for it;
or if the attribute is a UINT16 and you call ``get_int_attribute()``, it
will succeed, promoting to an int.

It is also possible to step through all the metadata, item by item.
This can be accomplished using the technique of the following example::

        for (size_t i = 0;  i < spec.extra_attribs.size();  ++i) {
            const ParamValue &p (spec.extra_attribs[i]);
            printf ("    %s: ", p.name.c_str());
            if (p.type() == TypeString)
                printf ("\"%s\"", *(const char **)p.data());
            else if (p.type() == TypeFloat)
                printf ("%g", *(const float *)p.data());
            else if (p.type() == TypeInt)
                printf ("%d", *(const int *)p.data());
            else if (p.type() == TypeDesc::UINT)
                printf ("%u", *(const unsigned int *)p.data());
            else if (p.type() == TypeMatrix) {
                const float *f = (const float *)p.data();
                printf ("%f %f %f %f %f %f %f %f "
                        "%f %f %f %f %f %f %f %f",
                        f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7],
                        f[8], f[9], f[10], f[11], f[12], f[13], f[14], f[15]);
            }
            else
                printf (" <unknown data type> ");
            printf ("\n");
        }

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
Section :ref:`Color information metadata` for explanations of particular values).

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
subimage or MIP-map level using the ``seek_subimage()`` function::


        auto in = ImageInput::open (filename);
        ...
        int subimage = 1;
        int miplevel = 0;
        if (in->seek_subimage (subimage, miplevel)) {
            ...
        } else {
            ... no such subimage/miplevel ...
        }


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
multi-image files::


        auto in = ImageInput::open (filename);
        const ImageSpec &spec = in->spec();

        int num_miplevels = 0;
        while (in->seek_subimage (0, num_miplevels, spec)) {
            // Note: spec has the format of the current subimage/miplevel
            int npixels = spec.width * spec.height;
            int nchannels = spec.nchannels;
            unsigned char *pixels = new unsigned char [npixels * nchannels];
            in->read_image (TypeDesc::UINT8, pixels);

            ... do whatever you want with this level, in pixels ...

            delete [] pixels;
            ++num_miplevels;
        }
        // Note: we break out of the while loop when seek_subimage fails
        // to find a next MIP level.

        in->close ();


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


        auto in = ImageInput::open (filename);
        const ImageSpec &spec = in->spec();

        // Allocate enough space
        unsigned char *pixels = new unsigned char [spec.image_bytes(true)];

        in->read_image (TypeDesc::UNKNOWN, /* use native channel formats */
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
deep data::

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


It is only possible to read "native" data types from deep files; that is,
there is no automatic translation into arbitrary data types as there is for
ordinary images.  All three of these functions store the resulting deep data
in a special DeepData structure, described in detail in
Section :ref:`Reading "deep" data`.

Here is an example of using these methods to read a deep image from a file
and print all its values::

    auto in = ImageInput::open (filename);
    if (! in)
        return;
    const ImageSpec &spec = in->spec();
    if (spec.deep) {
        DeepData deepdata;
        in->read_native_deep_image (0, 0, deepdata);
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
    in->close ();



.. _sec-imageinput-readfilefrommemory:
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

    ImageSpec config; // ImageSpec describing input configuration options
    Filesystem::IOMemReader memreader (buf, size);  // I/O proxy object
    void *ptr = &memreader;
    config.attribute ("oiio:ioproxy", TypeDesc::PTR, &ptr);

    auto in = ImageInput::open ("in.exr", &config);
    in->read_image (...);
    in->close();

    // That will have read the "file" from the memory buffer



Custom search paths for plugins
--------------------------------

Please see Section :ref:`Global Attributes` for discussion about setting the plugin
search path via the ``attribute()`` function. For example::

        std::string mysearch = "/usr/myapp/lib:${HOME}/plugins";
        OIIO::attribute ("plugin_searchpath", mysearch);
        auto in = ImageInput::open (filename);
        ...


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
Section :ref:`Image Input Made Simple`, but this time it is fully
elaborated with error checking and reporting::

        #include <OpenImageIO/imageio.h>
        using namespace OIIO;
        ...

        const char *filename = "foo.jpg";
        int xres, yres, channels;
        std::vector<unsigned char> pixels;

        auto in = ImageInput::open (filename);
        if (! in) {
            std::cerr << "Could not open " << filename
                      << ", error = " << OIIO::geterror() << "\n";
            return;
        }
        const ImageSpec &spec = in->spec();
        xres = spec.width;
        yres = spec.height;
        channels = spec.nchannels;
        pixels.resize (xres*yres*channels);

        if (! in->read_image (TypeDesc::UINT8, &pixels[0])) {
            std::cerr << "Could not read pixels from " << filename
                      << ", error = " << in->geterror() << "\n";
            return;
        }

        if (! in->close ()) {
            std::cerr << "Error closing " << filename
                      << ", error = " << in->geterror() << "\n";
            return;
        }



ImageInput Class Reference
===========================

.. doxygenclass:: OIIO::ImageInput
    :members:


