/*
  Copyright 2009 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/

#include "py_oiio.h"

namespace PyOpenImageIO
{


static ROI ROI_All;


// Declare the OIIO ROI class to Python
void declare_roi(py::module& m)
{
    using namespace py;

    py::class_<ROI>(m, "ROI")
        .def_readwrite("xbegin",   &ROI::xbegin)
        .def_readwrite("xend",     &ROI::xend)
        .def_readwrite("ybegin",   &ROI::ybegin)
        .def_readwrite("yend",     &ROI::yend)
        .def_readwrite("zbegin",   &ROI::zbegin)
        .def_readwrite("zend",     &ROI::zend)
        .def_readwrite("chbegin",  &ROI::chbegin)
        .def_readwrite("chend",    &ROI::chend)

        .def(py::init<>())
        .def(py::init<int,int,int,int>())
        .def(py::init<int,int,int,int,int,int>())
        .def(py::init<int,int,int,int,int,int,int,int>())
        .def(py::init<const ROI&>())

        // .def("defined",   [](const ROI& roi) { return (int)roi.defined(); })
        .def_property_readonly("defined",   &ROI::defined)
        .def_property_readonly("width",     &ROI::width)
        .def_property_readonly("height",    &ROI::height)
        .def_property_readonly("depth",     &ROI::depth)
        .def_property_readonly("nchannels", &ROI::nchannels)
        .def_property_readonly("npixels",   &ROI::npixels)
        .def("contains", &ROI::contains,
             "x"_a, "y"_a, "z"_a=0, "ch"_a=0)

        .def_readonly_static("All",                &ROI_All)

        // Conversion to string
        .def("__str__", [](const ROI& roi){ return Strutil::format("%s", roi); })

        // roi_union, roi_intersection, get_roi(spec), get_roi_full(spec)
        // set_roi(spec,newroi), set_roi_full(newroi)

        // overloaded operators
       .def(py::self == py::self)    // operator==
       .def(py::self != py::self)    // operator!=
    ;

    m.def("union",        &roi_union, "union of two ROI");
    m.def("intersection", &roi_intersection);
    m.def("get_roi",      &get_roi);
    m.def("get_roi_full", &get_roi_full);
    m.def("set_roi",      &set_roi);
    m.def("set_roi_full", &set_roi_full);
}


} // namespace PyOpenImageIO

