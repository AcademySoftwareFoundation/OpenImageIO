// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


#pragma once

#include <OpenEXR/ImathBox.h>
#include <OpenEXR/ImathVec.h>

#include <Field3D/DenseField.h>
#include <Field3D/Field3DFile.h>
#include <Field3D/FieldMetadata.h>
#include <Field3D/InitIO.h>
#include <Field3D/MACField.h>
#include <Field3D/SparseField.h>

#ifndef FIELD3D_NS
#    define FIELD3D_NS Field3D
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
    bool vecfield = false;  // true=vector, false=scalar
    Box3i extents;
    Box3i dataWindow;
    ImageSpec spec;
    FieldRes::Ptr field;

    layerrecord() {}
};



// Define an abstract interface that allows us to get special information
// from the Field3DInput.
class Field3DInput_Interface : public ImageInput {
public:
    Field3DInput_Interface() {}

    // Transform world space P to local space P.
    virtual void worldToLocal(const Imath::V3f& wsP, Imath::V3f& lsP,
                              float time) const = 0;
};



// Return a reference to the mutex that allows us to use f3d with multiple
// threads.
spin_mutex&
field3d_mutex();

void
oiio_field3d_initialize();


}  // end namespace f3dpvt

OIIO_NAMESPACE_END
