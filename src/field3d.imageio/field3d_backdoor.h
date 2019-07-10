// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


#pragma once

OIIO_NAMESPACE_BEGIN

namespace f3dpvt {


// Define an abstract interface that allows us to get special information
// from the Field3DInput.
class Field3DInput_Interface : public ImageInput {
public:
    Field3DInput_Interface() {}

    // Transform world space P to local space P.
    virtual void worldToLocal(const Imath::V3f& wsP, Imath::V3f& lsP,
                              float time) const = 0;
};


}  // end namespace f3dpvt

OIIO_NAMESPACE_END
