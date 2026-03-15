// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "py_oiio.h"


NB_MODULE(_OpenImageIO, m)
{
    m.doc() = "OpenImageIO nanobind bindings.";

    PyOpenImageIO::declare_typedesc(m);
    PyOpenImageIO::declare_roi(m);
    PyOpenImageIO::declare_imagespec(m);
    m.attr("__version__") = OIIO_VERSION_STRING;
}
