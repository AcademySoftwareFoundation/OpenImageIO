..
  Copyright Contributors to the OpenImageIO project.
  SPDX-License-Identifier: CC-BY-4.0


.. _chap-imagecache:

Cached Images
#############

.. _sec-imagecache-intro:

Image Cache Introduction and Theory of Operation
=========================================================

ImageCache is a utility class that allows an application to read pixels from
a large number of image files while using a remarkably small amount of
memory and other resources.  Of course it is possible for an application to
do this directly using ImageInput objects.  But ImageCache offers the
following advantages:

* ImageCache presents an even simpler user interface than ImageInput --- the
  only supported operations are asking for an ImageSpec describing a
  subimage in the file, retrieving for a block of pixels, and
  locking/reading/releasing individual tiles.  You refer to images by
  filename only; you don't need to keep track of individual file handles or
  ImageInput objects.  You don't need to explicitly open or close files.

* The ImageCache is completely thread-safe; if multiple threads are
  accessing the same file, the ImageCache internals will handle all the
  locking and resource sharing.

* No matter how many image files you are accessing, the ImageCache will
  maintain a reasonable number of simultaneously-open files, automatically
  closing files that have not been needed recently.

* No matter how large the total pixels in all the image files you are
  dealing with are, the ImageCache will use only a small amount of memory.
  It does this by loading only the individual tiles requested, and as memory
  allotments are approached, automatically releasing the memory from tiles
  that have not been used recently.

In short, if you have an application that will need to read pixels from many
large image files, you can rely on ImageCache to manage all the resources
for you.  It is reasonable to access thousands of image files totalling
hundreds of GB of pixels, efficiently and using a memory footprint on the
order of 50 MB.

Below are some simple code fragments that shows ImageCache in action::

    #include <OpenImageIO/imagecache.h>
    using namespace OIIO;

    // Create an image cache and set some options
    ImageCache *cache = ImageCache::create ();
    cache->attribute ("max_memory_MB", 500.0f);
    cache->attribute ("autotile", 64);

    // Get a block of pixels from a file.
    // (for brevity of this example, let's assume that 'size' is the
    // number of channels times the number of pixels in the requested region)
    float pixels[size];
    cache->get_pixels ("file1.jpg", 0, 0, xbegin, xend, ybegin, yend,
                       zbegin, zend, TypeDesc::FLOAT, pixels);

    // Get information about a file
    ImageSpec spec;
    bool ok = cache->get_imagespec ("file2.exr", spec);
    if (ok)
        std::cout << "resolution is " << spec.width << "x"
                  << spec.height << "\n";

    // Request and hold a tile, do some work with its pixels, then release
    ImageCache::Tile *tile;
    tile = cache->get_tile ("file2.exr", 0, 0, x, y, z);
    // The tile won't be freed until we release it, so this is safe:
    TypeDesc format;
    void *p = cache->tile_pixels (tile, format);
    // Now p points to the raw pixels of the tile, whose data format
    // is given by 'format'.
    cache->release_tile (tile);
    // Now cache is permitted to free the tile when needed

    // Note that all files were referenced by name, we never had to open
    // or close any files, and all the resource and memory management
    // was automatic.

    ImageCache::destroy (cache);


.. _sec-imagecache-api:

ImageCache API
=========================================================

.. doxygenclass:: OIIO::ImageCache
    :members:

