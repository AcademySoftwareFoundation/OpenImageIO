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

Below are some simple code fragments that shows ImageCache in action:

.. tabs::

   .. tab:: C++
      .. literalinclude:: ../../testsuite/docs-examples-cpp/src/docs-examples-imagecache.cpp
          :language: c++
          :start-after: BEGIN-imagecache-example1
          :end-before: END-imagecache-example1

   .. tab:: Python

      .. literalinclude:: ../../testsuite/docs-examples-python/src/docs-examples-imagecache.py
          :language: py
          :start-after: BEGIN-imagecache-example1
          :end-before: END-imagecache-example1

Note that all files were referenced by name, we never had to open
or close any files, and all the resource and memory management
was automatic.


.. _sec-imagecache-api:

ImageCache API
=========================================================

.. doxygenclass:: OIIO::ImageCache
    :members:

