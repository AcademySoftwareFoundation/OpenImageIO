// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <array>

#include <OpenColorIO/OpenColorIO.h>

#include <OpenImageIO/span.h>


OIIO_NAMESPACE_BEGIN
namespace pvt {

/// The RGB primaries and white point (Rx, Ry, Gx, Gy, Bx, By, Wx, Wy, CIE
/// 1931 xy) that the transforms of `cs` establish. They come from the
/// matrices that lead the conversion from `config`'s interchange space for
/// the reference type of `cs` (ACES2065-1 or CIE XYZ-D65): named as one of
/// `gamuts` when the matrices match it under Bradford, CAT02 or no chromatic
/// adaptation, and otherwise under the white point and adaptation that give
/// the most exact coordinates. A space whose transforms are not simple but
/// end in a ColorSpaceTransform answers as that transform's color space.
/// Returns false, leaving `xy` alone, if nothing is established.
bool derive_chromaticities(const OCIO_NAMESPACE::Config& config,
                           const OCIO_NAMESPACE::ColorSpace& cs,
                           cspan<const float*> gamuts,
                           std::array<float, 8>& xy);

}  // namespace pvt
OIIO_NAMESPACE_END
