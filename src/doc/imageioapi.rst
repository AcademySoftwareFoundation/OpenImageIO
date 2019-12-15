Image I/O API Helper Classes
############################



.. _sec-typedesc:

Data Type Descriptions: `TypeDesc`
====================================

There are two kinds of data that are important to OpenImageIO:

* *Internal data* is in the memory of the computer, used by an
  application program.
* *Native file data* is what is stored in an image file itself
  (i.e., on the "other side" of the abstraction layer that OpenImageIO
  provides).

Both internal and file data is stored in a particular *data format*
that describes the numerical encoding of the values.  OpenImageIO
understands several types of data encodings, and there is
a special class, `TypeDesc`, that allows their enumeration and
is described in the header file ``OpenImageIO/typedesc.h``.
A `TypeDesc` describes a base data format type, aggregation into simple
vector and matrix types, and an array length (if
it's an array).

The remainder of this section describes the C++ API for `TypeDesc`.
See Section~\ref{sec:pythontypedesc} for the corresponding Python
bindings.

.. doxygenstruct:: OIIO::TypeDesc
    :members:



A number of ``static constexpr TypeDesc`` aliases for common types exist
in the outer OpenImageIO scope:

::

    TypeUnknown TypeFloat TypeColor TypePoint TypeVector TypeNormal
    TypeMatrix33 TypeMatrix44 TypeMatrix TypeHalf
    TypeInt TypeUInt TypeInt16 TypeUInt16 TypeInt8 TypeUInt8
    TypeFloat2 TypeVector2 TypeFloat4 TypeVector2i
    TypeString TypeTimeCode TypeKeyCode
    TypeRational TypePointer

The only types commonly used to store *pixel values* in image files
are scalars of ``UINT8``, ``UINT16``, `float`, and ``half``
(the last only used by OpenEXR, to the best of our knowledge).

Note that the `TypeDesc` (which is also used for applications other
than images) can describe many types not used by
OpenImageIO.  Please ignore this extra complexity; only the above simple types are understood by
OpenImageIO as pixel storage data types, though a few others, including
`string` and ``MATRIX44`` aggregates, are occasionally used for
*metadata* for certain image file formats (see `sec-imageoutput-metadata`
Sections `sec-imageoutput-metadata`, `sec-imageinput-metadata`,
and the documentation of individual ImageIO plugins for details).




.. _sec-stringview:

Non-owning string views: ``string_view``
==========================================

.. doxygenclass:: OIIO::string_view
    :members:

|


 .. _sec-ustring:

Efficient unique strings: ``ustring``
==========================================

.. doxygenclass:: OIIO::ustring
    :members:

|

.. _sec-span:

Non-owning array views: ``span`` / ``cspan``
============================================

.. doxygenclass:: OIIO::span
    :members:


Additionally, there is a convenience template:

.. cpp:type:: template<typename T> cspan = span<const T>

    `cspan<T>` is a synonym for a non-mutable `span<const T>`.

|



 .. _sec-ROI:

Rectangular region of interest: ``ROI``
==========================================

.. doxygenstruct:: OIIO::ROI
    :members:


In addition, there are several related helper functions that involve ROI:

.. doxygenfunction:: roi_union

.. doxygenfunction:: roi_intersection

.. comment .. doxygenfunction:: get_roi

.. cpp:function:: ROI get_roi (const ImageSpec& spec)
                  ROI get_roi_full (const ImageSpec& spec)

    Return the ROI describing spec's pixel data window (the x, y, z, width,
    height, depth fields) or the full (display) window (the full_x, full_y,
    full_z, full_width, full_height, full_depth fields), respectively.

.. cpp:function:: void set_roi (const ImageSpec& spec, const ROI &newroi)
    void set_roi_full (const ImageSpec& spec, const ROI &newroi)

    Alters the `spec` so to make its pixel data window or the full (display)
    window match `newroi`.



 .. _sec-ImageSpec:

Image Specification: ``ImageSpec``
==========================================

An ``ImageSpec`` is a structure that describes the complete
format specification of a single image.  It contains:

* The image resolution (number of pixels) and origin. This specifies
  what is often called the "pixel data window."
* The full size and offset of an abstract "full" or "display" window.
  Differing full and data windows can indicate that the pixels are a crop
  region or a larger image, or contain overscan pixels.
* Whether the image is organized into *tiles*, and if so, the tile size.
* The *native data format* of the pixel values (e.g., float, 8-bit
  integer, etc.).
* The number of color channels in the image (e.g., 3 for RGB images), names
  of the channels, and whether any particular channels represent *alpha*
  and *depth*.
* A user-extensible (and format-extensible) list of any other
  arbitrarily-named and -typed data that may help describe the image or
  its disk representation.


The remainder of this section describes the C++ API for ``ImageSpec``.
See Section :ref:`sec-pythonimagespec` for the corresponding Python
bindings.



.. doxygenclass:: OIIO::ImageSpec
    :members:

|



 .. _sec-DeepData:

"Deep" pixel data: `DeepData`
==========================================

.. doxygenclass:: OIIO::DeepData
    :members:

|




 .. _sec-globalattribs:

Global Attributes
==========================================

These helper functions are not part of any other OpenImageIO class, they
just exist in the OpenImageIO namespace as general utilities. (See
:ref:`sec-pythonmiscapi` for the corresponding Python bindings.)

.. doxygenfunction:: OIIO::attribute(string_view, TypeDesc, const void*)

.. cpp:function:: bool OIIO::attribute(string_view name, int val)
                  bool OIIO::attribute(string_view name, float val)
                  bool OIIO::attribute(string_view name, string_view val)

    Shortcuts for setting an attribute to a single int, float, or string.


.. doxygenfunction:: OIIO::getattribute(string_view, TypeDesc, void*)


.. cpp:function:: bool getattribute (string_view name, int &val)
                  bool getattribute (string_view name, float &val)
                  bool getattribute (string_view name, char **val)
                  bool getattribute (string_view name, std::string& val)

    Specialized versions of `getattribute()` in which the data type is
    implied by the type of the argument (for single int, float, or string).
    Two string versions exist: one that retrieves it as a `std::string` and
    another that retrieves it as a `char *`. In all cases, the return value
    is `true` if the attribute is found and the requested data type
    conversion was legal.

    EXAMPLES::

        int threads;
        OIIO::getattribute ("threads", &threads);
        std::string path;
        OIIO::getattribute ("plugin_searchpath", &path);

.. cpp:function:: int get_int_attribute (string_view name, int defaultvalue=0)
                  float get_float_attribute (string_view name, float defaultvalue=0)
                  string_view get_string_attribute (string_view name, string_view defaultvalue="")

    Specialized versions of `getattribute()` for common types, in which the
    data is returned directly, and a supplied default value is returned if
    the attribute was not found.

    EXAMPLES::

        int threads = OIIO::getattribute ("threads", 0);
        string_view path = OIIO::getattribute ("plugin_searchpath");




 .. _sec-MiscUtils:

Miscellaneous Utilities
==========================================

These helper functions are not part of any other OpenImageIO class, they
just exist in the OIIO namespace as general utilities. (See
:ref:`sec-pythonmiscapi` for the corresponding Python bindings.)

.. doxygenfunction:: openimageio_version


.. cpp:function:: std::string OIIO::geterror ()

    Returns any error string describing what went wrong if
    `ImageInput::create()` or `ImageOutput::create()` failed (since in such
    cases, the `ImageInput` or `ImageOutput` itself does not exist to have
    its own `geterror()` function called). This function returns the last
    error for this particular thread; separate threads will not clobber each
    other's global error messages.



.. doxygenfunction:: declare_imageio_format

|


 .. _sec-envvars:

Environment variables
==========================================

There are a few special environment variables that can be used to control
OpenImageIO at times that it is not convenient to set options individually from
inside the source code.

``OPENIMAGEIO_OPTIONS``

    Allows you to seed the global OpenImageIO-wide options.

    The value of the environment variable should be a comma-separated list
    of *name=value* settings. If a value is a string that itself needs to
    contain commas, it may be enclosed in single or double quotes.

    Upon startup, the contents of this environment variable will be passed
    to a call to::

        OIIO::attribute ("options", value);

``OPENIMAGEIO_IMAGECACHE_OPTIONS``

    Allows you to seed the options for any ImageCache created.

    The value of the environment variable should be a comma-separated list
    of *name=value* settings. If a value is a string that itself needs to
    contain commas, it may be enclosed in single or double quotes.

    Upon creation of any ImageCache, the contents of this environment
    variable will be passed to a call to::

        imagecache->attribute ("options", value);


``OPENIMAGEIO_TEXTURESYSTEM_OPTIONS``

    Allows you to seed the options for any TextureSystem created.

    The value of the environment variable should be a comma-separated list of
    ``name=value`` settings. If a value is a string that itself needs to
    contain commas, it may be enclosed in single or double quotes.

    Upon creation of any TextureSystem, the contents of this environment variable
    will be passed to a call to::

        texturesys->attribute ("options", value);
