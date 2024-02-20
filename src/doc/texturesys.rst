..
  Copyright Contributors to the OpenImageIO project.
  SPDX-License-Identifier: CC-BY-4.0


.. _chap-texturesystem:

Texture Access: TextureSystem
#############################

.. |br| raw:: html

    <br>


.. _sec-texturesys-intro:

Texture System Introduction and Theory of Operation
==================================================================

Coming soon.
FIXME

.. _sec-texturesys-helperclasses:

Helper Classes
==================================================================

Imath
-------------------------------------------------

The texture functionality of OpenImageIO uses the excellent open source
``Imath`` types when it requires 3D vectors and
transformation matrixes.  Specifically, we use ``Imath::V3f`` for 3D
positions and directions, and ``Imath::M44f`` for 4x4 transformation
matrices.  To use these yourself, we recommend that you::

    // If using Imath 3.x:
    #include <Imath/ImathVec.h>
    #include <Imath/ImathMatrix.h>

    // OR, if using OpenEXR 2.x, before Imath split off:
    #include <OpenEXR/ImathVec.h>
    #include <OpenEXR/ImathMatrix.h>

Please refer to the ``Imath`` documentation and header
files for more complete information about use of these types in your own
application.  However, note that you are not strictly required to use these
classes in your application --- ``Imath::V3f`` has a memory layout identical
to ``float[3]`` and ``Imath::M44f`` has a memory layout identical to
``float[16]``, so as long as your own internal vectors and matrices have the
same memory layout, it's ok to just cast pointers to them when passing as
arguments to TextureSystem methods.


.. _sec-textureopt:

TextureOpt
-------------------------------------------------

TextureOpt is a structure that holds many options controlling single-point
texture lookups.  Because each texture lookup API call takes a reference to
a TextureOpt, the call signatures remain uncluttered rather than having an
ever-growing list of parameters, most of which will never vary from their
defaults.  Here is a brief description of the data members of a TextureOpt
structure:

- `int firstchannel` :
  The beginning channel for the lookup.  For example, to retrieve just the
  blue channel, you should have `firstchannel` = 2 while passing `nchannels`
  = 1 to the appropriate texture function.

- `int subimage, ustring subimagename` :
  Specifies the subimage or face within the file to use for the texture
  lookup. If `subimagename` is set (it defaults to the empty string), it
  will try to use the subimage that had a matching metadata
  `"oiio:subimagename"`, otherwise the integer `subimage` will be used
  (which defaults to 0, i.e., the first/default subimage).  Nonzero subimage
  indices only make sense for a texture file that supports subimages (like
  TIFF or multi-part OpenEXR) or separate images per face (such as Ptex).
  This will be ignored if the file does not have multiple subimages or
  separate per-face textures.

- `Wrap swrap, twrap` :
  Specify the *wrap mode* for 2D texture lookups (and 3D volume texture
  lookups, using the additional `rwrap` field).  These fields are ignored
  for shadow and environment lookups. These specify what happens when
  texture coordinates are found to be outside the usual [0,1] range over
  which the texture is defined. `Wrap` is an enumerated type that may take
  on any of the following values:

    - `WrapBlack` : The texture is black outside the [0,1] range.

    - `WrapClamp` : The texture coordinates will be clamped to [0,1], i.e.,
      the value outside [0,1] will be the same as the color at the nearest
      point on the border.

    - `WrapPeriodic` : The texture is periodic, i.e., wraps back to 0 after
      going past 1.

    - `WrapMirror` : The texture presents a mirror image at the edges, i.e.,
      the coordinates go from 0 to 1, then back down to 0, then back up to
      1, etc.

    - `WrapDefault` : Use whatever wrap might be specified in the texture
      file itself, or some other suitable default (caveat emptor).

  The wrap mode does not need to be identical in the `s` and `t`
  directions.

- `float swidth, twidth` :
  For each direction, gives a multiplier for the derivatives.  Note that
  a width of 0 indicates a point sampled lookup (assuming that blur is
  also zero).  The default width is 1, indicating that the derivatives
  should guide the amount of blur applied to the texture filtering (not
  counting any additional *blur* specified).

- `float sblur, tblur` :
  For each direction, specifies an additional amount of pre-blur to apply
  to the texture (*after* derivatives are taken into account),
  expressed as a portion of the width of the texture.  In other words,
  blur = 0.1 means that the texture lookup should act as if the texture
  was pre-blurred with a filter kernel with a width 1/10 the size of the
  full image.  The default blur amount is 0, indicating a sharp texture
  lookup.

- `float fill` :
  Specifies the value that will be used for any color channels that are
  requested but not found in the file.  For example, if you perform a
  4-channel lookup on a 3-channel texture, the last channel will get the
  fill value.  (Note: this behavior is affected by the `"gray_to_rgb"`
  attribute described in the Section about TextureSystem attributes.

- `const float* missingcolor` :
  If not NULL, indicates that a missing or broken texture should *not*
  be treated as an error, but rather will simply return the supplied color
  as the texture lookup color and `texture()` will return `true`. If the
  `missingcolor` field is left at its default (a NULL pointer), a
  missing or broken texture will be treated as an error and `texture()`
  will return `false`. Note: When not NULL, the data must point to
  `nchannels` contiguous floats.

..
  - `float bias` :
  For shadow map lookups only, this gives the "shadow bias" amount.

..
  - `int samples` :
  For shadow map lookups only, the number of samples to use for the lookup.

- `Wrap rwrap, float rblur, rwidth` :
  Specifies wrap, blur, and width for the third component of 3D volume
  texture lookups.  These are not used for 2D texture or environment
  lookups.

- `MipMode mipmode` :
  Determines if/how MIP-maps are used:

    - `MipModeDefault`   : The default high-quality lookup (same as Aniso).

    - `MipModeNoMIP`     : Just use highest-res image, no MIP mapping

    - `MipModeOneLevel`  : Use just one mipmap level

    - `MipModeTrilinear` : Use two MIPmap levels (trilinear)

    - `MipModeAniso`     : Use two MIPmap levels w/ anisotropic

- `InterpMode interpmode` :
  Determines how we sample within a mipmap level:

    - `InterpClosest`      : Force closest texel.

    - `InterpBilinear`     : Force bilinear lookup within a mip level.

    - `InterpBicubic`      : Force cubic lookup within a mip level.

    - `InterpSmartBicubic` : Bicubic when maxifying, else bilinear (default).

- `int anisotropic` :
  Maximum anisotropic ratio (default: 32).

- `bool conservative_filter` :
  When true (the default), filters conservatively in a way that chooses to
  sometimes over-blur rather than alias.





.. _sec-texturesys-api:

TextureSystem API
==================================================================

.. doxygenclass:: OIIO::TextureSystem
    :members:






.. _sec-texturesys-udim:

UDIM texture atlases
====================

Texture lookups
---------------

The `texture()` call supports virtual filenames that expand per lookup for
UDIM tiled texture atlases. The substitutions will occur if the texture
filename initially passed to `texture()` does not exist as a concrete file
and contains one or more of the following substrings:

========== ======================== =================================
Pattern    Numbering scheme         Example expansion if u=0.5, v=2.5
========== ======================== =================================
`<UDIM>`   1001 + utile + vtile*10  `1021`
`<u>`      utile                    `u0`
`<v>`      vtile                    `v2`
`<U>`      utile + 1                `u1`
`<V>`      vtile + 1                `v3`
`<uvtile>` equivalent to `<u>_<v>`  `u0_v2`
`<UVTILE>` equivalent to `<U>_<V>`  `u1_v3`
`_u##v##`  utile, vtile             `_u00v02`
`%(UDIM)d` synonym for `<UDIM>`     `1021`
========== ======================== =================================

where the tile numbers are derived from the input u,v texture
coordinates as follows::

    // Each unit square of texture is a different tile
    utile = max (0, int(u));
    vtile = max (0, int(v));
    // Re-adjust the texture coordinates to the offsets within the tile
    u = u - utile;
    v = v - vtile;

Example::

    ustring filename ("paint.<UDIM>.tif");
    float s = 1.4, t = 3.8;
    texsys->texture (filename, s, t, ...);

will retrieve from file :file:`paint.1032.tif` at coordinates (0.4,0.8).


Handles of udim files
---------------------

Calls to `get_texture_handle()`, when passing a UDIM pattern filename, will
always succeed. But without knowing a specific u and v, it has no way to
know that the concrete file you will eventually ask for would not succeed,
so this handle is for the overall
"virtual" texture atlas.

You can retrieve the handle of a specific "tile" of the UDIM set by using

.. cpp:function:: TextureHandle* resolve_udim(ustring udimpattern, float s, float t)
    TextureHandle* resolve_udim(TextureHandle* udimfile, Perthread* thread_info, float s, float t)

    Note: these will return `nullptr` if the UDIM tile for those
    coordinates is unpopulated.


Note also that the `is_udim()` method can be used to ask whether a filename
or handle corresponds to a UDIM pattern (the whole set of atlas tiles):

.. cpp:function:: bool is_udim(ustring filename)
    bool is_udim(TextureHandle* udimfile)


Retrieving metadata from UDIM sets and tiles
--------------------------------------------

Calls to `get_texture_info()` on UDIM file pattern will succeed if the
metadata is found and has the same value in all of the populated "tiles" of
a UDIM. If not all populated tile files have the same value for that
attribute, the call will fail.

If you want to know the metadata at a specific texture coordinate, you can
use a combination of `resolve_udim()` to find the handle for the corresponding
concrete texture file for that "tile," and then `get_texture_info()` to
retrieve the metadata for the concrete file.


Full inventory of a UDIM set
----------------------------

You can get the range in u and v of the UDIM texture atlas, and the list of
all of the concrete filenames of the corresponding tiles with this method:

.. cpp:function:: void inventory_udim(ustring udimpattern, std::vector<ustring>& filenames, int& nutiles, int& nvtiles)
   void inventory_udim(TextureHandle* udimfile, Perthread* thread_info, std::vector<ustring>& filenames, int& nutiles, int& nvtiles)

The indexing scheme is that `filenames[u + v * nvtiles]` is the name of the
tile with integer indices `(u,v)`, where 0 is the first index of each row or
column.

The combination of `inventory_udim()` and `get_texture_handle()` of the listed
filenames can be used to generate the corresponding handles for each UDIM
tile.



.. _sec-texturesys-api-batched:

Batched Texture Lookups
==================================================================

On CPU architectures with SIMD processing, texturing entire batches of
samples at once may provide a large speedup compared to texturing each
sample point individually. The batch size is fixed (for any build of
OpenImageIO) and may be accessed with the following constant:


.. doxygenvariable:: OIIO::Tex::BatchWidth

.. doxygentypedef:: OIIO::Tex::FloatWide

.. doxygentypedef:: OIIO::Tex::IntWide


All of the batched calls take a *run mask*, which describes which subset of
"lanes" should be computed by the batched lookup:

.. doxygentypedef:: RunMask

.. cpp:enumerator:: RunMaskOn

    The defined constant `RunMaskOn` contains the value with all bits
    `0..BatchWidth-1` set to 1.



Batched Options
---------------

TextureOptBatch is a structure that holds the options for doing an entire
batch of lookups from the same texture at once. The members of
TextureOptBatch correspond to the similarly named members of the
single-point TextureOpt, so we refer you to Section :ref:`sec-textureopt`
for detailed explanations, and this section will only explain the
differences between batched and single-point options. Members include:


- `int firstchannel` :
- `int subimage, ustring subimagename` :
- `Wrap swrap, twrap, rwrap` :
- `float fill` :
- `const float* missingcolor` :
- `MipMode mipmode` :
- `InterpMode interpmode` :
- `int anisotropic` :
- `bool conservative_filter` :

    These fields are all scalars --- a single value for each TextureOptBatch
    --- which means that the value of these options must be the same for
    every texture sample point within a batch. If you have a number of
    texture lookups to perform for the same texture, but they have (for
    example) differing wrap modes or subimages from point to point, then you
    must split them into separate batch calls.

- `float sblur[Tex::BatchWidth]` :
- `float tblur[Tex::BatchWidth]` :
- `float rblur[Tex::BatchWidth]` :

    These arrays hold the `s`, and `t` blur amounts, for each sample in the
    batch, respectively. (And the `r` blur amount, used only for volumetric
    `texture3d()` lookups.)

- `float swidth[Tex::BatchWidth]` :
- `float twidth[Tex::BatchWidth]` :
- `float rwidth[Tex::BatchWidth]` :

    These arrays hold the `s`, and `t` filtering width multiplier for
    derivatives, for each sample in the batch, respectively. (And the `r`
    multiplier, used only for volumetric `texture3d()` lookups.)


Batched Texture Lookup Calls
----------------------------

.. cpp:function::
    bool TextureSystem::texture (ustring filename, TextureOptBatch &options, Tex::RunMask mask, const float *s, const float *t, const float *dsdx, const float *dtdx, const float *dsdy, const float *dtdy, int nchannels, float *result, float *dresultds=nullptr, float *dresultdt=nullptr)
    bool TextureSystem::texture (TextureHandle *texture_handle, Perthread *thread_info, TextureOptBatch &options, Tex::RunMask mask, const float *s, const float *t, const float *dsdx, const float *dtdx, const float *dsdy, const float *dtdy, int nchannels, float *result, float *dresultds=nullptr, float *dresultdt=nullptr)

    Perform filtered 2D texture lookups on a batch of positions from the
    same texture, all at once.  The parameters `s`, `t`, `dsdx`, `dtdx`, and
    `dsdy`, `dtdy` are each a pointer to `[BatchWidth]` values.  The `mask`
    determines which of those array elements to actually compute.

    The various results are arranged as arrays that behave as if they were
    declared::

        float result[channels][BatchWidth]

    In other words, all the batch values for channel 0 are adjacent,
    followed by all the batch values for channel 1, etc. (This is "SOA"
    order.)

    This function returns `true` upon success, or `false` if the file was
    not found or could not be opened by any available ImageIO plugin.


.. cpp:function::
    bool texture3d (ustring filename, TextureOptBatch &options, Tex::RunMask mask, const float *P, const float *dPdx, const float *dPdy, const float *dPdz, int nchannels, float *result, float *dresultds=nullptr, float *dresultdt=nullptr,float *dresultdr=nullptr)
    bool texture3d (TextureHandle *texture_handle, Perthread *thread_info, TextureOptBatch &options, Tex::RunMask mask, const float *P, const float *dPdx, const float *dPdy, const float *dPdz, int nchannels, float *result, float *dresultds=nullptr, float *dresultdt=nullptr, float *dresultdr=nullptr)

    Perform filtered 3D volumetric texture lookups on a batch of positions
    from the same texture, all at once. The "point-like" parameters `P`,
    `dPdx`, `dPdy`, and `dPdz` are each a pointers to arrays of `float
    value[3][BatchWidth]`. That is, each one points to all the *x* values for
    the batch, immediately followed by all the *y* values, followed by the
    *z* values.
    
    The various results arrays are also arranged as arrays that behave as if
    they were declared `float result[channels][BatchWidth]`, where all the
    batch values for channel 0 are adjacent, followed by all the batch
    values for channel 1, etc.
    
    This function returns `true` upon success, or `false` if the file was
    not found or could not be opened by any available ImageIO plugin.


.. cpp:function::
    bool environment (ustring filename, TextureOptBatch &options, Tex::RunMask mask, const float *R, const float *dRdx, const float *dRdy, int nchannels, float *result, float *dresultds=nullptr, float *dresultdt=nullptr)
    bool environment (TextureHandle *texture_handle, Perthread *thread_info, TextureOptBatch &options, Tex::RunMask mask, const float *R, const float *dRdx, const float *dRdy, int nchannels, float *result, float *dresultds=nullptr, float *dresultdt=nullptr)

    Perform filtered directional environment map lookups on a batch of
    positions from the same texture, all at once. The "point-like"
    parameters `R`, `dRdx`, and `dRdy` are each a pointers to arrays of
    `float value[3][BatchWidth]`. That is, each one points to all the *x*
    values for the batch, immediately followed by all the *y* values,
    followed by the *z* values.
    
    Perform filtered directional environment map lookups on a collection of
    directions all at once, which may be much more efficient than repeatedly
    calling the single-point version of `environment()`.  The parameters
    `R`, `dRdx`, and `dRdy` are now VaryingRef's that may refer to either a
    single or an array of values, as are many the fields in the `options`.
    
    The various results arrays are also arranged as arrays that behave as if
    they were declared `float result[channels][BatchWidth]`, where all the
    batch values for channel 0 are adjacent, followed by all the batch
    values for channel 1, etc.
    
    This function returns `true` upon success, or `false` if the file was
    not found or could not be opened by any available ImageIO plugin.

