// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio


#include <OpenImageIO/dassert.h>
#include <OpenImageIO/typedesc.h>

#include "c-typedesc.h"

using OIIO::bit_cast;

// Sanity check that our types are equivalent
OIIO_STATIC_ASSERT(sizeof(TypeDesc) == sizeof(OIIO::TypeDesc));
OIIO_STATIC_ASSERT(alignof(TypeDesc) == alignof(OIIO::TypeDesc));
OIIO_STATIC_ASSERT(offsetof(TypeDesc, basetype)
                   == offsetof(OIIO::TypeDesc, basetype));
OIIO_STATIC_ASSERT(offsetof(TypeDesc, aggregate)
                   == offsetof(OIIO::TypeDesc, aggregate));
OIIO_STATIC_ASSERT(offsetof(TypeDesc, vecsemantics)
                   == offsetof(OIIO::TypeDesc, vecsemantics));
OIIO_STATIC_ASSERT(offsetof(TypeDesc, reserved)
                   == offsetof(OIIO::TypeDesc, reserved));
OIIO_STATIC_ASSERT(offsetof(TypeDesc, arraylen)
                   == offsetof(OIIO::TypeDesc, arraylen));

extern "C" {

TypeDesc
TypeDesc_from_string(const char* typestring)
{
    return bit_cast<OIIO::TypeDesc, TypeDesc>(OIIO::TypeDesc(typestring));
}



// Definitions for the convenience TypeDescs
TypeDesc OIIO_TypeUnknown = { OIIO_BASETYPE_UNKNOWN, OIIO_AGGREGATE_SCALAR,
                              OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TypeFloat = { OIIO_BASETYPE_FLOAT, OIIO_AGGREGATE_SCALAR,
                            OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TypeColor = { OIIO_BASETYPE_FLOAT, OIIO_AGGREGATE_VEC3,
                            OIIO_VECSEMANTICS_COLOR, 0, 0 };

TypeDesc OIIO_TypePoint = { OIIO_BASETYPE_FLOAT, OIIO_AGGREGATE_VEC3,
                            OIIO_VECSEMANTICS_POINT, 0, 0 };

TypeDesc OIIO_TypeVector = { OIIO_BASETYPE_FLOAT, OIIO_AGGREGATE_VEC3,
                             OIIO_VECSEMANTICS_VECTOR, 0, 0 };

TypeDesc OIIO_TypeNormal = { OIIO_BASETYPE_FLOAT, OIIO_AGGREGATE_VEC3,
                             OIIO_VECSEMANTICS_NORMAL, 0, 0 };

TypeDesc OIIO_TypeMatrix33 = { OIIO_BASETYPE_FLOAT, OIIO_AGGREGATE_MATRIX33,
                               OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TypeMatrix44 = { OIIO_BASETYPE_FLOAT, OIIO_AGGREGATE_MATRIX44,
                               OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TypeMatrix = OIIO_TypeMatrix44;

TypeDesc OIIO_TypeFloat2 = { OIIO_BASETYPE_FLOAT, OIIO_AGGREGATE_VEC2,
                             OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TypeVector2 = { OIIO_BASETYPE_FLOAT, OIIO_AGGREGATE_VEC2,
                              OIIO_VECSEMANTICS_VECTOR, 0, 0 };

TypeDesc OIIO_TypeFloat4 = { OIIO_BASETYPE_FLOAT, OIIO_AGGREGATE_VEC4,
                             OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TypeVector4 = OIIO_TypeFloat4;

TypeDesc OIIO_TypeString = { OIIO_BASETYPE_STRING, OIIO_AGGREGATE_SCALAR,
                             OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TypeInt = { OIIO_BASETYPE_INT, OIIO_AGGREGATE_SCALAR,
                          OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TypeUInt = { OIIO_BASETYPE_UINT, OIIO_AGGREGATE_SCALAR,
                           OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TypeInt32 = { OIIO_BASETYPE_INT32, OIIO_AGGREGATE_SCALAR,
                            OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TypeUInt32 = { OIIO_BASETYPE_UINT32, OIIO_AGGREGATE_SCALAR,
                             OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TypeInt16 = { OIIO_BASETYPE_INT32, OIIO_AGGREGATE_SCALAR,
                            OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TypeUInt16 = { OIIO_BASETYPE_UINT32, OIIO_AGGREGATE_SCALAR,
                             OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TypeInt8 = { OIIO_BASETYPE_INT32, OIIO_AGGREGATE_SCALAR,
                           OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TypeUInt8 = { OIIO_BASETYPE_UINT32, OIIO_AGGREGATE_SCALAR,
                            OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TypeVector2i = { OIIO_BASETYPE_INT, OIIO_AGGREGATE_VEC2,
                               OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TypeHalf = { OIIO_BASETYPE_HALF, OIIO_AGGREGATE_SCALAR,
                           OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TypeTimecode = { OIIO_BASETYPE_UINT, OIIO_AGGREGATE_SCALAR,
                               OIIO_VECSEMANTICS_TIMECODE, 0, 2 };

TypeDesc OIIO_TypeKeycode = { OIIO_BASETYPE_INT, OIIO_AGGREGATE_SCALAR,
                              OIIO_VECSEMANTICS_KEYCODE, 0, 7 };

TypeDesc OIIO_TypeRational = { OIIO_BASETYPE_INT, OIIO_AGGREGATE_VEC2,
                               OIIO_VECSEMANTICS_RATIONAL, 0, 0 };

TypeDesc OIIO_TypePointer = { OIIO_BASETYPE_PTR, OIIO_AGGREGATE_SCALAR,
                              OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };
}
