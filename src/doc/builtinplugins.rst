..
  Copyright Contributors to the OpenImageIO project.
  SPDX-License-Identifier: CC-BY-4.0

.. _chap-bundledplugins:

Bundled ImageIO Plugins
#######################

This chapter lists all the image format plugins that are bundled with
OpenImageIO.  For each plugin, we delineate any limitations, custom
attributes, etc.  The plugins are listed alphabetically by format name.



|

.. _sec-bundledplugins-bmp:

BMP
===============================================

BMP is a bitmap image file format used mostly on Windows systems.
BMP files use the file extension :file:`.bmp`.

BMP is not a nice format for high-quality or high-performance images.
It only supports unsigned integer 1-, 2-, 4-, and 8- bits per channel; only
grayscale, RGB, and RGBA; does not support MIPmaps, multiimage, or
tiles.

**BMP Attributes**

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - BMP header data or explanation
   * - ``compression``
     - string
     - The compression of the BMP file (``"rle4"`` or ``"rle8"``, if
       RLE compression is used).
   * - ``XResolution``
     - float
     - hres
   * - ``YResolution``
     - float
     - vres
   * - ``ResolutionUnit``
     - string
     - always ``"m"`` (pixels per meter)
   * - ``bmp:bitsperpixel``
     - int
     - When not a whole number of bytes per channel, this describes the
       bits per pixel in the file (16 for R4G4B4, 8 for a 256-color palette
       image, 4 for a 16-color palette image, 1 for a 2-color palette image).
   * - ``bmp:version``
     - int
     - Version of the BMP file format
   * - ``oiio:ColorSpace``
     - string
     - currently, it is always ``"sRGB"`` (we presume all BMP files are sRGB)

**Configuration settings for BMP input**

When opening a BMP ImageInput with a *configuration* (see
Section :ref:`sec-input-with-config`), the following special configuration
options are supported:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Input Configuration Attribute
     - Type
     - Meaning
   * - ``bmp:monochrome_detect``
     - int
     - If nonzero, try to detect when all palette entries are gray and pretend
       that it's a 1-channel image to allow the calling app to save memory
       and time (even though the BMP format does not actually support
       grayscale images per se. It is 1 by default, but by setting the hint
       to 0, you can disable this behavior.
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by reading from memory rather than the file system.

**Configuration settings for BMP output**

When opening an BMP ImageOutput, the following special metadata tokens
control aspects of the writing itself:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Output Configuration Attribute
     - Type
     - Meaning
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by writing to a memory buffer.
   * - ``oiio:dither``
     - int
     - If nonzero and outputting UINT8 values in the file from a source of
       higher bit depth, will add a small amount of random dither to combat
       the appearance of banding.

**Custom I/O Overrides**

BMP input and output both support the "custom I/O" feature via the special
``"oiio:ioproxy"`` attributes (see Sections :ref:`sec-imageoutput-ioproxy` and
:ref:`sec-imageinput-ioproxy`) as well as the `set_ioproxy()` methods.

**BMP Limitations**

* OIIO's current implementation will only write uncompressed 8bpp (from a
  1-channel source), 24bpp (if 3 channel), or 32bpp (if 4 channel). Reads,
  however, can handle RLE compression as well as 1, 4, or 16 bpp input.
* Only 1, 3, and 4-channel images are supported with BMP due to limitations
  of the file format itself.
* BMP only supports uint8 pixel data types. Requests for other pixel data
  types will automatically be converted to uint8.
* OIIO's current implementation only supports RGB BMP files and presumes that
  the pixel data are in sRGB color space.  It does not currently support CMYK
  files or the color primary header information. (Though if this is important
  to anyone, support can be added in the future.)

|

.. _sec-bundledplugins-cineon:

Cineon
===============================================

Cineon is an image file format developed by Kodak that is commonly
used for scanned motion picture film and digital intermediates.
Cineon files use the file extension :file:`.cin`.



|

.. _sec-bundledplugins-dds:

DDS
===============================================

DDS (Direct Draw Surface) is an image file format designed by Microsoft
for use in Direct3D graphics.  DDS files use the extension :file:`.dds`.

DDS is primarily meant for images that are directly usable by the GPU.
It supports 2D, cube and volume images with or without MIPmaps; using
either uncompressed pixel formats or one of the lossy compression
schemes supported by the graphics hardware (BC1-BC7).

OpenImageIO currently only supports reading DDS files, not writing them.

DDS files containing a "normal map" (`0x80000000`) pixel format flag
will be interpreted as a tangent space normal map. When reading such files,
the resulting image will be a 3-channel image with red & green channels
coming from file data, and the blue channel computed as if it were the
Z component of a normal map. This applies to images using DXT5 compression
(normal X & Y components are assumed to be in alpha & green channels)
and images using BC5/ATI2 compression (normal X & Y components are in
red & green channels).

**Attributes**

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - DDS header data or explanation
   * - ``compression``
     - string
     - Compression type
   * - ``oiio:BitsPerSample``
     - int
     - bits per sample
   * - ``textureformat``
     - string
     - Set correctly to one of ``"Plain Texture"``, ``"Volume Texture"``, or
       ``"CubeFace Environment"``.
   * - ``dds:CubeMapSides``
     - string
     - For environment maps, which cube faces are present (e.g., ``"+x -x
       +y -y"`` if *x* & *y* faces are present, but not *z*).


**Configuration settings for DDS input**

When opening an DDS ImageInput with a *configuration* (see
Section :ref:`sec-input-with-config`), the following special configuration
attributes are supported:

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

Additionally, an integer ``dds:bc5normal`` global attribute is supported
to control behaviour of images compressed in BC5/ATI2 compression format.
When the attribute value is set to non-zero (default is zero), any input
image using BC5/ATI2 compression format is assumed to be a normal map,
even if pixel format "normal map" flag is not set.

**Custom I/O Overrides**

DDS input supports the "custom I/O" feature via the
special ``"oiio:ioproxy"`` attributes (see Sections
:ref:`sec-imageoutput-ioproxy` and :ref:`sec-imageinput-ioproxy`) as well as
the `set_ioproxy()` methods.


|

.. _sec-bundledplugins-dicom:

DICOM
===============================================

DICOM (Digital Imaging and Communications in Medicine) is the standard
format used for medical images. DICOM files usually have the extension
:file:`.dcm`.

OpenImageIO currently only supports reading DICOM files, not writing them.


.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - DDS header data or explanation
   * - ``oiio:BitsPerSample``
     - int
     - Bits per sample.
   * - ``dicom:*``
     - *any*
     - DICOM header information and metadata is currently all
       preceded by the ``dicom:`` prefix.



|

.. _sec-bundledplugins-dpx:

DPX
===============================================

DPX (Digital Picture Exchange) is an image file format used for
motion picture film scanning, output, and digital intermediates.
DPX files use the file extension :file:`.dpx`.


**Configuration settings for DPX input**

When opening a DPX ImageInput with a *configuration* (see
Section :ref:`sec-input-with-config`), the following special configuration
options are supported:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Input Configuration Attribute
     - Type
     - Meaning
   * - ``oiio:RawColor``
     - int
     - If nonzero, reading images with non-RGB color models (such as YCbCr)
       will return unaltered pixel values (versus the default OIIO behavior
       of automatically converting to RGB).
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by reading from memory rather than the file system.
   * - ``oiio:subimages``
     - int
     - The number of "image elements" (subimages) in the file.


**Configuration settings for DPX output**

When opening a DPX ImageOutput, the following special metadata tokens
control aspects of the writing itself:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Output configuration Attribute
     - Type
     - Meaning
   * - ``oiio:RawColor``
     - int
     - If nonzero, writing images with non-RGB color models (such as YCbCr)
       will keep unaltered pixel values (versus the default OIIO behavior
       of automatically converting from RGB to the designated color space
       as the pixels are written).
   * - ``oiio:dither``
     - int
     - If nonzero and outputting UINT8 values in the file from a source of
       higher bit depth, will add a small amount of random dither to combat
       the appearance of banding.
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by writing to memory rather than the file system.

**Custom I/O Overrides**

DPX input and output both support the "custom I/O" feature via the
special ``"oiio:ioproxy"`` attributes (see Sections
:ref:`sec-imageoutput-ioproxy` and :ref:`sec-imageinput-ioproxy`) as well as
the `set_ioproxy()` methods.

**DPX Attributes**

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - DPX header data or explanation
   * - ``ImageDescription``
     - string
     - Description of image element
   * - ``Copyright``
     - string
     - Copyright statement
   * - ``Software``
     - string
     - Creator
   * - ``DocumentName``
     - string
     - Project name
   * - ``DateTime``
     - string
     - Creation date/time
   * - ``Orientation``
     - int
     - the orientation of the DPX image data (see ``metadata:orientation``)
   * - ``compression``
     - string
     - The compression type
   * - ``PixelAspectRatio``
     - float
     - pixel aspect ratio
   * - ``oiio:BitsPerSample``
     - int
     - the true bits per sample of the DPX file.
   * - ``oiio:Endian``
     - string
     - When writing, force a particular endianness for the output ``"little"`` or ``"big"``)
   * - ``smpte:TimeCode``
     - int[2]
     - SMPTE time code (vecsemantics will be marked as TIMECODE)
   * - ``smpte:KeyCode``
     - int[7]
     - SMPTE key code (vecsemantics will be marked as KEYCODE)
   * - ``dpx:Transfer``
     - string
     - Transfer characteristic
   * - ``dpx:Colorimetric``
     - string
     - Colorimetric specification
   * - ``dpx:ImageDescriptor``
     - string
     - ImageDescriptor
   * - ``dpx:Packing``
     - string
     - Image packing method
   * - ``dpx:TimeCode``
     - int
     - SMPTE time code
   * - ``dpx:UserBits``
     - int
     - SMPTE user bits
   * - ``dpx:SourceDateTime``
     - string
     - source time and date
   * - ``dpx:FilmEdgeCode``
     - string
     - FilmEdgeCode
   * - ``dpx:Signal``
     - string
     - Signal (``"Undefined"``, ``"NTSC"``, ``"PAL"``, etc.)
   * - ``dpx:UserData``
     - UCHAR[*]
     - User data (stored in an array whose length is whatever it it was in the DPX file)
   * - ``dpx:EncryptKey``
     - int
     - Encryption key (-1 is not encrypted)
   * - ``dpx:DittoKey``
     - int
     - Ditto (0 = same as previous frame, 1 = new)
   * - ``dpx:LowData``
     - int
     - reference low data code value
   * - ``dpx:LowQuantity``
     - float
     - reference low quantity
   * - ``dpx:HighData``
     - int
     - reference high data code value
   * - ``dpx:HighQuantity``
     - float
     - reference high quantity
   * - ``dpx:XScannedSize``
     - float
     - X scanned size
   * - ``dpx:YScannedSize``
     - float
     - Y scanned size
   * - ``dpx:FramePosition``
     - int
     - frame position in sequence
   * - ``dpx:SequenceLength``
     - int
     - sequence length (frames)
   * - ``dpx:HeldCount``
     - int
     - held count (1 = default)
   * - ``dpx:FrameRate``
     - float
     - frame rate of original (frames/s)
   * - ``dpx:ShutterAngle``
     - float
     - shutter angle of camera (deg)
   * - ``dpx:Version``
     - string
     - version of header format
   * - ``dpx:Format``
     - string
     - format (e.g., ``"Academy"``)
   * - ``dpx:FrameId``
     - string
     - frame identification
   * - ``dpx:SlateInfo``
     - string
     - slate information
   * - ``dpx:SourceImageFileName``
     - string
     - source image filename
   * - ``dpx:InputDevice``
     - string
     - input device name
   * - ``dpx:InputDeviceSerialNumber``
     - string
     - input device serial number
   * - ``dpx:Interlace``
     - int
     - interlace (0 = noninterlace, 1 = 2:1 interlace
   * - ``dpx:FieldNumber``
     - int
     - field number
   * - ``dpx:HorizontalSampleRate``
     - float
     - horizontal sampling rate (Hz)
   * - ``dpx:VerticalSampleRate``
     - float
     - vertical sampling rate (Hz)
   * - ``dpx:TemporalFrameRate``
     - float
     - temporal sampling rate (Hz)
   * - ``dpx:TimeOffset``
     - float
     - time offset from sync to first pixel (ms)
   * - ``dpx:BlackLevel``
     - float
     - black level code value
   * - ``dpx:BlackGain``
     - float
     - black gain
   * - ``dpx:BreakPoint``
     - float
     - breakpoint
   * - ``dpx:WhiteLevel``
     - float
     - reference white level code value
   * - ``dpx:IntegrationTimes``
     - float
     - integration time (s)
   * - ``dpx:EndOfLinePadding``
     - int
     - Padded bytes at the end of each line
   * - ``dpx:EndOfImagePadding``
     - int
     - Padded bytes at the end of each image


|

.. _sec-bundledplugins-fits:

FITS
===============================================

FITS (Flexible Image Transport System) is an image file format used for
scientific applications, particularly professional astronomy. FITS files use
the file extension :file:`.fits`. Official FITS specs and other info may be
found at: http://fits.gsfc.nasa.gov/

OpenImageIO supports multiple images in FITS files, and supports the
following pixel data types: UINT8, UINT16, UINT32, FLOAT, DOUBLE.

FITS files can store various kinds of arbitrary data arrays, but
OpenImageIO's support of FITS is mostly limited using FITS for image
storage.  Currently, OpenImageIO only supports 2D FITS data (images), not 3D
(volume) data, nor 1-D or higher-dimensional arrays.



.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - FITS header data or explanation
   * - ``Orientation``
     - int
     - derived from FITS "ORIENTAT" field.
   * - ``DateTime``
     - string
     - derived from the FITS "DATE" field.
   * - ``Comment``
     - string
     - FITS "COMMENT" (*)
   * - ``History``
     - string
     - FITS "HISTORY" (*)
   * - ``Hierarch``
     - string
     - FITS "HIERARCH" (*)
   * - ``oiio:subimages``
     - int
     - The number of subimages in the file.
   * - *other*
     - 
     - all other FITS keywords will be added to the ImageSpec as arbitrary
       named metadata.

.. note:: If the file contains multiple COMMENT, HISTORY, or HIERARCH
  fields, their text will be appended to form a single attribute (of
  each) in OpenImageIO's ImageSpec.


|

.. _sec-bundledplugins-gif:

GIF
===============================================

GIF (Graphics Interchange Format) is an image file format developed by
CompuServe in 1987.  Nowadays it is widely used to display basic animations
despite its technical limitations.

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - GIF header data or explanation
   * - ``gif:Interlacing``
     - int
     - Specifies if image is interlaced (0 or 1).
   * - ``FramesPerSecond``
     - int[2] (rational)
     - Frames per second
   * - ``oiio:Movie``
     - int
     - If nonzero, indicates that it's a multi-subimage file intended to represent an animation.
   * - ``oiio:LoopCount``
     - int
     - Number of times the animation should be played (0-65535, 0 stands for infinity).
   * - ``gif:LoopCount``
     - int
     - Deprecated synonym for ``oiio:LoopCount``.
   * - ``ImageDescription``
     - string
     - The GIF comment field.

**Configuration settings for GIF input**

When opening a GIF ImageInput with a *configuration* (see
Section :ref:`sec-input-with-config`), the following special configuration
options are supported:

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

**Configuration settings for GIF output**

When opening a GIF ImageOutput, the following special metadata tokens
control aspects of the writing itself:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Output configuration Attribute
     - Type
     - Meaning
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by writing to memory rather than the file system.

**Custom I/O Overrides**

GIF input and output support the "custom I/O" feature via the
special ``"oiio:ioproxy"`` attributes (see Sections
:ref:`sec-imageoutput-ioproxy` and :ref:`sec-imageinput-ioproxy`) as well as
the `set_ioproxy()` methods.

**Limitations**

* GIF only supports 3-channel (RGB) images and at most 8 bits per channel.
* Each subimage can include its own palette or use global palette. Palettes
  contain up to 256 colors of which one can be used as background color. It
  is then emulated with additional Alpha channel by OpenImageIO's reader.

|

.. _sec-bundledplugins-hdr:

HDR/RGBE
===============================================

HDR (High Dynamic Range), also known as RGBE (rgb with extended range),
is a simple format developed for the Radiance renderer to store high
dynamic range images.  HDR/RGBE files commonly use the file extensions
:file:`.hdr`.  The format is described in this section of the Radiance
documentation: http://radsite.lbl.gov/radiance/refer/filefmts.pdf

RGBE does not support tiles, multiple subimages, mipmapping, true half or
float pixel values, or arbitrary metadata.  Only RGB (3 channel) files are
supported.

RGBE became important because it was developed at a time when no standard
file formats supported high dynamic range, and is still used for many legacy
applications and to distribute HDR environment maps. But newer formats with
native HDR support, such as OpenEXR, are vastly superior and should be
preferred except when legacy file access is required.

**Attributes**

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - RGBE header data or explanation
   * - ``Orientation``
     - int
     - encodes the orientation (see Section :ref:`sec-metadata-orientation`)
   * - ``oiio:ColorSpace``
     - string
     - Color space (see Section :ref:`sec-metadata-color`). We currently
       assume that any RGBE files encountered are linear with sRGB primaries.
   * - ``oiio:Gamma``
     - float
     - the gamma correction specified in the RGBE header (if it's gamma corrected).
   * - ``heif:Orientation``
     - int
     - If the configuration option ``heif:reorient`` is nonzero and
       reorientation was performed, this will be set to the original
       orientation in the file.


**Configuration settings for HDR input**

When opening an HDR ImageInput with a *configuration* (see
Section :ref:`sec-input-with-config`), the following special configuration
options are supported:

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
   * - ``oiio:reorient``
     - int
     - The default of 1 means to let libheif auto-reorient the image to
       undo the camera's orientation (this will set a "heif:Orientation"
       metadata to the Exif orientation code indicating the original
       orientation of the image). If this hint is set to 0, the pixels will be
       left in their orientation as stored in the file, and the "Orientation"
       metadata will reflect that.

**Configuration settings for HDR output**

When opening a HDR ImageOutput, the following special metadata tokens
control aspects of the writing itself:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Output configuration Attribute
     - Type
     - Meaning
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by writing to memory rather than the file system.

**Custom I/O Overrides**

HDR input and output support the "custom I/O" feature via the
special ``"oiio:ioproxy"`` attributes (see Sections
:ref:`sec-imageoutput-ioproxy` and :ref:`sec-imageinput-ioproxy`) as well as
the `set_ioproxy()` methods.

|

.. _sec-bundledplugins-heif:

HEIF/HEIC/AVIF
===============================================

HEIF is a container format for images compressed with various compression
standards (HEIC is based on HEVC/H.265, AVIF is based on AV1). HEIC is used
commonly for iPhone camera pictures, but it is not Apple-specific and will
probably become more popular on other platforms in coming years. HEIF files
usually use the file extension :file:`.HEIC` or :file:`.AVIF` depending on their
main compression type.

HEIC and AVIF compression formats are lossy, but are higher visual quality than
JPEG while taking <= half the file size. Currently, OIIO's HEIF reader supports
reading files as RGB or RGBA, uint8 pixel values. Multi-image files are
currently supported for reading, but not yet writing. All pixel data is
uint8, though we hope to add support for HDR (more than 8 bits) in the
future.

The default behavior of the HEIF reader is to reorient the image to the
orientation indicated by the camera, and to report the "Orientation" metadata
as 1 (indicating that the image should be displayed as returned) and set the
"oiio:OriginalOrientation" metadata to what was originally stored in the file.
If you want to read the image without automatic reorientation, you can set the
configuration option "oiio:reorient" to 0, in which case the pixels will be
left in their orientation as stored in the file, and the "Orientation"
metadata will reflect that.

**Configuration settings for HEIF input**

When opening an HEIF ImageInput with a *configuration* (see
Section :ref:`sec-input-with-config`), the following special configuration
attributes are supported:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Input Configuration Attribute
     - Type
     - Meaning
   * - ``oiio:UnassociatedAlpha``
     - int
     - If nonzero, and the file contains unassociated alpha, this will
       cause the reader to leave alpha unassociated (versus the default of
       premultiplying color channels by alpha if the alpha channel is
       unassociated).
   * - ``oiio:reorient``
     - int
     - If nonzero, asks libheif to reorient any images (and report them as
       having Orientation 1). If zero, then libheif will not reorient the
       image and the Orientation metadata will be set to reflect the camera
       orientation.

**Configuration settings for HEIF output**

When opening an HEIF ImageOutput, the following special metadata tokens
control aspects of the writing itself:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - HEIF header data or explanation
   * - ``Compression``
     - string
     - If supplied, can be ``"heic"`` or ``"avif"``, but may optionally have a
       quality value appended, like ``"heic:90"``. Quality can be 1-100, with
       100 meaning lossless. The default is 75.



|

.. _sec-bundledplugins-ico:

ICO
===============================================

ICO is an image file format used for small images (usually icons) on
Windows.  ICO files use the file extension :file:`.ico`.

**Attributes**

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - ICO header data or explanation
   * - ``oiio:BitsPerSample``
     - int
     - the true bits per sample in the ICO file.
   * - ``ico:PNG``
     - int
     - if nonzero, will cause the ICO to be written out using PNG format.

**Configuration settings for ICO input**

When opening an ICO ImageInput with a *configuration* (see
Section :ref:`sec-input-with-config`), the following special configuration
attributes are supported:

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

**Configuration settings for ICO output**

When opening an ICO ImageOutput, the following special metadata tokens
control aspects of the writing itself:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Output Configuration Attribute
     - Type
     - Meaning
   * - ``oiio:dither``
     - int
     - If nonzero and outputting UINT8 values in the file from a source of
       higher bit depth, will add a small amount of random dither to combat
       the appearance of banding.

**Limitations**

* ICO only supports UINT8 and UINT16 formats; all output images will
  be silently converted to one of these.
* ICO only supports *small* images, up to 256 x 256.
  Requests to write larger images will fail their ``open()`` call.



|

.. _sec-bundledplugins-iff:

IFF
===============================================

IFF files are used by Autodesk Maya and use the file extension :file:`.iff`.

**Attributes**

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - IFF header data or explanation
   * - ``Artist``
     - string
     - The IFF "author"
   * - ``DateTime``
     - string
     - Creation date/time
   * - ``compression``
     - string
     - The compression type (``"none"`` or ``"rle"`` [default])
   * - ``oiio:BitsPerSample``
     - int
     - the true bits per sample of the IFF file.

**Configuration settings for IFF input**

When opening a IFF ImageInput with a *configuration* (see
Section :ref:`sec-input-with-config`), the following special configuration
options are supported:

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

**Configuration settings for IFF output**

When opening an IFF ImageOutput, the following special metadata tokens
control aspects of the writing itself:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Output Configuration Attribute
     - Type
     - Meaning
   * - ``oiio:dither``
     - int
     - If nonzero and outputting UINT8 values in the file from a source of
       higher bit depth, will add a small amount of random dither to combat
       the appearance of banding.
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by reading from memory rather than the file system.

**Custom I/O Overrides**

RLA input and output support the "custom I/O" feature via the
special ``"oiio:ioproxy"`` attributes (see Sections
:ref:`sec-imageoutput-ioproxy` and :ref:`sec-imageinput-ioproxy`) as well as
the `set_ioproxy()` methods.



|

.. _sec-bundledplugins-jpeg:

JPEG
===============================================

JPEG (Joint Photographic Experts Group), or more properly the JFIF file
format containing JPEG-compressed pixel data, is one of the most popular
file formats on the Internet, with applications, and from digital
cameras, scanners, and other image acquisition devices.  JPEG/JFIF files
usually have the file extension :file:`.jpg`, :file:`.jpe`, :file:`.jpeg`,
:file:`.jif`, :file:`.jfif`, or :file:`.jfi`.  The JFIF file format is
described by http://www.w3.org/Graphics/JPEG/jfif3.pdf.

Although we strive to support JPEG/JFIF because it is so widely used, we
acknowledge that it is a poor format for high-end work: it supports only
1- and 3-channel images, has no support for alpha channels, no support
for high dynamic range or even 16 bit integer pixel data, by convention
stores sRGB data and is ill-suited to linear color spaces, and does not
support multiple subimages or MIPmap levels.  There are newer formats
also blessed by the Joint Photographic Experts Group that attempt to
address some of these issues, such as JPEG-2000, but these do not have
anywhere near the acceptance of the original JPEG/JFIF format.

**Attributes**

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - JPEG header data or explanation
   * - ``ImageDescription``
     - string
     - the JPEG Comment field
   * - ``Orientation``
     - int
     - the image orientation
   * - ``XResolution``, ``YResolution``, ``ResolutionUnit``
     -
     - The resolution and units from the Exif header
   * - ``Compression``
     - string
     - If supplied, must be ``"jpeg"``, but may optionally have a quality
       value appended, like ``"jpeg:90"``. Quality can be 1-100, with 100
       meaning lossless.
   * - ``ICCProfile``
     - uint8[]
     - The ICC color profile. A variety of other ``ICCProfile:*`` attributes
       may also be present, extracted from the main profile.
   * - ``jpeg:subsampling``
     - string
     - Describes the chroma subsampling, e.g., ``"4:2:0"`` (the default),
       ``"4:4:4"``, ``"4:2:2"``, ``"4:2:1"``.
   * - ``Exif:*``, ``IPTC:*``, ``XMP:*``, ``GPS:*``
     - 
     - Extensive Exif, IPTC, XMP, and GPS data are supported by the
       reader/writer, and you should assume that nearly everything described
       Appendix :ref:`chap-stdmetadata` is properly translated when using
       JPEG files.

**Configuration settings for JPEG input**

When opening a JPEG ImageInput with a *configuration* (see
Section :ref:`sec-input-with-config`), the following special configuration
attributes are supported:

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

**Configuration settings for JPEG output**

When opening a JPEG ImageOutput, the following special metadata tokens
control aspects of the writing itself:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Output Configuration Attribute
     - Type
     - Meaning
   * - ``oiio:dither``
     - int
     - If nonzero and outputting UINT8 values in the file from a source of
       higher bit depth, will add a small amount of random dither to combat
       the appearance of banding.
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by writing to a memory buffer.
   * - ``jpeg:progressive``
     - int
     - If nonzero, will write a progressive JPEG file.


**Custom I/O Overrides**

JPEG input and output support the "custom I/O" feature
via the `ImageInput::set_ioproxy()` method and the special
``"oiio:ioproxy"`` attributes (see Section :ref:`sec-imageinput-ioproxy`).

**Limitations**

* JPEG/JFIF only supports 1- (grayscale) and 3-channel (RGB) images. As a
  special case, OpenImageIO's JPEG writer will accept n-channel image
  data, but will only output the first 3 channels (if n >= 3) or the first
  channel (if n <= 2), silently drop any extra channels from the output.
* Since JPEG/JFIF only supports 8 bits per channel, OpenImageIO's
  JPEG/JFIF writer will silently convert to UINT8 upon output,
  regardless of requests to the contrary from the calling program.
* OpenImageIO's JPEG/JFIF reader and writer always operate in scanline
  mode and do not support tiled image input or output.



|

.. _sec-bundledplugins-jpeg2000:

JPEG-2000
===============================================

JPEG-2000 is a successor to the popular JPEG/JFIF format, that supports
better (wavelet) compression and a number of other extensions.  It's geared
toward photography. JPEG-2000 files use the file extensions :file:`.jp2` or
:file:`.j2k`. The official JPEG-2000 format specification and other helpful
info may be found at: http://www.jpeg.org/JPEG2000.htm

JPEG-2000 is not yet widely used, so OpenImageIO's support of it is
preliminary.  In particular, we are not yet very good at handling the
metadata robustly.

**Attributes**

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - JPEG-2000 header data or explanation
   * - ``jpeg2000:streamformat``
     - string
     - specifies the JPEG-2000 stream format (``"none"`` or ``"jpc"``)
   * - ``oiio:ColorSpace``
     - string
     - Color space (see Section :ref:`sec-metadata-color`).
   * - ``ICCProfile``
     - uint8[]
     - The ICC color profile. A variety of other ``ICCProfile:*`` attributes
       may also be present, extracted from the main profile.


**Configuration settings for JPEG-2000 input**

When opening an JPEG-2000 ImageInput with a *configuration* (see
Section :ref:`sec-input-with-config`), the following special configuration
attributes are supported:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Input Configuration Attribute
     - Type
     - Meaning
   * - ``oiio:UnassociatedAlpha``
     - int
     - If nonzero, will leave alpha unassociated (versus the default of
       premultiplying color channels by alpha if the alpha channel is
       unassociated).
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by reading from memory rather than the file system.

**Configuration settings for JPEG-2000 output**

When opening a JPEG-2000 ImageOutput, the following special metadata tokens
control aspects of the writing itself:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Output Configuration Attribute
     - Type
     - Meaning
   * - ``oiio:dither``
     - int
     - If nonzero and outputting UINT8 values in the file from a source of
       higher bit depth, will add a small amount of random dither to combat
       the appearance of banding.
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by writing to a memory buffer.
   * - ``oiio:UnassociatedAlpha``
     - int
     - If nonzero, indicates that the data being passed is already in
       unassociated form (non-premultiplied colors) and should stay that way
       for output rather than being assumed to be associated and get automatic
       un-association to store in the file.

**Custom I/O Overrides**

JPEG-2000 input and output both support the "custom I/O" feature via the
special ``"oiio:ioproxy"`` attributes (see Sections
:ref:`sec-imageoutput-ioproxy` and :ref:`sec-imageinput-ioproxy`) as well as
the `set_ioproxy()` methods.


|

.. _sec-bundledplugins-ffmpeg:

Movie formats (using ffmpeg)
===============================================

The :program:`ffmpeg`-based reader is capable of reading the individual
frames from a variety of movie file formats, including:


=====================   ====================================================
Format                  Extensions
=====================   ====================================================
AVI                     :file:`.avi`
QuickTime               :file:`.qt`, :file:`.mov`
MPEG-4                  :file:`.mp4`, :file:`.m4a`, :file:`.m4v`
3GPP files              :file:`.3gp`, :file:`.3g2`
Motion JPEG-2000        :file:`.mj2`
Apple M4V               :file:`.m4v`
MPEG-1/MPEG-2           :file:`.mpg`
=====================   ====================================================

The format list include may other file types as well. We rely on the
:program:`ffmpeg` library to read these files, so the actual list of supported
formats may vary depending on the version of :program:`ffmpeg` that was linked
into OpenImageIO.

Currently, these files may only be read. Write support may be added in a
future release.  Also, currently, these files simply look to OIIO like
simple multi-image files and not much support is given to the fact that they
are technically *movies* (for example, there is no support for reading audio
information).

Some special attributes are used for movie files:


.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - Header data or explanation
   * - ``oiio:Movie``
     - int
     - Nonzero value for movie files
   * - ``oiio:subimages``
     - int
     - The number of frames (subimages) in the movie.
   * - ``FramesPerSecond``
     - int[2] (rational)
     - Frames per second



|

.. _sec-bundledplugins-null:

Null format
===============================================

The ``nullptr`` reader/writer is a mock-up that does not perform any actual
I/O. The reader just returns constant-colored pixels, and the writer just
returns directly without saving any data. This has several uses:

* Benchmarking, if you want to have OIIO's input or output truly take as
  close to no time whatsoever.

* "Dry run" of applications where you don't want it to produce any real
  output (akin to a Unix command that you redirect output to
  :file:`/dev/null`).

* Make "fake" input that looks like a file, but the file doesn't exist (if
  you are happy with constant-colored pixels).

The filename allows a REST-ful syntax, where you can append modifiers
that specify things like resolution (of the non-existent file), etc.
For example::

    foo.null?RES=640x480&CHANNELS=3

would specify a null file with resolution 640x480 and 3 channels.
Token/value pairs accepted are:

=====================   ====================================================
``RES=1024x1024``       Set resolution (3D example: 256x256x100)
``CHANNELS=4``          Set number of channels
``TILES=64x64``         Makes it look like a tiled image with tile size
``TYPE=uint8``          Set the pixel data type
``PIXEL=r,g,b,...``     Set pixel values (comma separates channel values)
``TEX=1``               Make it look like a full MIP-mapped texture
``attrib=value``        Anything else will set metadata
=====================   ====================================================




|

.. _sec-bundledplugins-openexr:

OpenEXR
===============================================

OpenEXR is an image file format developed by Industrial Light & Magic,
and subsequently open-sourced.  OpenEXR's strengths include support of
high dynamic range imagery (``half`` and `float` pixels), tiled
images, explicit support of MIPmaps and cubic environment maps,
arbitrary metadata, and arbitrary numbers of color channels.  OpenEXR
files use the file extension :file:`.exr`.
The official OpenEXR site is http://www.openexr.com/.

**Attributes**

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - OpeneEXR header data or explanation

   * - ``width``, ``height``, ``x``, ``y``
     - int
     - ``dataWindow``
   * - ``full_width``, ``full_height``, ``full_x``, ``full_y``
     - int
     - ``displayWindow``
   * - ``worldtocamera``
     - matrix
     - worldToCamera
   * - ``worldtoscreen``
     - matrix
     - worldToScreen
   * - ``worldtoNDC``
     - matrix
     - worldToNDC
   * - ``ImageDescription``
     - string
     - comments
   * - ``Copyright``
     - string
     - owner
   * - ``DateTime``
     - string
     - capDate
   * - ``PixelAspectRatio``
     - float
     - pixelAspectRatio
   * - ``ExposureTime``
     - float
     - expTime
   * - ``FNumber``
     - float
     - aperture
   * - ``compression``
     - string
     - one of: ``"none"``, ``"rle"``, ``"zip"``, ``"zips"``, ``"piz"``,
       ``"pxr24"``, ``"b44"``, ``"b44a"``, ``"dwaa"``, or ``"dwab"``.  If
       the writer receives a request for a compression type it does not
       recognize or is not supported by the version of OpenEXR on the
       system, it will use ``"zip"`` by default. For ``"dwaa"`` and
       ``"dwab"``, the dwaCompressionLevel may be optionally appended to the
       compression name after a colon, like this: ``"dwaa:200"``. (The
       default DWA compression value is 45.) For ``"zip"`` and ``"zips"``
       compression, a level from 1 to 9 may be appended (the default is
       ``"zip:4"``), but note that this is only honored when building
       against OpenEXR 3.1.3 or later.
   * - ``textureformat``
     - string
     - ``"Plain Texture"`` for MIP-mapped OpenEXR files, ``"CubeFace
       Environment"`` or ``"Latlong Environment"`` for OpenEXR environment
       maps.  Non-environment non-MIP-mapped OpenEXR files will not set this
       attribute.
   * - ``wrapmodes``
     - string
     - wrapmodes
   * - ``FramesPerSecond``
     - int[2]
     - Frames per second playback rate (vecsemantics will be marked as RATIONAL)
   * - ``captureRate``
     - int[2]
     - Frames per second capture rate (vecsemantics will be marked as RATIONAL)
   * - ``oiio:subimages``
     - int
     - The number of "parts" (subimages) in the file.
   * - ``smpte:TimeCode``
     - int[2]
     - SMPTE time code (vecsemantics will be marked as TIMECODE)
   * - ``smpte:KeyCode``
     - int[7]
     - SMPTE key code (vecsemantics will be marked as KEYCODE)
   * - ``openexr:lineOrder``
     - string
     - OpenEXR lineOrder attribute: ``"increasingY"``, ``"randomY"``, or
       ``"decreasingY"``.
   * - ``openexr:roundingmode``
     - int
     - the MIPmap rounding mode of the file.
   * - ``openexr:dwaCompressionLevel``
     - float
     - compression level for dwaa or dwab compression (default: 45.0).
   * - ``openexr::luminancechroma``
     - int
     - If nonzero, indicates whether the image is a luminance-chroma image.
       Upon reading, the subsampled Y/BY/RY(/A) channels of luminance-chroma
       images are automatically converted to RGB(A) channels.
   * - *other*
     - 
     - All other attributes will be added to the ImageSpec by their name and
       apparent type.


**Configuration settings for OpenEXR input**

When opening an OpenEXR ImageInput with a *configuration* (see
Section :ref:`sec-input-with-config`), the following special configuration
attributes are supported:

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
   * - ``oiio:missingcolor``
     - float *or* string
     - Either an array of float values or a string holding a comma-separated
       list of values, if present this is a request to use this color for
       pixels of any missing tiles or scanlines, rather than considering a
       tile/scanline read failure to be an error. This can be helpful when
       intentionally reading partially-written or incomplete files (such as
       an in-progress render).

**Configuration settings for OpenEXR output**

When opening an OpenEXR ImageOutput, the following special metadata tokens
control aspects of the writing itself:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Output Configuration Attribute
     - Type
     - Meaning
   * - ``oiio:RawColor``
     - int
     - If nonzero, writing images with non-RGB color models (such as YCbCr)
       will keep unaltered pixel values (versus the default OIIO behavior
       of automatically converting from RGB to the designated color space
       as the pixels are written).
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by writing to a memory buffer.


**Custom I/O Overrides**

OpenEXR input and output both support the "custom I/O" feature via the
special ``"oiio:ioproxy"`` attributes (see Sections
:ref:`sec-imageoutput-ioproxy` and :ref:`sec-imageinput-ioproxy`) as well as
the `set_ioproxy()` methods.

**A note on channel names**

The underlying OpenEXR library (:file:`libIlmImf`) always saves channels
into lexicographic order, so the channel order on disk (and thus when read!)
will NOT match the order when the image was created.

But in order to adhere to OIIO's convention that RGBAZ will always be the
first channels (if they exist), OIIO's OpenEXR reader will automatically
reorder just those channels to appear at the front and in that order. All
other channel names will remain in their relative order as presented to OIIO
by :file:`libIlmImf`.

**Limitations**

* The OpenEXR format only supports HALF, FLOAT, and UINT32 pixel
  data.  OpenImageIO's OpenEXR writer will silently convert data in formats
  (including the common UINT8 and UINT16 cases) to HALF data for output.

* Subsampled channels are not supported with the exception of reading
  luminance-chroma images with vertical and horizontal sampling rates of 2.
  This limited support does not work when OpenEXR's C Core API in used, only
  when OpenEXR's C++ API is used. Furthermore, it does not work in
  combination with tiles, multiple subimages, mipmapping, or deep pixels.


|

.. _sec-bundledplugins-openvdb:

OpenVDB
===============================================

OpenVDB is an open-source volume data file format.  OpenVDB files commonly
use the extension :file:`.vdb`. The official OpenVDB site is:
http://www.openvdb.org/ Currently, OpenImageIO only reads OpenVDB files, and
does not write them.

Volumes are comprised of multiple *layers* (which appear to OpenImageIO as
subimages).  Each layer/subimage may have a different name, resolution, and
coordinate mapping.  Layers may be scalar (1 channel) or vector (3 channel)
fields, and the voxel data are always `float`. OpenVDB files always
report as tiled, using the leaf dimension size.

**Attributes**

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - OpenVDB header data or explanation
   * - ``ImageDescription``
     - string
     - Description of image element
   * - ``oiio:subimagename``
     - string
     - unique layer name
   * - ``oiio:subimages``
     - int
     - The number of "layers" (subimages) in the file.
   * - ``openvdb:indextoworld``
     - matrix of doubles
     - conversion of voxel index to world space coordinates.
   * - ``openvdb:worldtoindex``
     - matrix of doubles
     - conversion of world space coordinates to voxel index.
   * - ``worldtocamera``
     - matrix
     - World-to-local coordinate mapping.



|

.. _sec-bundledplugins-png:

PNG
===============================================

PNG (Portable Network Graphics) is an image file format developed by the
open source community as an alternative to the GIF, after Unisys started
enforcing patents allegedly covering techniques necessary to use GIF. PNG
files use the file extension :file:`.png`.

**Attributes**

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - PNG header data or explanation
   * - ``ImageDescription``
     - string
     - Description
   * - ``Artist``
     - string
     - Author
   * - ``DocumentName``
     - string
     - Title
   * - ``DateTime``
     - string
     - the timestamp in the PNG header
   * - ``PixelAspectRatio``
     - float
     - pixel aspect ratio
   * - ``XResolution``, ``YResolution``, ``ResolutionUnit``
     - 
     - resolution and units from the PNG header.
   * - ``oiio:ColorSpace``
     - string
     - Color space (see Section :ref:`sec-metadata-color`).
   * - ``oiio:Gamma``
     - float
     - the gamma correction value (if specified).
   * - ``ICCProfile``
     - uint8[]
     - The ICC color profile. A variety of other ``ICCProfile:*`` attributes
       may also be present, extracted from the main profile.

**Configuration settings for PNG input**

When opening an PNG ImageInput with a *configuration* (see
Section :ref:`sec-input-with-config`), the following special configuration
attributes are supported:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Input Configuration Attribute
     - Type
     - Meaning
   * - ``oiio:UnassociatedAlpha``
     - int
     - If nonzero, will leave alpha unassociated (versus the default of
       premultiplying color channels by alpha if the alpha channel is
       unassociated).
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by reading from memory rather than the file system.

**Configuration settings for PNG output**

When opening an PNG ImageOutput, the following special metadata tokens
control aspects of the writing itself:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Output Configuration Attribute
     - Type
     - Meaning
   * - ``oiio:dither``
     - int
     - If nonzero and outputting UINT8 values in the file from a source of
       higher bit depth, will add a small amount of random dither to combat
       the appearance of banding.
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by writing to a memory buffer.
   * - ``png:compressionLevel``
     - int
     - Compression level for zip/deflate compression, on a scale from 0
       (fastest, minimal compression) to 9 (slowest, maximal compression).
       The default is 6. PNG compression is always lossless.
   * - ``png:filter``
     - int
     - Controls the "row filters" that prepare the image for optimal
       compression. The default is 0 (``PNG_NO_FILTERS``), but other values
       (which may be "or-ed" or summed to combine their effects) are 8
       (``PNG_FILTER_NONE``), 16 (``PNG_FILTER_SUB``), 32
       (``PNG_FILTER_UP``), 64 (``PNG_FILTER_AVG``), or 128
       (``PNG_FILTER_PAETH``).

       **Important**: We have noticed that 8 (PNG_FILTER_NONE) is much
       faster than the default of NO_FILTERS (sometimes 3x or more faster),
       but it also makes the resulting files quite a bit larger (sometimes
       2x larger). If you need to optimize PNG write speed and are willing
       to have larger PNG files on disk, you may want to use that value for
       this attribute.

**Custom I/O Overrides**

PNG input and output both support the "custom I/O" feature via the special
``"oiio:ioproxy"`` attributes (see Sections :ref:`sec-imageoutput-ioproxy`
and :ref:`sec-imageinput-ioproxy`) as well as the `set_ioproxy()` methods.



**Limitations**

* PNG stupidly specifies that any alpha channel is "unassociated" (i.e.,
  that the color channels are not "premultiplied" by alpha). This is a
  disaster, since it results in bad loss of precision for alpha image
  compositing, and even makes it impossible to properly represent certain
  additive glows and other desirable pixel values. OpenImageIO automatically
  associates alpha (i.e., multiplies colors by alpha) upon input and
  deassociates alpha (divides colors by alpha) upon output in order to
  properly conform to the OIIO convention (and common sense) that all pixel
  values passed through the OIIO APIs should use associated alpha.

* PNG only supports UINT8 and UINT16 output; other requested formats will be
  automatically converted to one of these.



|

.. _sec-bundledplugins-pnm:

PNM / Netpbm
===============================================

The Netpbm project, a.k.a. PNM (portable "any" map) defines PBM, PGM, PPM
and later added PFM (portable float map) as a set of simple image formats
(portable bitmap, portable graymap, portable pixmap) files.
Without loss of generality, we will refer to these all collectively as
"PNM."  These files have extensions :file:`.pbm`, :file:`.pgm`,
:file:`.ppm`, :file:`.pfm` and customarily correspond to bi-level bitmaps,
1-channel grayscale, and 3-channel RGB files, respectively, or :file:`.pnm`
for those who reject the nonsense about naming the files depending on the
number of channels and bitdepth.

PNM files are widely used in the Unix world as simple ASCII or binary image 
files that are easy to read and write. They are not compressed, and are
not particularly efficient for large images. They are not widely used in
the professional graphics world, but because of their historical
significance and extreme simplicity, OpenImageIO supports them.
PNM files do not support anything other than 1 or 3 channels, no tiles,
no multi-image, no MIPmapping.

The pbm, pgm, and ppm varieties are stored with scanlines ordered in the
file as top-to-bottom (the same as the usual OIIO convention), but the
float-based pfm files are conventionally ordered in the file as
bottom-to-top. Therefore, by default, reading and writing of the pfm
variety will automatically flip the image so that an application calling
the OpenImageIO API can, as usual, assume that scanline 0 is the visual
"top" (even though it is actually the last scanline stored in the file).

Both the reader and writer accept configuration hints "pnm:pfmflip"
(default: 1), which if set to 0 will disable this flipping and ensure
that scanline 0 is written as the first in the file (therefore
representing what PFM assumes is the visual "bottom" of the image).
This hint only affects PFM files and has no effect on the pbm, pgm,
or ppm varieties.

**Attributes**

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - PNM header data or explanation
   * - ``oiio:BitsPerSample``
     - int
     - The true bits per sample of the file (1 for true PBM files, even
       though OIIO will report the ``format`` as UINT8).
   * - ``pnm:binary``
     - int
     - nonzero if the file itself used the PNM binary format, 0 if it used
       ASCII.  The PNM writer honors this attribute in the ImageSpec to
       determine whether to write an ASCII or binary file.

**Configuration settings for PNM input**

When opening a PNM ImageInput with a *configuration* (see
Section :ref:`sec-input-with-config`), the following special configuration
attributes are supported:

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
   * - ``pnm:bigendian``
     - int
     - If nonzero, the PNM file is big-endian (the default is little-endian).  
   * - ``pnm:pfmflip``
      - int
      - If this configuration hint is present and is zero, the automatic
      vertical flipping of PFM image will be disabled (i.e., scanline 0 will
      really be the first one stored in the file). If nonzero (the default),
      float PFM files will store scanline 0 as the last scanline in the file
      (i.e. the visual "top" of the image).

**Configuration settings for PNM output**

When opening a PNM ImageOutput, the following special metadata tokens
control aspects of the writing itself:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Output Configuration Attribute
     - Type
     - Meaning
   * - ``oiio:dither``
     - int
     - If nonzero and outputting UINT8 values in the file from a source of
       higher bit depth, will add a small amount of random dither to combat
       the appearance of banding.
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by writing to a memory buffer.
   * - ``pnm:bigendian``
     - int
     - If nonzero, the PNM file is big-endian (the default is little-endian).
   * - ``pnm:binary``
     - int
     - nonzero if the file itself used the PNM binary format, 0 if it used
       ASCII.  The PNM writer honors this attribute in the ImageSpec to
       determine whether to write an ASCII or binary file.
       Float PFM files are always written in binary format.
   * - ``pnm:pfmflip``
      - int
      - If this configuration hint is present and is zero, for PFM files,
      scanline 0 will really be stored first in the file, thus disabling the
      usual automatically flipping that accounts for PFM files conventionally
      being stored in bottom-to-top order. If nonzero (the default), float
      PFM files will store scanline 0 as the last scanline in the file (i.e.
      the visual "top" of the image).

**Custom I/O Overrides**

PNM input and output both support the "custom I/O" feature via the
special ``"oiio:ioproxy"`` attributes (see Sections
:ref:`sec-imageoutput-ioproxy` and :ref:`sec-imageinput-ioproxy`) as well as
the `set_ioproxy()` methods.

|

.. _sec-bundledplugins-psd:

PSD
===============================================

PSD is the file format used for storing Adobe PhotoShop images. OpenImageIO
provides limited read abilities for PSD, but not currently the ability to
write PSD files.

**Attributes**

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - JPEG header data or explanation
   * - ``ICCProfile``
     - uint8[]
     - The ICC color profile. A variety of other ``ICCProfile:*`` attributes
       may also be present, extracted from the main profile.

**Configuration settings for PSD input**

When opening an ImageInput with a *configuration* (see
Section :ref:`sec-input-with-config`), the following special configuration
options are supported:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Input Configuration Attribute
     - Type
     - Meaning
   * - ``oiio:RawColor``
     - int
     - If nonzero, reading images with non-RGB color models (such as YCbCr
       or CMYK) will return unaltered pixel values (versus the default OIIO
       behavior of automatically converting to RGB).
   * - ``oiio:UnassociatedAlpha``
     - int
     - If nonzero, will leave alpha unassociated (versus the default of
       premultiplying color channels by alpha if the alpha channel is
       unassociated).
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by reading from memory rather than the file system.

Currently, the PSD format reader supports color modes RGB, CMYK,
multichannel, grayscale, indexed, and bitmap. It does NOT currently support
Lab or duotone modes.

**Custom I/O Overrides**

PSD output supports the "custom I/O" feature via the special ``"oiio:ioproxy"``
attributes (see Sections :ref:`sec-imageoutput-ioproxy` and
:ref:`sec-imageinput-ioproxy`) as well as the `set_ioproxy()` methods.

|

.. _sec-bundledplugins-ptex:

Ptex
===============================================

Ptex is a special per-face texture format developed by Walt Disney
Feature Animation.  The format and software to read/write it are open
source, and available from http://ptex.us/.  Ptex files commonly
use the file extension :file:`.ptex`.

OpenImageIO's support of Ptex is still incomplete.  We can read pixels from
Ptex files, but the TextureSystem doesn't properly filter across face
boundaries when using it as a texture.  OpenImageIO currently does not write
Ptex files at all.

**Attributes**

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - Ptex header data or explanation
   * - ``ptex:meshType``
     - string
     - the mesh type, either ``"triangle"`` or ``"quad"``.
   * - ``ptex:hasEdits``
     - int
     - nonzero if the Ptex file has edits.
   * - ``wrapmode``
     - string
     - the wrap mode as specified by the Ptex file.
   * - *other*
     -
     - Any other arbitrary metadata in the Ptex file will be stored directly
       as attributes in the ImageSpec.



|

.. _sec-bundledplugins-raw:

RAW digital camera files
===============================================

A variety of digital camera "raw" formats are supported via this
plugin that is based on the LibRaw library (http://www.libraw.org/).

**Configuration settings for RAW input**

When opening an ImageInput with a *configuration* (see
Section :ref:`sec-input-with-config`), the following special configuration
options are supported:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Input Configuration Attribute
     - Type
     - Meaning
   * - ``raw:auto_bright``
     - int
     - If nonzero, will use libraw's exposure correction. (Default: 0)
   * - ``raw:use_camera_wb``
     - int
     - If 1, use libraw's camera white balance adjustment. (Default: 1)
   * - ``raw:use_camera_matrix``
     - int
     - Whether to use the embedded color profile, if it's present: 0 =
       never, 1 (default) = only for DNG files, 3 = always.
   * - ``raw:adjust_maximum_thr``
     - float
     - If nonzero, auto-adjusting maximum value. (Default:0.0)
   * - ``raw:user_sat``
     - int
     - If nonzero, sets the camera maximum value that will be normalized to
       appear saturated. (Default: 0)
   * - ``raw:aber``
     - float[2]
     - Red and blue scale factors for chromatic aberration correction when
       decoding the raw image. The default (1,1) means to perform no
       correction. This is an overall spatial scale, sensible values will be
       very close to 1.0.
   * - ``raw:half_size``
     - int
     - If nonzero, outputs the image in half size. (Default: 0)
   * - ``raw:user_mul``
     - float[4]
     - Sets user white balance coefficients. Only applies if ``raw:use_camera_wb``
       is not equal to 0.
   * - ``raw:ColorSpace``
     - string
     - Which color primaries to use for the returned pixel values: ``raw``,
       ``sRGB``, ``sRGB-linear`` (sRGB primaries, but a linear transfer
       function), ``Adobe``, ``Wide``, ``ProPhoto``, ``ProPhoto-linear``,
       ``XYZ``, ``ACES`` (only supported by LibRaw >= 0.18), ``DCI-P3``
       (LibRaw >= 0.21), ``Rec2020`` (LibRaw >= 0.2). (Default: ``sRGB``)
   * - ``raw:Exposure``
     - float
     - Amount of exposure before de-mosaicing, from 0.25 (2 stop darken) to
       8.0 (3 stop brighten). (Default: 1.0, meaning no correction.)
   * - ``raw:Demosaic``
     - string
     - Force a demosaicing algorithm: ``linear``, ``VNG``, ``PPG``, ``AHD``
       (default), ``DCB``, ``AHD-Mod``, ``AFD``, ``VCD``, ``Mixed``,
       ``LMMSE``, ``AMaZE``, ``DHT``, ``AAHD``, ``none``.
   * - ``raw:HighlightMode``
     - int
     - Set libraw highlight mode processing: 0 = clip, 1 = unclip, 2 =
       blend, 3+ = rebuild. (Default: 0.)
   * - ``raw:balance_clamped``
     - int
     - If nonzero, balance any clamped highlight values in the image. Resolves issues
       where highlights take on an undesired hue shift due to incongruous channel
       sensor saturation.
       Enabling this option will change the output datatype to HALF.
       (Default: 0)
   * - ``raw:apply_scene_linear_scale``
     - int
     - If nonzero, applies an additional multiplication to the pixel values returned
       by libraw. See ``raw:camera_to_scene_linear_scale`` for more details.
       Enabling this option will change the output datatype to HALF.
       (Default: 0)
   * - ``raw:camera_to_scene_linear_scale``
     - float
     - Whilst the libraw pixel values are linear, they are normalized based on
       the whitepoint / sensor / ISO and shooting conditions. An additional multiplication
       is needed to bring exposure levels up so that a correctly photographed 18% grey card
       has pixel values at 0.18. Setting this metadata key implies ``raw:apply_scene_linear_scale``.
       Enabling this option will change the output datatype to HALF.
       (Default: 2.2222222222222223 (1.0/0.45))
   * - ``raw:user_flip``
     - int
     - Set libraw user flip value : -1 ignored, other values are between [0; 8] with the same 
       definition than the Exif orientation code.
   * - ``raw:threshold``
     - float
     - Libraw parameter for noise reduction through wavelet denoising.
       The best threshold should be somewhere between 100 and 1000.
       (Default: 0.0)
   * - ``raw:fbdd_noiserd``
     - int
     - Controls FBDD noise reduction before demosaic.
       0 - do not use FBDD noise reduction, 1 - light FBDD reduction,
       2 (and more) - full FBDD reduction
       (Default: 0)


|

.. _sec-bundledplugins-rla:

RLA
===============================================

RLA (Run-Length encoded, version A) is an early CGI renderer output format,
originating from Wavefront Advanced Visualizer and used primarily by
software developed at Wavefront.  RLA files commonly use the file extension
:file:`.rla`.

**Attributes**

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - RLA header data or explanation
   * - ``width``, ``height``, ``x``, ``y``
     - int
     - RLA "active/viewable" window.
   * - ``full_width``, ``full_height``, ``full_x``,
       ``full_y``
     - int
     - RLA "full" window.
   * - ``rla:FrameNumber``
     - int
     - frame sequence number.
   * - ``rla:Revision``
     - int
     - file format revision number, currently ``0xFFFE``.
   * - ``rla:JobNumber``
     - int
     - job number ID of the file.
   * - ``rla:FieldRendered``
     - int
     - whether the image is a field-rendered (interlaced) one ``0`` for false, non-zero for true.
   * - ``rla:FileName``
     - string
     - name under which the file was originally saved.
   * - ``ImageDescription``
     - string
     - RLA "Description" of the image.
   * - ``Software``
     - string
     - name of software used to save the image.
   * - ``HostComputer``
     - string
     - name of machine used to save the image.
   * - ``Artist``
     - string
     - RLA "UserName": logon name of user who saved the image.
   * - ``rla:Aspect``
     - string
     - aspect format description string.
   * - ``rla:ColorChannel``
     - string
     - textual description of color channel data format (usually ``rgb``).
   * - ``rla:Time``
     - string
     - description (format not standardized) of amount of time spent on creating the image.
   * - ``rla:Filter``
     - string
     - name of post-processing filter applied to the image.
   * - ``rla:AuxData``
     - string
     - textual description of auxiliary channel data format.
   * - ``rla:AspectRatio``
     - float
     - image aspect ratio.
   * - ``rla:RedChroma``
     - vec2 or vec3 of floats
     - red point XY (vec2) or XYZ (vec3) coordinates.
   * - ``rla:GreenChroma``
     - vec2 or vec3 of floats
     - green point XY (vec2) or XYZ (vec3) coordinates.
   * - ``rla:BlueChroma``
     - vec2 or vec3 of floats
     - blue point XY (vec2) or XYZ (vec3) coordinates.
   * - ``rla:WhitePoint``
     - vec2 or vec3 of floats
     - white point XY (vec2) or XYZ (vec3) coordinates.
   * - ``oiio:ColorSpace``
     - string
     - Color space (see Section :ref:`sec-metadata-color`).
   * - ``oiio:Gamma``
     - float
     - the gamma correction value (if specified).

**Configuration settings for RLA input**

When opening a RLA ImageInput with a *configuration* (see
Section :ref:`sec-input-with-config`), the following special configuration
options are supported:

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

**Configuration settings for RLA output**

When opening a RLA ImageOutput, the following special metadata tokens
control aspects of the writing itself:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Output Configuration Attribute
     - Type
     - Meaning
   * - ``oiio:dither``
     - int
     - If nonzero and outputting UINT8 values in the file from a source of
       higher bit depth, will add a small amount of random dither to combat
       the appearance of banding.
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by reading from memory rather than the file system.

**Custom I/O Overrides**

RLA input and output support the "custom I/O" feature via the
special ``"oiio:ioproxy"`` attributes (see Sections
:ref:`sec-imageoutput-ioproxy` and :ref:`sec-imageinput-ioproxy`) as well as
the `set_ioproxy()` methods.

**Limitations**

* OpenImageIO will only write a single image to each file, multiple
  subimages are not supported by the writer (but are supported by the
  reader).



|

.. _sec-bundledplugins-sgi:

SGI
===============================================

The SGI image format was a simple raster format used long ago on SGI
machines.  SGI files use the file extensions ``sgi``, ``rgb``, ``rgba``,
``bw``, `int`, and ``inta``.

The SGI format is sometimes used for legacy apps, but has little merit
otherwise: no support for tiles, no MIPmaps, no multi-subimage, only 8- and
16-bit integer pixels (no floating point), only 1-4 channels.

**Attributes**

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - SGI header data or explanation
   * - ``compression``
     - string
     - The compression of the SGI file (``rle``, if RLE compression is used).
   * - ``ImageDescription``
     - string
     - Image name.

**Configuration settings for SGI input**

When opening a SGI ImageInput with a *configuration* (see
Section :ref:`sec-input-with-config`), the following special configuration
options are supported:

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

**Configuration settings for SGI output**

When opening an SGI ImageOutput, the following special metadata tokens
control aspects of the writing itself:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Output Configuration Attribute
     - Type
     - Meaning
   * - ``oiio:dither``
     - int
     - If nonzero and outputting UINT8 values in the file from a source of
       higher bit depth, will add a small amount of random dither to combat
       the appearance of banding.
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by reading from memory rather than the file system.


**Custom I/O Overrides**

SGI input and output support the "custom I/O" feature via the
special ``"oiio:ioproxy"`` attributes (see Sections
:ref:`sec-imageoutput-ioproxy` and :ref:`sec-imageinput-ioproxy`) as well as
the `set_ioproxy()` methods.

|

.. _sec-bundledplugins-pic:

Softimage PIC
===============================================

Softimage PIC is an image file format used by the SoftImage 3D application,
and some other programs that needed to be compatible with it.  Softimage
files use the file extension :file:`.pic`.

The Softimage PIC format is sometimes used for legacy apps, but has little
merit otherwise, so currently OpenImageIO only reads Softimage files and is
unable to write them.

**Attributes**

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - PIC header data or explanation
   * - ``compression``
     - string
     - The compression of the SGI file (``rle``, if RLE compression is used).
   * - ``ImageDescription``
     - string
     - Comment
   * - ``oiio:BitsPerSample``
     - int
     - the true bits per sample of the PIC file.



|

.. _sec-bundledplugins-targa:

Targa
===============================================

Targa (a.k.a. Truevision TGA) is an image file format with little merit
except that it is very simple and is used by many legacy applications. Targa
files use the file extension :file:`.tga`, or, much more rarely,
:file:`.tpic`. The official Targa format specification may be found at:
http://www.dca.fee.unicamp.br/~martino/disciplinas/ea978/tgaffs.pdf

**Attributes**

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - TGA header data or explanation
   * - ``ImageDescription``
     - string
     - Comment
   * - ``Artist``
     - string
     - author
   * - ``DocumentName``
     - string
     - job name/ID
   * - ``Software``
     - string
     - software name
   * - ``DateTime``
     - string
     - TGA time stamp
   * - ``targa:JobTime``
     - string
     - TGA "job time."
   * - ``compression``
     - string
     - values of ``none`` and ``rle`` are supported.  The writer will use
       RLE compression if any unknown compression methods are requested.
   * - ``targa:alpha_type``
     - int
     - Meaning of any alpha channel (0 = none; 1 = undefined, ignore;
       2 = undefined, preserve; 3 = useful unassociated alpha;
       4 = useful associated alpha / premultiplied color).
   * - ``targa:ImageID``
     - string
     - Image ID
   * - ``targa:JobTime``
     - string
     - Job time
   * - ``targa:version``
     - int
     - TGA file format version (1 or 2)
   * - ``PixelAspectRatio``
     - float
     - pixel aspect ratio
   * - ``oiio:BitsPerSample``
     - int
     - the true bits per sample of the PIC file.
   * - ``oiio:ColorSpace``
     - string
     - Color space (see Section :ref:`sec-metadata-color`).
   * - ``oiio:Gamma``
     - float
     - the gamma correction value (if specified).

If the TGA file contains a thumbnail, its dimensions will be stored in the
attributes ``"thumbnail_width"``, ``"thumbnail_height"``, and
``"thumbnail_nchannels"``, and the thumbnail pixels themselves will be
retrievable via `ImageInput::get_thumbnail()` or `ImageBuf::thumbnail()` or
`ImageCache::get_thumbnail()`.

**Configuration settings for Targa input**

When opening an Targa ImageInput with a *configuration* (see
Section :ref:`sec-input-with-config`), the following special configuration
attributes are supported:

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
   * - ``oiio:UnassociatedAlpha``
     - int
     - If nonzero, and the file contains unassociated alpha, this will
       cause the reader to leave alpha unassociated (versus the default of
       premultiplying color channels by alpha if the alpha channel is
       unassociated).

**Configuration settings for Targa output**

When opening a Targa ImageOutput, the following special metadata tokens
control aspects of the writing itself:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Output Configuration Attribute
     - Type
     - Meaning
   * - ``oiio:dither``
     - int
     - If nonzero and outputting UINT8 values in the file from a source of
       higher bit depth, will add a small amount of random dither to combat
       the appearance of banding.
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by writing to a memory buffer.

**Custom I/O Overrides**

Targa input and output support the "custom I/O" feature via the
special ``"oiio:ioproxy"`` attributes (see Sections
:ref:`sec-imageoutput-ioproxy` and :ref:`sec-imageinput-ioproxy`) as well as
the `set_ioproxy()` methods.

**Limitations**

* The Targa reader reserves enough memory for the entire image. Therefore it
  is not a good choice for high-performance image use such as would be used
  for ImageCache or TextureSystem.
* Targa files only support 8- and 16-bit unsigned integers (no signed,
  floating point, or HDR capabilities); the OpenImageIO TGA writer will
  silently convert all output images to UINT8 (except if UINT16 is
  explicitly requested).
* Targa only supports grayscale, RGB, and RGBA; the OpenImageIO TGA writer
  will fail its call to ``open()`` if it is asked create a file with more
  than 4 color channels.


|

.. _sec-bundledplugins-term:

Term (Terminal)
===============================================

This *experimental* output-only "format" is actually a procedural output
that writes a low-res representation of the image to the console output. It
requires a terminal application that supports Unicode and 24 bit color
extensions.

The `term` ImageOutput supports the following special metadata tokens to
control aspects of the writing itself:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Output Configuration Attribute
     - Type
     - Meaning
   * - ``term:method``
     - string
     - May be one of `iterm2`, `24bit` (default), `24bit-space`, `256color`,
       or `dither`.
   * - ``term:fit``
     - int
     - If 1 (the default), the image will be resized to fit on the console
       window.



The `iterm2` mode is the best quality and is the default mode when actually
running on a Mac and launching using iTerm2 as the terminal. This mode uses
iTerm2's nonstandard extension to directly output an pixel array to be
visible in the terminal.

The default in other circumstances is the `24bit` mode, which displays two
approximately square pixels vertically in each character cell, by outputting
the Unicode "upper half block" glyph (`\u2508`) with the foreground color
set to the top pixel's color and the background color set to the bottom
pixel's color.

If this doesn't look right, or your terminal doesn't support Unicode,
the `24bit-space` is an alternate mode that displays one elongated pixel
in each character cell, writing a space character with the correct color.

There's also a `256color` method that just uses the 6x6x6 color space in the
256 color palette -- which looks horrible -- and an experimental `dither`
which does a half-assed Floyd-Steinberg dithering, horizontally only, and
frankly is not an improvement unless you squint really hard. These may
change or be eliminted in the future.

In all cases, the image will automatically be resized to fit in the terminal
and keep approximately the correct aspect ratio, as well as converted to
sRGB so it looks kinda ok.

|

.. _sec-bundledplugins-tiff:

TIFF
===============================================

TIFF (Tagged Image File Format) is a flexible file format created by Aldus,
now controlled by Adobe.  TIFF supports nearly everything anybody could want
in an image format (and has exactly the complexity you would expect from
such a requirement). TIFF files commonly use the file extensions
:file:`.tif` or, :file:`.tiff`. Additionally, OpenImageIO associates the
following extensions with TIFF files by default: :file:`.tx`, :file:`.env`,
:file:`.sm`, :file:`.vsm`.

The official TIFF format specification may be found here:
http://partners.adobe.com/public/developer/tiff/index.html The most popular
library for reading TIFF directly is :file:`libtiff`, available here:
http://www.remotesensing.org/libtiff/  OpenImageIO uses :file:`libtiff` for
its TIFF reading/writing.

We like TIFF a lot, especially since its complexity can be nicely hidden
behind OIIO's simple APIs.  It supports a wide variety of data formats
(though unfortunately not ``half``), an arbitrary number of channels, tiles
and multiple subimages (which makes it our preferred texture format), and a
rich set of metadata.

OpenImageIO supports the vast majority of TIFF features, including: tiled
images (``tiled``) as well as scanline images; multiple subimages per file
(``multiimage``); MIPmapping (using multi-subimage; that means you can't use
multiimage and MIPmaps simultaneously); data formats 8- 16, and 32 bit
integer (both signed and unsigned), and 32- and 64-bit floating point;
palette images (will convert to RGB); "miniswhite" photometric mode (will
convert to "minisblack").

The TIFF plugin attempts to support all the standard Exif, IPTC, and XMP
metadata if present.

**Configuration settings for TIFF input**

When opening an ImageInput with a *configuration* (see
Section :ref:`sec-input-with-config`), the following special configuration
options are supported:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Input Configuration Attribute
     - Type
     - Meaning
   * - ``oiio:UnassociatedAlpha``
     - int
     - If nonzero, and the file contains unassociated alpha, this will
       cause the reader to leave alpha unassociated (versus the default of
       premultiplying color channels by alpha if the alpha channel is
       unassociated).
   * - ``oiio:RawColor``
     - int
     - If nonzero, reading images with non-RGB color models (such as YCbCr)
       will return unaltered pixel values (versus the default OIIO behavior
       of automatically converting to RGB).
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by reading from memory rather than the file system.

**Configuration settings for TIFF output**

When opening an ImageOutput, the following special metadata tokens control
aspects of the writing itself:


.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Output Configuration Attribute
     - Type
     - Meaning
   * - ``oiio:BitsPerSample``
     - int
     - Requests a rescaling to a specific bits per sample (such as writing
       12-bit TIFFs).
   * - ``oiio:dither``
     - int
     - If nonzero and outputting UINT8 values in the file from a source of
       higher bit depth, will add a small amount of random dither to combat
       the appearance of banding.
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by writing to memory rather than the file system.
   * - ``oiio:UnassociatedAlpha``
     - int
     - If nonzero, any alpha channel is understood to be unassociated, and
       the EXTRASAMPLES tag in the TIFF file will be set to reflect this).
   * - ``tiff:write_exif``
     - int
     - If zero, will not write any Exif data to the TIFF file. (The default
       is 1.)
   * - ``tiff:half``
     - int
     - If nonzero, allow writing TIFF files with `half` (16 bit float)
       pixels. The default of 0 will automatically translate to float
       pixels, since most non-OIIO applications will not properly read half
       TIFF files despite their being legal.
   * - ``tiff:ColorSpace``
     - string
     - Requests that the RGB image be converted and saved in the TIFF file in
       a non-RGB color space. Choices are ``RGB``, ``CMYK``.  (Note that
       ``YCbCr``, ``CIELAB``, ``ICCLAB``, ``ITULAB`` are not yet supported
       for convertion. However, if the `oiio:ColorSpace` is one of those,
       meaning that the image data is presumed to already be in that
       space, the TIFF PhotometricInterpretation tag will be set to convey
       this information.)
   * - ``tiff:zipquality``
     - int
     - A time-vs-space knob for ``zip`` compression, ranging from 1-9
       (default is 6). Higher means compress to less space, but taking
       longer to do so. It is strictly a time vs space tradeoff, the visual
       image quality is identical (lossless) no matter what the setting.
   * - ``tiff:RowsPerStrip``
     - int
     - Overrides TIFF scanline rows per strip with a specific request (if
       not supplied, OIIO will choose a reasonable default).
   * - ``tiff:bigtiff``
     - int
     - If nonzero, forces use of "bigtiff," a nonstandard extension that
       allows files to be more than 4 GB (default: 0).
   * - ``tiff:write_extrasamples``
     - int
     - If zero, do NOT write the "EXTRASAMPLES" tag to the TIFF header.
       (The default is 1, which means write the tag.)
   * - ``tiff:write_iptc``
     - int
     - If nonzero, write an IPTC data block to the TIFF file.
       (The default is 0, which means not to write an IPTC block.)


**TIFF compression modes**

The full list of possible TIFF compression mode values are as
follows.

    ``none`` :sup:`*`
    ``lzw`` :sup:`*`
    ``zip`` :sup:`*`
    ``ccitt_t4``
    ``ccitt_t6``
    ``ccittfax3``
    ``ccittfax4``
    ``ccittrle2``
    ``ccittrle`` :sup:`*`
    ``dcs``
    ``isojbig``
    ``IT8BL``
    ``IT8CTPAD``
    ``IT8LW``
    ``IT8MP``
    ``jp2000``
    ``jpeg`` :sup:`*`
    ``lzma``
    ``next``
    ``ojpeg``
    ``packbits`` :sup:`*`
    ``pixarfilm``
    ``pixarlog``
    ``sgilog24``
    ``sgilog``
    ``T43``
    ``T85``
    ``thunderscan``

:sup:`*` indicates that OpenImageIO can write that format, and is not
part of the format name. The compression types without the asterisk are
supported for reading but not for writing.

**Custom I/O Overrides**

TIFF input and output support the "custom I/O" feature
via the `ImageInput::set_ioproxy()` method and the special
``"oiio:ioproxy"`` attributes (see Section :ref:`sec-imageinput-ioproxy`).

**Limitations**

OpenImageIO's TIFF reader and writer have some limitations you should be
aware of:

* No separate per-channel data formats (not supported by :file:`libtiff`).
* Only multiples of 8 bits per pixel may be passed through OpenImageIO's
  APIs, e.g., 1-, 2-, and 4-bits per pixel will be passed by OIIO as 8 bit
  images; 12 bits per pixel will be passed as 16, etc.  But the
  ``oiio:BitsPerSample`` attribute in the ImageSpec will correctly report
  the original bit depth of the file. Similarly for output, you must pass 8
  or 16 bit output, but ``oiio:BitsPerSample`` gives a hint about how you
  want it to be when written to the file, and it will try to accommodate the
  request (for signed integers, TIFF output can accommodate 2, 4, 8, 10, 12,
  and 16 bits).
* JPEG compression is limited to 8-bit per channel, 3-channel files.


**TIFF Attributes**

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - TIFF header data or explanation

   * - ``ImageSpec::x``
     - int
     - XPosition
   * - ``ImageSpec::y``
     - int
     - YPosition
   * - ``ImageSpec::full_width``
     - int
     - PIXAR_IMAGEFULLWIDTH
   * - ``ImageSpec::full_length``
     - int
     - PIXAR_IMAGEFULLLENGTH
   * - ``ImageDescription``
     - string
     - ImageDescription
   * - ``DateTime``
     - string
     - DateTime
   * - ``Software``
     - string
     - Software
   * - ``Artist``
     - string
     - Artist
   * - ``Copyright``
     - string
     - Copyright
   * - ``Make``
     - string
     - Make
   * - ``Model``
     - string
     - Model
   * - ``DocumentName``
     - string
     - DocumentName
   * - ``HostComputer``
     - string
     - HostComputer
   * - ``XResultion``, ``YResolution``
     - float
     - XResolution, YResolution
   * - ``ResolutionUnit``
     - string
     - ResolutionUnit (``in`` or ``cm``).
   * - ``Orientation``
     - int
     - Orientation
   * - ``ICCProfile``
     - uint8[]
     - The ICC color profile. A variety of other ``ICCProfile:*`` attributes
       may also be present, extracted from the main profile.
   * - ``textureformat``
     - string
     - PIXAR_TEXTUREFORMAT
   * - ``wrapmodes``
     - string
     - PIXAR_WRAPMODES
   * - ``fovcot``
     - float
     - PIXAR_FOVCOT
   * - ``worldtocamera``
     - matrix
     - PIXAR_MATRIX_WORLDTOCAMERA
   * - ``worldtoscreen``
     - matrix
     - PIXAR_MATRIX_WORLDTOSCREEN
   * - ``compression``
     - string
     - based on TIFF Compression (one of ``none``, ``lzw``, ``zip``, or others listed above).
   * - ``tiff:compression``
     - int
     - the original integer code from the TIFF Compression tag.
   * - ``tiff:planarconfig``
     - string
     - PlanarConfiguration (``separate`` or ``contig``).  The OpenImageIO TIFF writer will honor such a request in the ImageSpec.
   * - ``tiff:PhotometricInterpretation``
     - int
     - Photometric
   * - ``tiff:PageName``
     - string
     - PageName
   * - ``tiff:PageNumber``
     - int
     - PageNumber
   * - ``tiff:RowsPerStrip``
     - int
     - RowsPerStrip
   * - ``tiff:subfiletype``
     - int
     - SubfileType
   * - ``Exif:*``
     -
     - A wide variety of EXIF data are honored, and are all prefixed with `Exif`.
   * - ``oiio:BitsPerSample``
     - int
     - The actual bits per sample in the file (may differ from `ImageSpec::format`).
   * - ``oiio:UnassociatedAlpha``
     - int
     - Nonzero if the data returned by OIIO will have "unassociated" alpha.
   * - ``tiff:UnassociatedAlpha``
     - int
     - Nonzero if the data in the file had "unassociated" alpha (even if using
       the usual convention of returning associated alpha from the read
       methods).



|

.. _sec-bundledplugins-webp:

Webp
===============================================

WebP is an image file format developed by Google that is intended to be an
open standard for lossy-compressed images for use on the web.

**Attributes**

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - WebP header data or explanation
   * - ``oiio:Movie``
     - int
     - If nonzero, indicates that it's a multi-subimage file intended to
       represent an animation.
   * - ``oiio:LoopCount``
     - int
     - Number of times the animation should be played (0-65535, 0 stands for infinity).
   * - ``webp:LoopCount``
     - int
     - Deprecated synonym for ``oiio:LoopCount``.

**Configuration settings for WebP input**

When opening an WebP ImageInput with a *configuration* (see
Section :ref:`sec-input-with-config`), the following special configuration
attributes are supported:

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

**Configuration settings for WebP output**

When opening a WebP ImageOutput, the following special metadata tokens
control aspects of the writing itself:

.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - Output Configuration Attribute
     - Type
     - Meaning
   * - ``oiio:dither``
     - int
     - If nonzero and outputting UINT8 values in the file from a source of
       higher bit depth, will add a small amount of random dither to combat
       the appearance of banding.
   * - ``oiio:ioproxy``
     - ptr
     - Pointer to a ``Filesystem::IOProxy`` that will handle the I/O, for
       example by writing to a memory buffer.

**Custom I/O Overrides**

WebP input and output both support the "custom I/O" feature via the special
``"oiio:ioproxy"`` attributes (see Sections :ref:`sec-imageoutput-ioproxy` and
:ref:`sec-imageinput-ioproxy`) as well as the `set_ioproxy()` methods.

**Limitations**

* WebP only supports 3-channel (RGB) or 4-channel (RGBA) images and must
  be 8-bit unsigned integer pixel values (uint8).


|

.. _sec-bundledplugins-zfile:

Zfile
===============================================

Zfile is a very simple format for writing a depth (*z*) image, originally
from Pixar's PhotoRealistic RenderMan but now supported by many other
renderers.  It's extremely minimal, holding only a width, height,
world-to-screen and camera-to-screen matrices, and uncompressed float pixels
of the z-buffer. Zfile files use the file extension :file:`.zfile`.


.. list-table::
   :widths: 30 10 65
   :header-rows: 1

   * - ImageSpec Attribute
     - Type
     - Zfile header data or explanation
   * - ``worldtocamera``
     - matrix
     - NP
   * - ``worldtoscreen``
     - matrix
     - Nl

