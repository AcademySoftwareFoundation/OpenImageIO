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
using namespace boost::python;
using self_ns::str;



static ROI ROI_All;



// Declare the OIIO ROI class to Python
void declare_roi()
{

    class_<ROI>("ROI")
        .def_readwrite("xbegin",   &ROI::xbegin)
        .def_readwrite("xend",     &ROI::xend)
        .def_readwrite("ybegin",   &ROI::ybegin)
        .def_readwrite("yend",     &ROI::yend)
        .def_readwrite("zbegin",   &ROI::zbegin)
        .def_readwrite("zend",     &ROI::zend)
        .def_readwrite("chbegin",  &ROI::chbegin)
        .def_readwrite("chend",    &ROI::chend)

        .def(init<int,int,int,int>())
        .def(init<int,int,int,int,int,int>())
        .def(init<int,int,int,int,int,int,int,int>())
        .def(init<const ROI&>())

        .add_property("defined",   &ROI::defined)
        .add_property("width",     &ROI::width)
        .add_property("height",    &ROI::height)
        .add_property("depth",     &ROI::depth)
        .add_property("nchannels", &ROI::nchannels)
        .add_property("npixels",   &ROI::npixels)

        .def_readonly("All",                &ROI_All)

        // Define Python str(ROI), it automatically uses '<<'
        .def(str(self))    // __str__

        // roi_union, roi_intersection, get_roi(spec), get_roi_full(spec)
        // set_roi(spec,newroi), set_roi_full(newroi)

        // overloaded operators
        .def(self == other<ROI>())    // operator==
        .def(self != other<ROI>())    // operator!=
    ;

    def("union",        &roi_union);
    def("intersection", &roi_intersection);
    def("get_roi",      &get_roi);
    def("get_roi_full", &get_roi_full);
    def("set_roi",      &set_roi);
    def("set_roi_full", &set_roi_full);
}

} // namespace PyOpenImageIO

