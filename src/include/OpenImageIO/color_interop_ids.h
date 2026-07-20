// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <OpenImageIO/export.h>
#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/span.h>
#include <OpenImageIO/string_view.h>

OIIO_NAMESPACE_BEGIN

namespace ColorInteropIDs {

/// The canonical Color Interop Forum IDs declared by OIIO's built-in
/// interop identities registry (see the CIF recommendation "An ID for
/// Color Interop",
/// https://github.com/AcademySoftwareFoundation/ColorInterop/wiki), in
/// deterministic (sorted) registry order. The returned storage has
/// process lifetime. The values are the canonical ID strings -- pass
/// them anywhere a `string_view` CIID is accepted (e.g.
/// ColorConfig::get_color_interop_id). This is registry *data*, not an
/// exhaustive ID grammar: raw strings remain first-class for ids no
/// finite set can enumerate (local/custom/icc/user-namespaced ids).
///
/// @version 3.2
OIIO_API cspan<string_view>
all();

}  // namespace ColorInteropIDs

OIIO_NAMESPACE_END
