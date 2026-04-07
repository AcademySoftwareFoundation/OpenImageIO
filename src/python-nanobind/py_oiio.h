// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/typedesc.h>

#include <nanobind/nanobind.h>
#include <nanobind/operators.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

namespace nb = nanobind;
using namespace nb::literals;

namespace PyOpenImageIO {

OIIO_NAMESPACE_USING

void
declare_roi(nb::module_& m);
void
declare_imagespec(nb::module_& m);
void
declare_typedesc(nb::module_& m);

}  // namespace PyOpenImageIO
