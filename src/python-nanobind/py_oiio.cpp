// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <OpenImageIO/oiioversion.h>

#include <nanobind/nanobind.h>

namespace nb = nanobind;

namespace PyOpenImageIO {

void
declare_roi(nb::module_& m);

}  // namespace PyOpenImageIO


NB_MODULE(_OpenImageIO, m)
{
    m.doc() = "Experimental OpenImageIO nanobind bindings.";

    PyOpenImageIO::declare_roi(m);
    m.attr("__version__") = OIIO_VERSION_STRING;
}
