/*
  Copyright 2010 Larry Gritz and the other authors and contributors.
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


#include <OpenEXR/ImathVec.h>
#include <OpenEXR/ImathBox.h>

#include <Field3D/DenseField.h>
#include <Field3D/MACField.h>
#include <Field3D/SparseField.h>
#include <Field3D/InitIO.h>
#include <Field3D/Field3DFile.h>
#include <Field3D/FieldMetadata.h>
#ifndef FIELD3D_NS
#define FIELD3D_NS Field3D
#endif
using namespace FIELD3D_NS;



OIIO_NAMESPACE_BEGIN

namespace f3dpvt {


enum FieldType { Dense, Sparse, MAC };



struct layerrecord {
    std::string name;
    std::string attribute;
    std::string unique_name;
    TypeDesc datatype;
    FieldType fieldtype;
    bool vecfield;      // true=vector, false=scalar
    Box3i extents;
    Box3i dataWindow;
    ImageSpec spec;
    FieldRes::Ptr field;

    layerrecord () : vecfield(false) { }
};



// Define an abstract interface that allows us to get special information
// from the Field3DInput.
class Field3DInput_Interface : public ImageInput {
public:
    Field3DInput_Interface () { }

    // Transform world space P to local space P.
    virtual void worldToLocal (const Imath::V3f &wsP, Imath::V3f &lsP,
                               float time) const = 0;
};



// Return a reference to the mutex that allows us to use f3d with multiple
// threads.
spin_mutex &field3d_mutex ();

void oiio_field3d_initialize ();


} // end namespace f3dpvt

OIIO_NAMESPACE_END

