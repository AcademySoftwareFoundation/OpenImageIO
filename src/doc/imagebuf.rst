..
  Copyright Contributors to the OpenImageIO project.
  SPDX-License-Identifier: CC-BY-4.0


.. _chap-imagebuf:

ImageBuf: Image Buffers
#######################


ImageBuf Introduction and Theory of Operation
=============================================

ImageBuf is a utility class that stores an entire image.  It provides a
nice API for reading, writing, and manipulating images as a single unit,
without needing to worry about any of the details of storage or I/O.

All I/O involving ImageBuf (that is, calls to `read` or `write`) are
implemented underneath in terms of ImageCache, ImageInput, and ImageOutput,
and so support all of the image file formats supported by OIIO.

The ImageBuf class definition requires that you::

    #include <OpenImageIO/imagebuf.h>


.. doxygenenum:: OIIO::ImageBuf::IBStorage


Constructing, destructing, resetting an ImageBuf
================================================

There are several ways to construct an ImageBuf. Each constructor has a
corresponding `reset` method that takes the same arguments. Calling `reset`
on an existing ImageBuf is equivalent to constructing a new ImageBuf from
scratch (even if the ImageBuf, prior to reset, previously held an image).


Making an empty or uninitialized ImageBuf
-----------------------------------------

.. doxygenfunction:: OIIO::ImageBuf::ImageBuf()
.. doxygenfunction:: OIIO::ImageBuf::reset()


Constructing a readable ImageBuf
--------------------------------

.. doxygenfunction:: OIIO::ImageBuf::ImageBuf(string_view name, int subimage = 0, int miplevel = 0, std::shared_ptr<ImageCache> imagecache = {}, const ImageSpec *config = nullptr, Filesystem::IOProxy *ioproxy = nullptr)
.. doxygenfunction:: OIIO::ImageBuf::reset(string_view name, int subimage = 0, int miplevel = 0, std::shared_ptr<ImageCache> imagecache = {}, const ImageSpec *config = nullptr, Filesystem::IOProxy *ioproxy = nullptr)


Constructing a writable ImageBuf
--------------------------------------------------

.. doxygenfunction:: OIIO::ImageBuf::ImageBuf(const ImageSpec &spec, InitializePixels zero = InitializePixels::Yes)
.. doxygenfunction:: OIIO::ImageBuf::reset(const ImageSpec &spec, InitializePixels zero = InitializePixels::Yes)
.. doxygenfunction:: OIIO::ImageBuf::make_writable


Constructing an ImageBuf that "wraps" an application buffer
-------------------------------------------------------------

.. doxygenfunction:: OIIO::ImageBuf::ImageBuf(const ImageSpec &spec, const image_span<T>&)
.. doxygenfunction:: OIIO::ImageBuf::reset(const ImageSpec &spec, const image_span<T>&)

.. doxygenfunction:: OIIO::ImageBuf::ImageBuf(const ImageSpec &spec, void *buffer, stride_t xstride = AutoStride, stride_t ystride = AutoStride, stride_t zstride = AutoStride)
.. doxygenfunction:: OIIO::ImageBuf::reset(const ImageSpec &spec, void *buffer, stride_t xstride = AutoStride, stride_t ystride = AutoStride, stride_t zstride = AutoStride)



Reading and Writing disk images
-------------------------------

.. doxygenfunction:: OIIO::ImageBuf::read(int subimage = 0, int miplevel = 0, bool force = false, TypeDesc convert = TypeDesc::UNKNOWN, ProgressCallback progress_callback = nullptr, void *progress_callback_data = nullptr)
.. doxygenfunction:: OIIO::ImageBuf::read(int subimage, int miplevel, int chbegin, int chend, bool force, TypeDesc convert, ProgressCallback progress_callback = nullptr, void *progress_callback_data = nullptr)
.. doxygenfunction:: OIIO::ImageBuf::init_spec

.. doxygenfunction:: OIIO::ImageBuf::write(string_view filename, TypeDesc dtype = TypeUnknown, string_view fileformat = string_view(), ProgressCallback progress_callback = nullptr, void *progress_callback_data = nullptr) const
.. doxygenfunction:: OIIO::ImageBuf::write(ImageOutput *out, ProgressCallback progress_callback = nullptr, void *progress_callback_data = nullptr) const
.. doxygenfunction:: OIIO::ImageBuf::set_write_format(TypeDesc format)
.. doxygenfunction:: OIIO::ImageBuf::set_write_format(cspan<TypeDesc> format)
.. doxygenfunction:: OIIO::ImageBuf::set_write_tiles
.. doxygenfunction:: OIIO::ImageBuf::set_write_ioproxy



Getting and setting information about an ImageBuf
=================================================

.. doxygenfunction:: OIIO::ImageBuf::initialized
.. doxygenfunction:: OIIO::ImageBuf::storage
.. doxygenfunction:: OIIO::ImageBuf::spec
.. doxygenfunction:: OIIO::ImageBuf::nativespec
.. doxygenfunction:: OIIO::ImageBuf::specmod
.. doxygenfunction:: OIIO::ImageBuf::name
.. doxygenfunction:: OIIO::ImageBuf::file_format_name
.. doxygenfunction:: OIIO::ImageBuf::subimage
.. doxygenfunction:: OIIO::ImageBuf::nsubimages
.. doxygenfunction:: OIIO::ImageBuf::miplevel
.. doxygenfunction:: OIIO::ImageBuf::nmiplevels
.. doxygenfunction:: OIIO::ImageBuf::nchannels


.. cpp:function:: int xbegin() const
                  int xend() const
                  int ybegin() const
                  int yend() const
                  int zbegin() const
                  int zend() const

    Returns the `[begin,end)` range of the pixel data window of the buffer.
    These are equivalent to `spec().x`, `spec().x+spec().width`, `spec().y`,
    `spec().y+spec().height`, `spec().z`, and `spec().z+spec().depth`,
    respectively.

.. doxygenfunction:: OIIO::ImageBuf::orientation
.. doxygenfunction:: OIIO::ImageBuf::set_orientation

.. cpp:function:: int oriented_width() const
                  int oriented_height() const
                  int oriented_x() const
                  int oriented_y() const
                  int oriented_full_width() const
                  int oriented_full_height() const
                  int oriented_full_x() const
                  int oriented_full_y() const

    The oriented width, height, x, and y describe the pixel data window
    after taking the display orientation into consideration.  The *full*
    versions the "full" (a.k.a. display) window after taking the display
    orientation into consideration.


.. doxygenfunction:: OIIO::ImageBuf::roi
.. doxygenfunction:: OIIO::ImageBuf::roi_full
.. doxygenfunction:: OIIO::ImageBuf::set_origin
.. doxygenfunction:: OIIO::ImageBuf::set_full
.. doxygenfunction:: OIIO::ImageBuf::set_roi_full
.. doxygenfunction:: OIIO::ImageBuf::contains_roi
.. doxygenfunction:: OIIO::ImageBuf::pixeltype
.. doxygenfunction:: OIIO::ImageBuf::threads() const
.. doxygenfunction:: OIIO::ImageBuf::threads(int n) const



Copying ImageBuf's and blocks of pixels
========================================

.. doxygenfunction:: OIIO::ImageBuf::operator=(const ImageBuf &src)
.. doxygenfunction:: OIIO::ImageBuf::operator=(ImageBuf &&src)
.. doxygenfunction:: OIIO::ImageBuf::copy(const ImageBuf &src, TypeDesc format = TypeUnknown)
.. doxygenfunction:: OIIO::ImageBuf::copy(TypeDesc format) const
.. doxygenfunction:: OIIO::ImageBuf::copy_pixels
.. doxygenfunction:: OIIO::ImageBuf::copy_metadata
.. doxygenfunction:: OIIO::ImageBuf::merge_metadata
.. doxygenfunction:: OIIO::ImageBuf::swap



Getting and setting pixel values
================================

**Getting and setting individual pixels -- slow**

.. doxygenfunction:: OIIO::ImageBuf::getchannel
.. doxygenfunction:: OIIO::ImageBuf::getpixel(int x, int y, int z, float *pixel, int maxchannels = 1000, WrapMode wrap = WrapBlack) const

.. doxygenfunction:: OIIO::ImageBuf::interppixel(float, float, span<float>, WrapMode) const
.. doxygenfunction:: OIIO::ImageBuf::interppixel_bicubic(float, float, span<float>, WrapMode) const
.. doxygenfunction:: OIIO::ImageBuf::interppixel_NDC(float, float, span<float>, WrapMode) const
.. doxygenfunction:: OIIO::ImageBuf::interppixel_bicubic_NDC(float, float, span<float>, WrapMode) const

.. doxygenfunction:: OIIO::ImageBuf::setpixel(int x, int y, int z, cspan<float> pixel)
.. doxygenfunction:: OIIO::ImageBuf::setpixel(int i, cspan<float> pixel)

|

**Getting and setting regions of pixels -- fast**

.. doxygenfunction:: OIIO::ImageBuf::get_pixels(ROI, const image_span<T>&) const
.. doxygenfunction:: OIIO::ImageBuf::get_pixels(ROI, TypeDesc, const image_span<std::byte>&) const
.. doxygenfunction:: OIIO::ImageBuf::set_pixels(ROI, const image_span<T>&)
.. doxygenfunction:: OIIO::ImageBuf::set_pixels(ROI, TypeDesc, const image_span<std::byte>&)

|

**Getting and setting regions of pixels -- deprecated**

These raw pointer versions of `get_pixels()` and `set_pixels()` should be
converted to the `span` and `image_span` based versions.

.. doxygenfunction:: OIIO::ImageBuf::get_pixels(ROI, span<T>, stride_t, stride_t, stride_t) const
.. doxygenfunction:: OIIO::ImageBuf::get_pixels(ROI, span<T>, T*, stride_t, stride_t, stride_t) const
.. doxygenfunction:: OIIO::ImageBuf::set_pixels(ROI, span<T>, stride_t, stride_t, stride_t)
.. doxygenfunction:: OIIO::ImageBuf::set_pixels(ROI, span<T>, const T*, stride_t, stride_t, stride_t)



Deep data in an ImageBuf
========================

.. doxygenfunction:: OIIO::ImageBuf::deep
.. doxygenfunction:: OIIO::ImageBuf::deep_samples
.. doxygenfunction:: OIIO::ImageBuf::set_deep_samples
.. doxygenfunction:: OIIO::ImageBuf::deep_insert_samples
.. doxygenfunction:: OIIO::ImageBuf::deep_erase_samples
.. doxygenfunction:: OIIO::ImageBuf::deep_value(int x, int y, int z, int c, int s) const
.. doxygenfunction:: OIIO::ImageBuf::deep_value_uint(int x, int y, int z, int c, int s) const
.. doxygenfunction:: OIIO::ImageBuf::set_deep_value(int x, int y, int z, int c, int s, float value)
.. doxygenfunction:: OIIO::ImageBuf::set_deep_value(int x, int y, int z, int c, int s, uint32_t value)
.. doxygenfunction:: OIIO::ImageBuf::deep_pixel_ptr

.. cpp:function:: DeepData& OIIO::ImageBuf::deepdata()
                  const DeepData& OIIO::ImageBuf::deepdata() const

    Returns a reference to the underlying `DeepData` for a deep image.



Error Handling
==============

.. doxygenfunction:: OIIO::ImageBuf::errorfmt
.. doxygenfunction:: OIIO::ImageBuf::has_error
.. doxygenfunction:: OIIO::ImageBuf::geterror


Miscellaneous
=============

.. cpp:function:: void* localpixels()
                  const void* localpixels() const

    Return a raw pointer to the "local" pixel memory, if they are fully in
    RAM and not backed by an ImageCache, or `nullptr` otherwise. You can
    also test it like a `bool` to find out if pixels are local.

.. cpp:function:: void* pixeladdr(int x, int y, int z = 0, int ch = 0)
                  const void* pixeladdr(int x, int y, int z = 0, int ch = 0) const

    Return the address where pixel `(x,y,z)`, channel `ch`, is stored in the
    image buffer.  Use with extreme caution!  Will return `nullptr` if the
    pixel values aren't local in RAM.


.. doxygenfunction:: OIIO::ImageBuf::pixelindex
.. doxygenfunction:: OIIO::ImageBuf::WrapMode_from_string

.. cpp:function:: void lock() const
                  void unlock() const

    Manually lock or unlock the mutex that protects the ImageBuf from
    concurrent access by multiple threads. Use with caution -- this should
    almost never be needed in ordinary user code.



Writing your own image processing functions
===========================================

In this section, we will discuss how to write functions that operate
pixel by pixel on an ImageBuf. There are several different approaches
to this, with different trade-offs in terms of speed, flexibility, and
simplicity of implementation.

Simple pixel-by-pixel access with `ImageBufAlgo::perpixel_op()`
---------------------------------------------------------------

Pros:

* You only need to supply the inner loop body, the part that does the work
  for a single pixel.
* You can assume that all pixel data are float values.

Cons/Limitations:

* The operation must be one where each output pixel depends only on the
  corresponding pixel of the input images.
* Currently, the operation must be unary (one input image to produce one
  output image), or binary (two input images, one output image). At this time,
  there are not options to operate on a single image in-place, or to have more
  than two input images, but this may be extended in the future.
* Operating on `float`-based images is "full speed," but if the input images
  are not `float`, the automatic conversions will add some expense. In
  practice, we find working on non-float images to be about half the speed of
  float images, but this may be acceptable in exchange for the simplicity of
  this approach, especially for operations where you expect inputs to be float
  typically.

.. doxygenfunction:: perpixel_op(const ImageBuf &src, function_view<bool(span<float>, cspan<float>)> op, KWArgs options = {})

.. doxygenfunction:: perpixel_op(const ImageBuf &srcA, const ImageBuf& srcB, function_view<bool(span<float>, cspan<float>, cspan<float>)> op, KWArgs options = {})

Examples:

.. code-block:: cpp

    // Assume ImageBuf A, B are the inputs, ImageBuf R is the output

    /////////////////////////////////////////////////////////////////
    // Approach 1: using a standalone function to add two images
    bool my_add (span<float> r, cspan<float> a, cspan<float> b) {
        for (size_t c = 0, nc = size_t(r.size()); c < nc; ++c)
            r[c] = a[c] + b[c];
        return true;
    }

    R = ImageBufAlgo::perpixel_op(A, B, my_add);

    /////////////////////////////////////////////////////////////////
    // Approach 2: using a "functor" class to add two images
    struct Adder {
        bool operator() (span<float> r, cspan<float> a, cspan<float> b) {
            for (size_t c = 0, nc = size_t(r.size()); c < nc; ++c)
                r[c] = a[c] + b[c];
            return true;
        }
    };

    Adder adder;
    R = ImageBufAlgo::perpixel_op(A, B, adder);
    
    /////////////////////////////////////////////////////////////////
    // Approach 3: using a lambda to add two images
    R = ImageBufAlgo::perpixel_op(A, B,
            [](span<float> r, cspan<float> a, cspan<float> b) {
                for (size_t c = 0, nc = size_t(r.size()); c < nc; ++c)
                    r[c] = a[c] + b[c];
                return true;
            });



Iterators -- the fast way of accessing individual pixels
========================================================

Sometimes you need to visit every pixel in an ImageBuf (or at least, every
pixel in a large region).  Using the `getpixel()` and `setpixel()` for this
purpose is simple but very slow.  But ImageBuf provides templated `Iterator`
and `ConstIterator` types that are very inexpensive and hide all the details
of local versus cached storage.

    .. note:: `ImageBuf::ConstIterator` is identical to the Iterator,
        except that `ConstIterator` may be used on a `const ImageBuf` and
        may not be used to alter the contents of the ImageBuf.  For
        simplicity, the remainder of this section will only discuss the
        `Iterator`.

An Iterator is associated with a particular ImageBuf. The Iterator has a
*current pixel* coordinate that it is visiting, and an *iteration range*
that describes a rectangular region of pixels that it will visits as it
advances.  It always starts at the upper left corner of the iteration
region.  We say that the iterator is *done* after it has visited every pixel
in its iteration range.  We say that a pixel coordinate *exists* if it is
within the pixel data window of the ImageBuf.  We say that a pixel
coordinate is *valid* if it is within the iteration range of the iterator.

The `Iterator<BUFT,USERT>` is templated based on two types: `BUFT` the type
of the data stored in the ImageBuf, and `USERT` type type of the data that
you want to manipulate with your code.  `USERT` defaults to `float`, since
usually you will want to do all your pixel math with `float`.  We will
thus use `Iterator<T>` synonymously with `Iterator<T,float>`.

For the remainder of this section, we will assume that you have a
`float`-based ImageBuf, for example, if it were set up like this::

    ImageBuf buf ("myfile.exr");
    buf.read (0, 0, true, TypeDesc::FLOAT);


.. cpp:function:: Iterator<BUFT> (ImageBuf &buf, WrapMode wrap=WrapDefault)

    Initialize an iterator that will visit every pixel in the data window
    of `buf`, and start it out pointing to the upper left corner of
    the data window.  The `wrap` describes what values will be retrieved
    if the iterator is positioned outside the data window of the buffer.

.. cpp:function:: Iterator<BUFT> (ImageBuf &buf, const ROI &roi, WrapMode wrap=WrapDefault)

    Initialize an iterator that will visit every pixel of `buf` within the
    region described by `roi`, and start it out pointing to pixel
    (`roi.xbegin, roi.ybegin, roi.zbegin`). The `wrap` describes what values
    will be retrieved if the iterator is positioned outside the data window
    of the buffer.

.. cpp:function:: Iterator<BUFT> (ImageBuf &buf, int x, int y, int z, WrapMode wrap=WrapDefault)

    Initialize an iterator that will visit every pixel in the data window
    of `buf`, and start it out pointing to pixel (x, y, z).
    The `wrap` describes what values will be retrieved
    if the iterator is positioned outside the data window of the buffer.

.. cpp:function:: Iterator::operator++ ()

    The `++` operator advances the iterator to the next pixel in its
    iteration range.  (Both prefix and postfix increment operator are
    supported.)

.. cpp:function:: bool Iterator::done () const

    Returns `true` if the iterator has completed its visit of all pixels in
    its iteration range.

.. cpp:function:: ROI Iterator::range () const

    Returns the iteration range of the iterator, expressed as an ROI.

.. cpp:function:: int Iterator::x () const
                  int Iterator::y () const
                  int Iterator::z () const

    Returns the x, y, and z pixel coordinates, respectively, of the pixel
    that the iterator is currently visiting.

.. cpp:function:: bool Iterator::valid () const

    Returns `true` if the iterator's current pixel coordinates are within
    its iteration range.

.. cpp:function:: bool Iterator::valid (int x, int y, int z=0) const

    Returns `true` if pixel coordinate (x, y, z) are within the iterator's
    iteration range (regardless of where the iterator itself is currently
    pointing).

.. cpp:function:: bool Iterator::exists () const

    Returns `true` if the iterator's current pixel coordinates are within
    the data window of the ImageBuf.

.. cpp:function:: bool Iterator::exists (int x, int y, int z=0) const

    Returns `true` if pixel coordinate (x, y, z) are within the pixel data
    window of the ImageBuf (regardless of where the iterator itself is
    currently pointing).

.. cpp:function:: USERT& Iterator::operator[] (int i)

    The value of channel `i` of the current pixel.  (The wrap mode, set up
    when the iterator was constructed, determines what value is returned if
    the iterator points outside the pixel data window of its buffer.)

.. cpp:function:: int Iterator::deep_samples () const

    For deep images only, retrieves the number of deep samples for the
    current pixel.

.. cpp:function:: void Iterator::set_deep_samples ()

    For deep images only (and non-const ImageBuf), set the number of deep
    samples for the current pixel. This only is useful if the ImageBuf has
    not yet had the `deep_alloc()` method called.

.. cpp:function:: USERT Iterator::deep_value (int c, int s) const
                  uint32_t Iterator::deep_value_int (int c, int s) const

    For deep images only, returns the value of channel `c`, sample number
    `s`, at the current pixel.

.. cpp:function:: void Iterator::set_deep_value (int c, int s, float value)
                  void Iterator::set_deep_value (int c, int s, uint32_t value)

    For deep images only (and non-cconst ImageBuf, sets the value of channel
    `c`, sample number `s`, at the current pixel. This only is useful if the
    ImageBuf has already had the `deep_alloc()` method called.


Example: Visiting all pixels to compute an average color
--------------------------------------------------------

.. code-block:: cpp

    void print_channel_averages (const std::string &filename)
    {
        // Set up the ImageBuf and read the file
        ImageBuf buf (filename);
        bool ok = buf.read (0, 0, true, TypeDesc::FLOAT);  // Force a float buffer
        if (! ok)
            return;
    
        // Initialize a vector to contain the running total
        int nc = buf.nchannels();
        std::vector<float> total (n, 0.0f);
    
        // Iterate over all pixels of the image, summing channels separately
        for (ImageBuf::ConstIterator<float> it (buf);  ! it.done();  ++it)
            for (int c = 0;  c < nc;  ++c)
                total[c] += it[c];
    
        // Print the averages
        imagesize_t npixels = buf.spec().image_pixels();
        for (int c = 0;  c < nc;  ++c)
            std::cout << "Channel " << c << " avg = " (total[c] / npixels) << "\n";
    }


.. _sec-make-black:

Example: Set all pixels in a region to black
--------------------------------------------

.. code-block:: cpp

    bool make_black (ImageBuf &buf, ROI region)
    {
        if (buf.spec().format != TypeDesc::FLOAT)
            return false;    // Assume it's a float buffer
    
        // Clamp the region's channel range to the channels in the image
        roi.chend = std::min (roi.chend, buf.nchannels);
    
        // Iterate over all pixels in the region...
        for (ImageBuf::Iterator<float> it (buf, region);  ! it.done();  ++it) {
            if (! it.exists())   // Make sure the iterator is pointing
                continue;        //   to a pixel in the data window
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                it[c] = 0.0f;  // clear the value
        }
        return true;
    }


Dealing with buffer data types
==============================

The previous section on iterators presented examples and discussion based on
the assumption that the ImageBuf was guaranteed to store `float` data and
that you wanted all math to also be done as `float` computations.  Here we
will explain how to deal with buffers and files that contain different data
types.

Strategy 1: Only have `float` data in your ImageBuf
-----------------------------------------------------

When creating your own buffers, make sure they are `float`::

    ImageSpec spec (640, 480, 3, TypeDesc::FLOAT); // <-- float buffer
    ImageBuf buf ("mybuf", spec);

When using ImageCache-backed buffers, force the ImageCache
to convert everything to `float`::

    // Just do this once, to set up the cache:
    ImageCache *cache = ImageCache::create (true /* shared cache */);
    cache->attribute ("forcefloat", 1);
    ...
    ImageBuf buf ("myfile.exr");   // Backed by the shared cache

Or force the read to convert to `float` in the buffer if
it's not a native type that would automatically stored as a `float`
internally to the ImageCache:[#]_

.. [#] ImageCache only supports a limited set of types internally, currently
       float, half, uint8, uint16, and all other data types are converted to
       these automatically as they are read into the cache.

.. code-block:: cpp

    ImageBuf buf ("myfile.exr");   // Backed by the shared cache
    buf.read (0, 0, false /* don't force read to local mem */,
              TypeDesc::FLOAT /* but do force conversion to float*/);

Or force a read into local memory unconditionally (rather
than relying on the ImageCache), and convert to `float`::

    ImageBuf buf ("myfile.exr");
    buf.read (0, 0, true /*force read*/,
              TypeDesc::FLOAT /* force conversion */);

Strategy 2: Template your iterating functions based on buffer type
------------------------------------------------------------------

Consider the following alternate version of the `make_black` function
from Section `Example: Set all pixels in a region to black`_ ::

    template<typename BUFT>
    static bool make_black_impl (ImageBuf &buf, ROI region)
    {
        // Clamp the region's channel range to the channels in the image
        roi.chend = std::min (roi.chend, buf.nchannels);
    
        // Iterate over all pixels in the region...
        for (ImageBuf::Iterator<BUFT> it (buf, region);  ! it.done();  ++it) {
            if (! it.exists())   // Make sure the iterator is pointing
                continue;        //   to a pixel in the data window
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                it[c] = 0.0f;  // clear the value
        }
        return true;
    }
    
    bool make_black (ImageBuf &buf, ROI region)
    {
        if (buf.spec().format == TypeDesc::FLOAT)
            return make_black_impl<float> (buf, region);
        else if (buf.spec().format == TypeDesc::HALF)
            return make_black_impl<half> (buf, region);
        else if (buf.spec().format == TypeDesc::UINT8)
            return make_black_impl<unsigned char> (buf, region);
        else if (buf.spec().format == TypeDesc::UINT16)
            return make_black_impl<unsigned short> (buf, region);
        else {
            buf.error ("Unsupported pixel data format %s", buf.spec().format);
            return false;
        }
    }

In this example, we make an implementation that is templated on the buffer
type, and then a wrapper that calls the appropriate template specialization
for each of 4 common types (and logs an error in the buffer for any other
types it encounters).

In fact, :file:`imagebufalgo_util.h` provides a macro to do this (and
several variants, which will be discussed in more detail in the next
chapter).  You could rewrite the example even more simply::

    #include <OpenImageIO/imagebufalgo_util.h>
    
    template<typename BUFT>
    static bool make_black_impl (ImageBuf &buf, ROI region)
    {
        ... same as before ...
    }
    
    bool make_black (ImageBuf &buf, ROI region)
    {
        bool ok;
        OIIO_DISPATCH_COMMON_TYPES (ok, "make_black", make_black_impl,
                                     buf.spec().format, buf, region);
        return ok;
    }

This other type-dispatching helper macros will be discussed in more
detail in Chapter :ref:`chap-imagebufalgo`.

