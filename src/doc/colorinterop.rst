..
  Copyright Contributors to the OpenImageIO project.
  SPDX-License-Identifier: CC-BY-4.0


.. _chap-colorinterop:

Color interop IDs and CICP
##########################

This chapter describes how OpenImageIO identifies color spaces using
*color interop IDs* and *CICP* codes, and how individual file format
plugins read and write that information today.

A **color interop ID** is a short, stable text token that names a color
space in a way meant to be portable across applications and vendors,
independent of any particular color management configuration. The tokens
used by OpenImageIO come from the Academy Software Foundation's `Color
Interop Forum <https://github.com/AcademySoftwareFoundation/ColorInterop>`_,
which publishes recommendations for identifying color spaces consistently
across tools and pipelines.

**CICP** ("Coding-independent code points") is a numeric encoding of color
space information defined by `ITU-T H.273
<https://www.itu.int/rec/T-REC-H.273>`_. It is a 4-tuple of small integers
--- color primaries, transfer characteristics, matrix coefficients, and a
full/narrow range flag --- and is the mechanism several image and video
container formats use natively to record color space metadata.

See also the general description of the `"oiio:ColorSpace"` and `"CICP"`
attributes in :ref:`sec-metadata-color`.



Color interop IDs in OpenImageIO
=================================

`ColorConfig` provides three methods for moving between color space names,
color interop IDs, and CICP codes:

.. code-block::

    // Find color interop ID for the given colorspace name (color space,
    // alias, or role). Returns "" if not found.
    string_view get_color_interop_id(string_view colorspace) const;

    // Find color interop ID corresponding to the CICP code.
    // Returns "" if not found.
    string_view get_color_interop_id(const int cicp[4]) const;

    // Find CICP code corresponding to the colorspace.
    // Returns an empty span if not found.
    cspan<int> get_cicp(string_view colorspace) const;

`get_color_interop_id(string_view)` first asks the active OCIO config for
its own interop ID for the resolved color space (this requires OCIO >=
2.5, which added `ColorSpace::getInteropID()`), and only if that isn't
available does it fall back to a built-in table of color interop IDs
maintained inside OpenImageIO. This means that with a sufficiently recent
OCIO config that annotates its color spaces with interop IDs, the config's
own answer takes precedence over OpenImageIO's built-in table.

`get_color_interop_id(const int cicp[4])` and `get_cicp(string_view)` only
consult the built-in table; they do not consult the OCIO config.

Color space name lookups are case-insensitive, and the name may be any
color space, alias, or role that OpenImageIO can relate to one of the
built-in interop ID tokens by name, alias, or its cheap color space
classification. `get_color_interop_id()` is deliberately an inexpensive
lookup: it never probes transforms or builds color processors. The full
derivation (which can additionally identify a space by comparing its
transform values against the built-in registry identities, or generate a
config-local ID) runs internally at write-planning time when a file is
written.


Built-in color interop ID / CICP table
=======================================

OpenImageIO ships a built-in table pairing color interop ID tokens with
their CICP correspondence, where one exists. It is transcribed below,
faithful to the order and content of the table in OpenImageIO's source
(`color_ocio.cpp`). Scene-referred interop IDs are listed first, and
display-referred ones follow. Some interop IDs describe color spaces (or
non-color-space states such as "data" and "unknown") that cannot be
expressed as CICP at all, and have no CICP entry.

CICP columns give the numeric code for each of primaries, transfer
characteristics, and matrix coefficients (the range flag is always "Full"
for every table entry that has a CICP mapping).

.. list-table:: Scene-referred color interop IDs
   :widths: 22 14 20 20 24
   :header-rows: 1

   * - Color interop ID
     - Primaries
     - Transfer
     - Matrix
     - Notes
   * - ``lin_ap1_scene``
     - —
     - —
     - —
     - No CICP mapping.
   * - ``lin_ap0_scene``
     - —
     - —
     - —
     - No CICP mapping. ACES2065-1 (AP0), used for the ACES Container (see
       below).
   * - ``lin_rec709_scene``
     - Rec709 (1)
     - Linear (8)
     - BT709 (1)
     -
   * - ``lin_p3d65_scene``
     - P3D65 (12)
     - Linear (8)
     - BT709 (1)
     -
   * - ``lin_rec2020_scene``
     - Rec2020 (9)
     - Linear (8)
     - Rec2020_CL (10)
     -
   * - ``lin_adobergb_scene``
     - —
     - —
     - —
     - No CICP mapping (no CICP code for Adobe RGB primaries).
   * - ``lin_ciexyzd65_scene``
     - XYZD65 (10)
     - Linear (8)
     - Unspecified (2)
     -
   * - ``srgb_rec709_scene``
     - Rec709 (1)
     - sRGB (13)
     - BT709 (1)
     -
   * - ``g22_rec709_scene``
     - Rec709 (1)
     - Gamma22 (4)
     - BT709 (1)
     -
   * - ``g18_rec709_scene``
     - —
     - —
     - —
     - No CICP mapping.
   * - ``srgb_ap1_scene``
     - —
     - —
     - —
     - No CICP mapping.
   * - ``g22_ap1_scene``
     - —
     - —
     - —
     - No CICP mapping.
   * - ``srgb_p3d65_scene``
     - P3D65 (12)
     - sRGB (13)
     - BT709 (1)
     -
   * - ``g22_adobergb_scene``
     - —
     - —
     - —
     - No CICP mapping (no CICP code for Adobe RGB primaries).
   * - ``data``
     - —
     - —
     - —
     - Not a color space; marks pixel data that is not meant to be color
       managed.
   * - ``unknown``
     - —
     - —
     - —
     - Marks pixel data whose color space is not known. On write, OpenImageIO
       never *derives* a bare ``unknown``: a user's explicitly-set
       ``colorInteropID`` attribute of ``unknown`` is written verbatim (the
       author's bytes are never rewritten), a config that itself declares a
       space unknown (an ``interop_id`` of ``unknown``, or a color space
       *named* ``unknown`` with no contradicting ``interop_id``) derives the
       marker ``ocio:unknown``, and an undeterminable color space simply
       omits the attribute.

.. list-table:: Display-referred color interop IDs
   :widths: 22 14 20 20 24
   :header-rows: 1

   * - Color interop ID
     - Primaries
     - Transfer
     - Matrix
     - Notes
   * - ``srgb_rec709_display``
     - Rec709 (1)
     - sRGB (13)
     - BT709 (1)
     -
   * - ``g24_rec709_display``
     - Rec709 (1)
     - BT709 (1)
     - BT709 (1)
     -
   * - ``srgb_p3d65_display``
     - P3D65 (12)
     - sRGB (13)
     - BT709 (1)
     -
   * - ``srgbe_p3d65_display``
     - P3D65 (12)
     - sRGB (13)
     - BT709 (1)
     -
   * - ``pq_p3d65_display``
     - P3D65 (12)
     - PQ (16)
     - Rec2020_NCL (9)
     -
   * - ``pq_rec2020_display``
     - Rec2020 (9)
     - PQ (16)
     - Rec2020_NCL (9)
     -
   * - ``hlg_rec2020_display``
     - Rec2020 (9)
     - HLG (18)
     - Rec2020_NCL (9)
     -
   * - ``g22_rec709_display``
     - —
     - —
     - —
     - No CICP mapping, by deliberate choice: OpenImageIO's source notes
       that this is left unmapped "to keep previous behavior unchanged, as
       Gamma 2.2 display is more likely meant to be written as sRGB"; on
       read, the scene-referred interop ID is used instead.
   * - ``g22_adobergb_display``
     - —
     - —
     - —
     - No CICP mapping (no CICP code for Adobe RGB primaries).
   * - ``g26_p3d65_display``
     - P3D65 (12)
     - Gamma26 (17)
     - BT709 (1)
     -
   * - ``g26_xyzd65_display``
     - XYZD65 (10)
     - Gamma26 (17)
     - Unspecified (2)
     -
   * - ``pq_xyzd65_display``
     - XYZD65 (10)
     - PQ (16)
     - Unspecified (2)
     -

.. note::

    `get_color_interop_id(const int cicp[4])` only matches on the
    *primaries* and *transfer* fields of the CICP tuple --- matrix
    coefficients and the range flag are not part of the lookup key, and it
    returns the first table entry (in the order shown above) whose
    primaries and transfer match. Several distinct interop IDs share the
    same (primaries, transfer) pair by design or coincidence:

    - ``srgb_rec709_scene`` and ``srgb_rec709_display`` both correspond to
      (Rec709, sRGB); the scene-referred entry, being listed first, is
      what a CICP-to-interop-ID lookup returns.
    - ``srgb_p3d65_scene``, ``srgb_p3d65_display``, and
      ``srgbe_p3d65_display`` all correspond to (P3D65, sRGB); again the
      scene-referred entry is returned.

    This is intentional: scene-referred entries are ordered first in the
    table specifically "so they are the default in automatic conversion
    from CICP to interop ID."


Format plugin support
======================

The following describes color interop ID and CICP handling as currently
implemented in OpenImageIO's format plugins.

OpenEXR
-------

The OpenEXR plugin reads and writes a string attribute literally named
`colorInteropID` (not `oiio:ColorInteropID`).

- On read, if the file has an `acesImageContainerFlag` attribute set to 1,
  `"oiio:ColorSpace"` is set to `lin_ap0_scene`. Otherwise, if a
  `colorInteropID` attribute is present, its value is used to set
  `"oiio:ColorSpace"`.
- On write, if no `colorInteropID` attribute is already present on the
  spec, one is derived automatically from `"oiio:ColorSpace"` by the
  write-planning derivation cascade (declared config `interop_id`,
  registry identity match, built-in table, config-local ID) and attached
  to the file (only if a matching interop ID is found).
- The `openexr:ACESContainerPolicy` output configuration attribute (`none`,
  `strict`, or `relaxed`) can additionally force a file into ACES
  Container form: it sets the ACES AP0 `chromaticities`, sets
  `colorInteropID` to `lin_ap0_scene`, and (in `strict` mode, only if the
  spec already qualifies, or in `relaxed` mode regardless) sets
  `acesImageContainerFlag` to 1. See :ref:`sec-bundledplugins-openexr` for
  the full configuration attribute reference.

OpenEXR does not natively support CICP; there is no `"CICP"` attribute
handling in the OpenEXR plugin.

PNG
---

The PNG plugin's `feature("cicp")` query reports true when built against a
libpng new enough to support the `cICP` chunk (gated behind the compile-time
`PNG_cICP_SUPPORTED` macro). However, this only reflects libpng's
capability: the plugin does not currently read or write the `cICP` chunk,
and has no `"CICP"` attribute handling at all.

HEIF/HEIC/AVIF
--------------

The HEIF plugin's CICP support requires libheif >= 1.9.0 (gated behind
`LIBHEIF_HAVE_VERSION(1, 9, 0)`; `feature("cicp")` reflects this).

- On read, the NCLX color profile is queried from the image handle (not
  the decoded image, since the two can differ) via libheif's C API. If an
  NCLX profile is present and not entirely "unspecified", its four values
  are stored as `"CICP"`, and `"oiio:ColorSpace"` is set from
  `ColorConfig::get_color_interop_id()` if a match is found.
- On write, an explicit `"CICP"` attribute takes priority; otherwise CICP
  is derived from `"oiio:ColorSpace"` via `ColorConfig::get_cicp()`. If
  present, the values are used to populate an NCLX color profile attached
  to the encoded image.

JPEG XL
-------

The JPEG XL plugin (`jpegxl.imageio`) supports only a subset of CICP,
constrained by what `JxlColorEncoding` can represent; there is no
compile-time version gate for this support.

- On read, CICP is derived from the decoded `JxlColorEncoding` only when
  the color encoding does not use custom primaries, a custom white point,
  or a gamma transfer function. The matrix coefficients value is always
  recorded as 0 (RGB) and the range flag is always recorded as full range,
  since JPEG XL's internal representation doesn't carry those independent
  of the primaries/transfer function. As with the other formats,
  `"oiio:ColorSpace"` is then set via `ColorConfig::get_color_interop_id()`
  if a match is found.
- On write, an explicit `"CICP"` attribute takes priority; otherwise CICP
  is derived from `"oiio:ColorSpace"` via `ColorConfig::get_cicp()` (only
  if color space metadata wasn't already written some other way). Only
  primaries in {sRGB, BT.2100, P3} and transfer functions in {BT.709,
  unknown, linear, sRGB, PQ, DCI, HLG} are supported for writing; if the
  CICP tuple names an unsupported primary or transfer function (such as a
  gamma or custom code point), no color encoding is set on the JPEG XL
  output at all.

There is no `colorInteropID` string-attribute round-trip in the PNG,
HEIF, or JPEG XL plugins; all three only work in terms of `"CICP"` plus
`ColorConfig`'s interop ID / CICP conversion.


Color policy
============

The rules that turn file color metadata into an `"oiio:ColorSpace"` on
read, and decide which color signals a writer emits, are collectively the
*color policy*. The policy is not hard-wired: it is driven by the ambient
OCIO configuration and, layered on top, by explicit settings.

The ambient config drives I/O
-----------------------------

Reads and writes consult the ambient OCIO configuration -- the one named
by the ``$OCIO`` environment variable (or otherwise the current config).
A configuration author can therefore make policy decisions *in the config
itself*, and every OIIO reader and writer honors them without any change
to the I/O call.

If OpenImageIO is built without OCIO support, or no configuration is
active, there is nothing to consult and behavior is exactly as it has
always been -- the built-in defaults, unchanged. This is the
no-color-management opt-out: with no config, no config-declared policy
can apply.

Config-declared policy
----------------------

An OCIO configuration already lets its author attach arbitrary custom
key/value pairs to each file rule. OCIO round-trips those keys byte-for-
byte and other applications ignore them, so they are a ready-made,
author-owned channel for a config to declare its own color policy. OIIO
reads the keys whose names begin with ``oiio:`` and applies them.

Policy is carried on a *profile* rule: a file rule whose regex is ``$^``
(so it never matches a real file -- it exists only to hold policy) and
whose custom keys are the policy settings. The reserved profile
``oiio:default`` is the config author's hook for adjusting OIIO's own
defaults:

.. code-block:: yaml

   file_rules:
     - !<Rule>
       name: oiio:default
       colorspace: raw_data
       regex: "$^"
       custom:
         oiio:colorpolicy:read:cicp_state: scene
         oiio:colorpolicy:write:cicp: never
     - !<Rule> {name: Default, colorspace: raw_data}

With this config active, a state-ambiguous CICP tuple read from a file
resolves to its scene-referred interpretation instead of the default
display-referred one, and writers suppress the CICP signal they would
otherwise emit -- with no OIIO attribute set anywhere. A config that
declares no ``oiio:`` keys changes nothing.

Naming
------

Every profile name is ``oiio:``-prefixed; there is no bare ``default``
namespace (which also avoids colliding with OCIO's own catch-all
``Default`` rule). ``oiio:default`` adjusts OIIO's shipped defaults.
Named profiles follow the pattern ``oiio:<app>:<context>`` -- for
example ``oiio:blender:textures`` -- so an application declares its own
policy bundles without claiming the bare ``oiio:`` prefix, which is
reserved for the standard. An unknown ``oiio:`` key, or an unrecognized
value for a known key, is ignored (warn-once); a malformed configuration
never breaks resolution -- policy reading is best-effort and always falls
through to the built-in default.

Precedence
----------

Several sources may express an opinion about the same policy key. They
resolve into one snapshot per call, weakest to strongest:

.. list-table:: Color-policy precedence, weakest to strongest
   :widths: 6 36 58
   :header-rows: 1

   * - Layer
     - Name
     - Authored by
   * - 1
     - Built-in defaults
     - OpenImageIO, at ship time.
   * - 2
     - The active config's ``oiio:default`` profile
     - Config author.
   * - 4
     - Global individual keys (``OIIO::attribute``)
     - User / application, for the session.
   * - 5
     - The config file rule matching *this* file
     - Config author, per file pattern.
   * - 6
     - Per-call arguments on this open / write call
     - The caller, now.

The non-intuitive rung is that layer 5 beats layer 4: a rule that matches
``*.png`` overrides a user's "global" per-key attribute for PNG files.
The rationale is CSS-specificity -- the more specific selector wins
regardless of who authored it -- and the escape hatch is the per-call
argument (layer 6), which always wins. (Layer 3, composable profile
selection, is planned; see below.)

Per-format round-trip
---------------------

Not every format round-trips color identity 1:1, but each format's
behavior is predictable. Writing a tagged image and reading it back
yields:

.. list-table:: Per-format color-identity round-trip
   :widths: 12 88
   :header-rows: 1

   * - Format
     - What survives a write / read round-trip
   * - OpenEXR
     - ``colorInteropID`` is preserved (native slot). CICP is never
       written -- even an explicit ``"CICP"`` tuple is stripped, since
       EXR has no CICP convention.
   * - PNG
     - A display identity is carried as a ``cICP`` chunk and read back as
       ``"CICP"``, resolving to the display space. It round-trips as
       CICP, not as ``colorInteropID``.
   * - TIFF
     - No color identity is carried: a round-tripped TIFF reads back
       untagged. Lossy but predictable.
   * - JPEG
     - No color identity is carried; the reader's fixed sRGB assumption
       resolves every JPEG to ``srgb_rec709_scene`` regardless of the
       source tag.

Planned
-------

The following extend the same mechanism and are not yet available:

- **Composable profile selection** (layer 3): an environment variable and
  a global attribute that select active profiles as a ``+``/``-``
  expression at profile and individual-key granularity.
- **Verbose write emission** and **forcing** ``colorInteropID`` into
  formats that have no native slot (``write:verbose``,
  ``write:force_interop_id``).
- The ``oiio:broadcast`` delivery profile, which changes write-time
  gamut/container mapping for broadcast delivery.
