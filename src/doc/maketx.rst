..
  Copyright Contributors to the OpenImageIO project.
  SPDX-License-Identifier: CC-BY-4.0


.. _chap-maketx:

Making Tiled MIP-Map Texture Files With `maketx` or `oiiotool`
##############################################################


Overview
========

The TextureSystem (Chapter :ref:`chap-texturesystem_`) will exhibit much
higher performance if the image files it uses as textures are tiled (versus
scanline) orientation, have multiple subimages at different resolutions
(MIP-mapped), and include a variety of header or metadata fields
appropriately set for texture maps. Any image that you intend to use as
input to TextureSystem, therefore, should first be converted to have these
properties. An ordinary image will work as a texture, but without this
additional step, it will be drastically less efficient in terms of memory,
disk or network I/O, and time.

This can be accomplished programmatically using the ImageBufAlgo
`make_texture()` function (see Section :ref:`sec-iba-importexport` for C++
and Section :ref:`sec-iba-py-importexport` for Python).

OpenImageIO includes two command-line utilities capable of converting
ordinary images into texture files: :program:`maketx` and
:program:`oiiotool`. [#]_

.. [#] Why are there two programs? Historical artifact -- :program:`maketx`
       existed first, and much later :program:`oiiotool` was extended to be
       capable of directly writing texture files. If you are simply
       converting an image into a texture, :program:`maketx` is more
       straightforward and foolproof, in that you can't accidentally forget
       to turn it into a texture, as you might do with and much later
       :program:`oiiotool` was extended to be capable of directly writing
       texture files.



.. sec-maketx:

`maketx`
========

The :program:`maketx` program will convert ordinary images to efficient
textures. The :program:`maketx` utility is invoked as follows:

    ``maketx`` [*options*] *input*... ``-o`` *output*


Where *input* and *output* name the input image and desired output filename.
The input files may be of any image format recognized by OpenImageIO (i.e.,
for which ImageInput plugins are available).  The file format of the output
image will be inferred from the file extension of the output filename (e.g.,
:filename:`foo.tif` will write a TIFF file).

Command-line arguments are:

.. option:: --help

    Prints usage information to the terminal.

.. option:: --version

    Prints the version designation of the OIIO library.

.. option:: -v

    Verbose status messages, including runtime statistics and timing.

.. option:: --runstats

    Print runtime statistics and timing.

.. option:: -o outputname

    Sets the name of the output texture.

.. option:: --threads <n>

    Use *n* execution threads if it helps to speed up image operations. The
    default (also if n=0) is to use as many threads as there are cores
    present in the hardware.

.. option:: --format <formatname>

    Specifies the image format of the output file (e.g., "tiff", "OpenEXR",
    etc.).  If `--format` is not used, :program:`maketx` will guess based on
    the file extension of the output filename; if it is not a recognized
    format extension, TIFF will be used by default.

.. option:: -d <datatype>

    Attempt to sets the output pixel data type to one of: `UINT8`, `sint8`,
    `uint16`, `sint16`, `half`, `float`, `double`.

    If the `-d` option is not supplied, the output data type will be the
    same as the data format of the input file.

    In either case, the output file format itself (implied by the file
    extension of the output filename) may trump the request if the file
    format simply does not support the requested data type.

.. option:: --tile <x> <y>

    Specifies the tile size of the output texture.  If not specified,
    :program:`maketx` will make 64 x 64 tiles.

.. option:: --separate

    Forces "separate" (e.g., RRR...GGG...BBB) packing of channels in the
    output file.  Without this option specified, "contiguous" (e.g.,
    RGBRGBRGB...) packing of channels will be used for those file formats
    that support it.

.. option:: --compression <method>
            --compression <method:quality>

    Sets the compression method, and optionally a quality setting, for the
    output image (the default is to try to use "zip" compression, if it is
    available).

.. option:: -u

    Ordinarily, textures are created unconditionally (which could take
    several seconds for large input files if read over a network) and will
    be stamped with the current time.
    
    The `-u` option enables *update mode*: if the output file already
    exists, and has the same time stamp as the input file, and the
    command-lie arguments that created it are identical to the current ones,
    then the texture will be left alone and not be recreated. If the output
    file does not exist, or has a different time stamp than the input file,
    or was created using different command line arguments, then the texture
    will be created and given the time stamp of the input file.

.. option:: --wrap <wrapmode>
            --swrap <wrapmode>, --twrap <wrapmode>

    Sets the default *wrap mode* for the texture, which determines the
    behavior when the texture is sampled outside the [0,1] range. Valid wrap
    modes are: `black`, `clamp`, `periodic`, `mirror`.  The default, if none
    is set, is `black`.  The `--wrap` option sets the wrap mode in both
    directions simultaneously, while the `--swrap` and `--twrap` may be used
    to set them individually in the *s* (horizontal) and *t* (vertical)
    diretions.
    
    Although this sets the default wrap mode for a texture, note that the
    wrap mode may have an override specified in the texture lookup at
    runtime.

.. option:: --resize

    Causes the highest-resolution level of the MIP-map to be a power-of-two
    resolution in each dimension (by rounding up the resolution of the input
    image).  There is no good reason to do this for the sake of OIIO's
    texture system, but some users may require it in order to create MIP-map
    images that are compatible with both OIIO and other texturing systems
    that require power-of-2 textures.

.. option:: --filter <name>

    By default, the resizing step that generates successive MIP levels uses
    a triangle filter to bilinearly combine pixels (for MIP levels with even
    number of pixels, this is also equivalent to a box filter, which merely
    averages groups of 4 adjacent pixels).  This is fast, but for source
    images with high frequency content, can result in aliasing or other
    artifacts in the lower-resolution MIP levels.
    
    The `--filter` option selects a high-quality filter to use when resizing
    to generate successive MIP levels.  Choices include `lanczos3` (our best
    recommendation for highest-quality filtering, a 3-lobe Lanczos filter),
    `box`, `triangle`, `catrom`, `blackman-harris`, `gaussian`, `mitchell`,
    `bspline`, `cubic`, `keys`, `simon`, `rifman`, `radial-lanczos3`,
    `disk`, `sinc`.
    
    If you select a filter with negative lobes (including `lanczos3`,
    `sinc`, `lanczos3`, `keys`, `simon`, `rifman`, or `catrom`), and your
    source image is an HDR image with very high contrast regions that
    include pixels with values >1, you may also wish to use the
    `--rangecompress` option to avoid ringing artifacts.

.. option:: --hicomp

    Perform highlight compensation.  When HDR input data with high-contrast
    highlights is turned into a MIP-mapped texture using a high-quality
    filter with negative lobes (such as `lanczos3`), objectionable ringing
    could appear near very high-contrast regions with pixel values >1. This
    option improves those areas by using range compression (transforming
    values from a linear to a logarithmic scale that greatly compresses the
    values > 1) prior to each image filtered-resize step, and then expanded
    back to a linear format after the resize, and also clamping resulting
    pixel values to be non-negative.  This can result in some loss of
    energy, but often this is a preferable alternative to ringing artifacts
    in your upper MIP levels.

.. option:: --sharpen <contrast>

    EXPERIMENTAL: USE AT YOUR OWN RISK!

    This option will run an additional sharpening filter when creating the
    successive MIP-map levels. It uses an unsharp mask (much like in Section
    :ref:`sec-iba-unsharpmask`) to emphasize high-frequency details to make
    features "pop" visually even at high MIP-map levels. The *contrast*
    controls the degree to which it does this. Probably a good value to
    enhance detail but not go overboard is 0.5 or even 0.25. A value of 1.0
    may make strage artifacts at high MIP-map levels. Also, if you
    simultaneously use `--filter unsharp-median`, a slightly different
    variant of unsharp masking will be used that employs a median filter to
    separate out the low-frequencies, this may tend to help emphasize small
    features while not over-emphasizing large edges.

.. option:: --nomipmap

    Causes the output to *not* be MIP-mapped, i.e., only will have the
    highest-resolution level.

.. option:: --nchannels <n>

    Sets the number of output channels.  If *n* is less than the number of
    channels in the input image, the extra channels will simply be ignored.
    If *n* is greater than the number of channels in the input image, the
    additional channels will be filled with 0 values.

.. option:: --chnames a,b,...

    Renames the channels of the output image.  All the channel names are
    concatenated together, separated by commas.  A "blank" entry will cause
    the channel to retain its previous value (for example, `--chnames ,,,A`
    will rename channel 3 to be "A" and leave channels 0-2 as they were.

.. option:: --checknan

    Checks every pixel of the input image to ensure that no `NaN` or `Inf`
    values are present.  If such non-finite pixel values are found, an error
    message will be printed and `maketx` will terminate without writing the
    output image (returning an error code).

.. option:: --fixnan streategy

    Repairs any pixels in the input image that contained `NaN` or `Inf`
    values (hereafter referred to collectively as "nonfinite"). If
    *strategy* is `black`, nonfinite values will be replaced with 0.  If
    *strategy* is `box3`, nonfinite values will be replaced by the average
    of all the finite values within a 3x3 region surrounding the pixel.

.. option:: --fullpixels

    Resets the "full" (or "display") pixel range to be the "data" range.
    This is used to deal with input images that appear, in their headers, to
    be crop windows or overscanned images, but you want to treat them as
    full 0--1 range images over just their defined pixel data.

.. option:: --Mcamera <...16 floats...>
            --Mscreen <...16 floats...>

    Sets the camera and screen matrices (sometimes called `Nl` and `NP`,
    respectively, by some renderers) in the texture file, overriding any
    such matrices that may be in the input image (and would ordinarily be
    copied to the output texture).

.. option:: --prman-metadata

    Causes metadata "PixarTextureFormat" to be set, which is useful if you
    intend to create an OpenEXR texture or environment map that can be used
    with PRMan as well as OIIO.

.. option:: --attrib <name> <value>

    Adds or replaces metadata with the given *name* to have the
    specified *value*.
    
    It will try to infer the type of the metadata from the value: if the
    value contains only numerals (with optional leading minus sign), it will
    be saved as `int` metadata; if it also contains a decimal point, it
    will be saved as `float` metadata; otherwise, it will be saved as
    a `string` metadata.
    
    For example, you could explicitly set the IPTC location metadata fields
    with::

        oiiotool --attrib "IPTC:City" "Berkeley" in.jpg out.jpg


.. option:: --sattrib <name> <value>

    Adds or replaces metadata with the given *name* to have the specified
    *value*, forcing it to be interpreted as a `string`. This is helpful if
    you want to set a `string` metadata to a value that the `--attrib`
    command would normally interpret as a number.

.. option:: --sansattrib

    When set, this edits the command line inserted in the "Software" and
    "ImageHistory" metadata to omit any verbose `--attrib` and `--sattrib`
    commands.

.. option:: --constant-color-detect

    Detects images in which all pixels are identical, and outputs the
    texture at a reduced resolution equal to the tile size, rather than
    filling endless tiles with the same constant color.  That is, by
    substituting a low-res texture for a high-res texture if it's a constant
    color, you could save a lot of save disk space, I/O, and texture cache
    size. It also sets the `"ImageDescription"` to contain a special message
    of the form `ConstantColor=[r,g,...]`.

.. option:: --monochrome-detect

    Detects multi-channel images in which all color components are
    identical, and outputs the texture as a single-channel image instead.
    That is, it changes RGB images that are gray into single-channel gray
    scale images.

.. option:: --opaque-detect

    Detects images that have a designated alpha channel for which the alpha
    value for all pixels is 1.0 (fully opaque), and omits the alpha channel
    from the output texture.  So, for example, an RGBA input texture where
    A=1 for all pixels will be output just as RGB.  The purpose is to save
    disk space, texture I/O bandwidth, and texturing time for those textures
    where alpha was present in the input, but clearly not necessary.

.. option:: --ignore-unassoc

    Ignore any header tags in the input images that indicate that the input
    has "unassociated" alpha.  When this option is used, color channels with
    unassociated alpha will not be automatically multiplied by alpha to turn
    them into associated alpha. This is also a good way to fix input images
    that really are associated alpha, but whose headers incorrectly indicate
    that they are unassociated alpha.

.. option:: --prman

    PRMan is will crash in strange ways if given textures that don't have
    its quirky set of tile sizes and other specific metadata.  If you want
    :program:`maketx` to generate textures that may be used with either
    OpenImageIO or PRMan, you should use the `--prman` option, which will
    set several options to make PRMan happy, overriding any contradictory
    settings on the command line or in the input texture.
    
    Specifically, this option sets the tile size (to 64x64 for 8 bit, 64x32
    for 16 bit integer, and 32x32 for float or `half` images), uses
    "separate" planar configuration (`--separate`), and sets PRMan-specific
    metadata (`--prman-metadata`).  It also outputs sint16 textures if
    uint16 is requested (because PRMan for some reason does not accept true
    uint16 textures), and in the case of TIFF files will omit the Exif
    directory block which will not be recognized by the older version of
    libtiff used by PRMan.
    
    OpenImageIO will happily accept textures that conform to PRMan's
    expectations, but not vice versa.  But OpenImageIO's TextureSystem has
    better performance with textures that use :program:`maketx`'s default
    settings rather than these oddball choices.  You have been warned!

.. option:: --oiio

    This sets several options that we have determined are the optimal values
    for OpenImageIO's TextureSystem, overriding any contradictory settings
    on the command line or in the input texture.
    
    Specifically, this is the equivalent to using

        `--separate --tile 64 64`

.. option:: --colorconvert <inspace> <outspace>

    Convert the color space of the input image from *inspace* to *tospace*.
    If OpenColorIO is installed and finds a valid configuration, it will be
    used for the color conversion.  If OCIO is not enabled (or cannot find a
    valid configuration, OIIO will at least be able to convert among linear,
    sRGB, and Rec709.

.. option:: --colorconfig <name>

    Explicitly specify a custom OpenColorIO configuration file. Without this,
    the default is to use the `$OCIO` environment variable as a guide for
    the location of the OpenColorIO configuration file.

.. option:: --unpremult

    When undergoing some color conversions, it is helpful to
    "un-premultiply" the alpha before converting color channels, and then
    re-multiplying by alpha.  Caveat emptor -- if you don't know exactly
    when to use this, you probably shouldn't be using it at all.


.. option:: --mipimage <filename>

    Specifies the name of an image file to use as a custom MIP-map level,
    instead of simply downsizing the last one.  This option may be used
    multiple times to specify multiple levels.  For example::

        maketx 256.tif --mipimage 128.tif --mipimage 64.tif -o out.tx

    This will make a texture with the first MIP level taken from `256.tif`,
    the second level from `128.tif`, the third from `64.tif`, and then
    subsequent levels will be the usual downsizings of `64.tif`.

.. option:: --envlatl

    Creates a latitude-longitude environment map, rather than an ordinary
    texture map.

.. option:: --lightprobe

    Creates a latitude-longitude environment map, but in contrast to
    `--envlatl`, the original input image is assumed to be formatted as a
    *light probe* image. (See http://www.pauldebevec.com/Probes/ for
    examples and an explanation of the geometric layout.)

.. option:: --bumpslopes

    For a single channel input image representing height (that you would
    ordinarily use for a bump or displacement), this produces a 6-channel
    texture that contains the first and second moments of bump slope, which
    can be used to implement certain bump-to-roughness techniques.
    The channel layout is as follows:

    +------+--------------+-------------------------------------------------------------------+
    |index | channel name | data at MIP level 0                                               |
    +------+--------------+-------------------------------------------------------------------+
    |0     | `b0_h`       | :math:`h`  (height)                                               |
    +------+--------------+-------------------------------------------------------------------+
    |1     | `b1_dhds`    | :math:`\partial h / \partial s`                                   |
    +------+--------------+-------------------------------------------------------------------+
    |2     | `b2_dhdt`    | :math:`\partial h / \partial t`                                   |
    +------+--------------+-------------------------------------------------------------------+
    |3     | `b3_dhds2`   | :math:`(\partial h / \partial s)^2`                               |
    +------+--------------+-------------------------------------------------------------------+
    |4     | `b4_dhdt2`   | :math:`(\partial h / \partial t)^2`                               |
    +------+--------------+-------------------------------------------------------------------+
    |5     | `b5_dhdsdt`  | :math:`(\partial h / \partial s) \cdot (\partial h / \partial t)` |
    +------+--------------+-------------------------------------------------------------------+

    (The strange channel names are to guarantee they are in alphabetical
    order, to prevent reordering by OpenEXR. And also note that the simple
    relationships between channels 1 & 2, and 3-6, is only for the highest-
    resolution level of the MIP-map, and will differ for the lower-res
    filtered versions, and those differences is what gives us the slope
    momets that we need.)

    A reference for explaining how this can be used is here:

    Christophe Hery, Michael Kass, and Junhi Ling. "Geometry into Shading."
    Technical Memo 14-04, Pixar Animation Studios, 2014.
    https://graphics.pixar.com/library/BumpRoughness

.. option:: --bumpformat <bformat>

    In conjunction with `--bumpslopes`, this option can specify the strategy
    for determining whether a 3-channel source image specifies a height map
    or a normal map. The value "height" indicates it is a height map (only
    the first channel will be used). The value "normal" indicates it is a
    normal map (all three channels will be used for x, y, z). The default
    value, "auto", indicates that it should be interpreted as a height map
    if and only if the R, G, B channel values are identical in all pixels,
    otherwise it will be interpreted as a 3-channel normal map.

.. option:: --uvslopes_scale <scalefactor>

   Used in conjunction with `--bumpslopes`, this computes derivatives for
   the bumpslopes data in UV space rather than in texel space, and divides
   them by a scale factor. If the value is 0 (default), this is disabled.
   For a nonzero value, it will be the scale factor. If you use this feature,
   a suggested value is 256.

   (This option was added for OpenImageIO 2.3.)

.. option:: --cdf
            --cdfsigma <SIGMA>
            --cdfbits <BITS>

   When `--cdf` is used, the output texture will write a Gaussian CDF and
   Inverse Gaussian CDF as per-channel metadata in the texture, which can be
   used by shaders to implement Histogram-Preserving Blending. This is only
   useful when the texture being created is written to an image format that
   supports arbitrary metadata (e.g. OpenEXR).

   When `--cdf` has been enabled, the additional options `--cdfsigma` may be
   used to specify the CDF sigma value (defaulting to 1.0/6.0), and
   `--cdfbits` specifies the number of bits to use for the size of the CDF
   table (defaulting to 8, which means 256 bins).
   
   References:

   * Histogram-Preserving Blending for Randomized Texture Tiling," JCGT 8(4),
     2019.
   
   * Heitz/Neyret, "High-Performance By-Example Noise using a
     Histogram-Preserving Blending Operator," ACM SIGGRAPH / Eurographics
     Symposium on High-Performance Graphics 2018.)

   * Benedikt Bitterli https://benedikt-bitterli.me/histogram-tiling/

   These options were first added in OpenImageIO 2.3.10.

.. option:: --handed <value>

   Adds a "handed" metadata to the resulting texture, which reveals the
   handedness of vector displacement maps or normal maps, when expressed in
   tangent space. Possible values are `left` or `right`.

   This option was first added in OpenImageIO 2.4.0.2.


.. sec-oiiotooltex:

`oiiotool`
=========

The :program:`oiiotool` utility (Chapter :ref:`chap-oiiotool`) is capable of
writing textures using the `-otex` option, lat-long environment maps using the
`-oenv` option, and bump/normal maps that include normal distribution moments.
Roughly speaking,

    `maketx` [*maketx-options*] *input* `-o` *output*

    `maketx -envlatl` [*maketx-options*] *input* `-o` *output*

    `maketx -bumpslopes` [*maketx-options*] *input* `-o` *output*

are equivalent to, respectively,

    `oiiotool` *input* [*oiiotool-options*] `-otex` *output*

    `oiiotool` *input* [*oiiotool-options*] `-oenv` *output*

    `oiiotool` *input* [*oiiotool-options*] `-obump` *output*

However, the notation for the various options are not identical between the
two programs, as will be explained by the remainder of this section.

The most important difference between :program:`oiiotool` and
:program:`maketx` is that :program:`oiiotool` can do so much more than
convert an existing input image to a texture -- literally any image
creation or manipulation you can do via :program:`oiiotool` may be output
directly as a full texture file using `-otex` (or as a lat-long environment
map using `-oenv`).

Note that it is vitally important that you use one of the texture output
commands (`-otex` or `-oenv`) when creating textures with :program:`oiiotool`
--- if you inadvertently forget and use an ordinary `-o`, you will end
up with an output image that is much less efficient to use as a texture.

Command line arguments useful when creating textures
----------------------------------------------------

As with any use of :program:`oiiotool`, you may use the following to control the
run generally:

.. option:: --help
            -v
            --runstats
            --threads <n>

    and as with any use of :program:`oiiotool`, you may use the following
    command-line options to control aspects of the any output files
    (including those from `-otex` and `-oenv`, as well as `-o`). Only brief
    descriptions are given below, please consult Chapter :ref:`oiiotool` for
    detailed explanations.

.. option:: -d <datatype>

    Specify the pixel data type (`UINT8`, `uint16`, `half`, `float`, etc.)
    if you wish to override the default of writing the same data type as the
    first input file read.

.. option:: --tile <x> <y>

    Explicitly override the tile size (though you are strongly urged to use
    the default, and not use this command).

.. option:: --compression <method>

    Explicitly override the default compression methods when writing the
    texture file.

.. option:: --ch <channellist>

    Shuffle, add, delete, or rename channels (see :ref:`sec-oiiotool-ch`).

.. option:: --chnames a,b,...

    Renames the channels of the output image.

.. option:: --fixnan <stretegy>

    Repairs any pixels in the input image that contained `NaN` or `Inf`
    values (if the *strategy* is `box3` or `black`), or to simply abort with
    an error message (if the *strategy* is `error`).

.. option:: --fullpixels

    Resets the "full" (or "display") pixel range to be the "data" range.

.. option:: --planarconfig separate

    Forces "separate" (e.g., RRR...GGG...BBB) packing of channels in the
    output texture.  This is almost always a bad choice, unless you know
    that the texture file must be readable by PRMan, in which case it is
    required.

.. option:: --attrib <name> <value>

    :program:`oiiotool`'s `--attrib` command may be used to set attributes
    in the metadata of the output texture.

.. option:: --attrib:type=matrix worldtocam <...16 comma-separated floats...>
            --attrib:type=matrix screentocam <...16 comma-separated floats...>

    Set/override the camera and screen matrices.


Optional arguments to `-otex`, `-oenv`, `-obump`
------------------------------------------------

As with many :program:`oiiotool` commands, the `-otex` and `-oenv` may
have various optional arguments appended, in the form `:name=value`
(see Section :ref:`sec-oiiotooloptionalargs`).

Optional arguments supported by `-otex` and `-oenv` include all the same
options as `-o` (Section :ref:`sec-oiiotool-o`) and also the following
(explanations are brief, please consult Section :ref:`sec-maketx` for more
detailed explanations of each, for the corresponding :program:`maketx`
option):


=======================     ============================================
Appended Option             `maketx` equivalent
=======================     ============================================
`wrap=` *string*            `--wrap`
`swrap=` *string*           `--swrap`
`twrap=` *string*           `--twrap`
`resize=1`                  `--resize`
`nomipmap=1`                `--nomipmap`
`updatemode=1`              `-u`
`monochrome_detect=1`       `--monochrome-detect`
`opaque_detect=1`           `--opaque-detect`
`unpremult=1`               `--unpremult`
`incolorspace=` *name*      `--incolorspace`
`outcolorspace=` *name*     `--outcolorspace`
`hilightcomp=1`             `--hicomp`
`sharpen=` *float*          `--sharpen`
`filter=` *string*          `--filter`
`bumpformat=` *string*      `--bumpformat`
`uvslopes_scale=` *float*   `--uvslopes-scale`
`prman_metadata=1`          `--prman`
`prman_options=1`           `--prman-metadata`
=======================     ============================================


Examples
--------

.. code-block::

    oiiotool in.tif -otex out.tx
    
    oiiotool in.jpg --colorconvert sRGB linear -d uint16 -otex out.tx
    
    oiiotool --pattern:checker 512x512 3 -d uint8 -otex:wrap=periodic checker.tx
    
    oiiotool in.exr -otex:hilightcomp=1:sharpen=0.5 out.exr



