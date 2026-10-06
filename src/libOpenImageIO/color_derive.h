// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <OpenColorIO/OpenColorIO.h>

#include <OpenImageIO/span.h>


OIIO_NAMESPACE_BEGIN
namespace pvt {

/// Derive what `cs`'s transforms establish about it: its RGB primaries and
/// white (CIE xy, R G B W), as `xy` pointing to one of `gamuts`, and the
/// exponent of a pure-power transfer function (1 for a linear one). It reads
/// the conversion to `config`'s scene or display interchange role, which
/// must be one shared per-channel curve followed by a matrix; anything else
/// establishes nothing. `ap0` is ACES2065-1's primaries and white, which the
/// scene interchange is. Properties that are not established are left
/// unchanged.
void derive_color_space_properties(const OCIO_NAMESPACE::Config& config,
                                   const OCIO_NAMESPACE::ColorSpace& cs,
                                   cspan<const float*> gamuts, const float* ap0,
                                   const float*& xy, float& gamma);

}  // namespace pvt
OIIO_NAMESPACE_END
