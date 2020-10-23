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
TypeDesc OIIO_TYPE_UNKNOWN = { OIIO_BASETYPE_UNKNOWN, OIIO_AGGREGATE_SCALAR,
                               OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TYPE_FLOAT = { OIIO_BASETYPE_FLOAT, OIIO_AGGREGATE_SCALAR,
                             OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TYPE_COLOR = { OIIO_BASETYPE_FLOAT, OIIO_AGGREGATE_VEC3,
                             OIIO_VECSEMANTICS_COLOR, 0, 0 };

TypeDesc OIIO_TYPE_POINT = { OIIO_BASETYPE_FLOAT, OIIO_AGGREGATE_VEC3,
                             OIIO_VECSEMANTICS_POINT, 0, 0 };

TypeDesc OIIO_TYPE_VECTOR = { OIIO_BASETYPE_FLOAT, OIIO_AGGREGATE_VEC3,
                              OIIO_VECSEMANTICS_VECTOR, 0, 0 };

TypeDesc OIIO_TYPE_NORMAL = { OIIO_BASETYPE_FLOAT, OIIO_AGGREGATE_VEC3,
                              OIIO_VECSEMANTICS_NORMAL, 0, 0 };

TypeDesc OIIO_TYPE_MATRIX33 = { OIIO_BASETYPE_FLOAT, OIIO_AGGREGATE_MATRIX33,
                                OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TYPE_MATRIX44 = { OIIO_BASETYPE_FLOAT, OIIO_AGGREGATE_MATRIX44,
                                OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TYPE_MATRIX = OIIO_TYPE_MATRIX44;

TypeDesc OIIO_TYPE_FLOAT2 = { OIIO_BASETYPE_FLOAT, OIIO_AGGREGATE_VEC2,
                              OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TYPE_VECTOR2 = { OIIO_BASETYPE_FLOAT, OIIO_AGGREGATE_VEC2,
                               OIIO_VECSEMANTICS_VECTOR, 0, 0 };

TypeDesc OIIO_TYPE_FLOAT4 = { OIIO_BASETYPE_FLOAT, OIIO_AGGREGATE_VEC4,
                              OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TYPE_VECTOR4 = OIIO_TYPE_FLOAT4;

TypeDesc OIIO_TYPE_STRING = { OIIO_BASETYPE_STRING, OIIO_AGGREGATE_SCALAR,
                              OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TYPE_INT = { OIIO_BASETYPE_INT, OIIO_AGGREGATE_SCALAR,
                           OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TYPE_UINT = { OIIO_BASETYPE_UINT, OIIO_AGGREGATE_SCALAR,
                            OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TYPE_INT32 = { OIIO_BASETYPE_INT32, OIIO_AGGREGATE_SCALAR,
                             OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TYPE_UINT32 = { OIIO_BASETYPE_UINT32, OIIO_AGGREGATE_SCALAR,
                              OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TYPE_INT16 = { OIIO_BASETYPE_INT32, OIIO_AGGREGATE_SCALAR,
                             OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TYPE_UINT16 = { OIIO_BASETYPE_UINT32, OIIO_AGGREGATE_SCALAR,
                              OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TYPE_INT8 = { OIIO_BASETYPE_INT32, OIIO_AGGREGATE_SCALAR,
                            OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TYPE_UINT8 = { OIIO_BASETYPE_UINT32, OIIO_AGGREGATE_SCALAR,
                             OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TYPE_VECTOR2I = { OIIO_BASETYPE_INT, OIIO_AGGREGATE_VEC2,
                                OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TYPE_HALF = { OIIO_BASETYPE_HALF, OIIO_AGGREGATE_SCALAR,
                            OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };

TypeDesc OIIO_TYPE_TIMECODE = { OIIO_BASETYPE_UINT, OIIO_AGGREGATE_SCALAR,
                                OIIO_VECSEMANTICS_TIMECODE, 0, 2 };

TypeDesc OIIO_TYPE_KEYCODE = { OIIO_BASETYPE_INT, OIIO_AGGREGATE_SCALAR,
                               OIIO_VECSEMANTICS_KEYCODE, 0, 7 };

TypeDesc OIIO_TYPE_RATIONAL = { OIIO_BASETYPE_INT, OIIO_AGGREGATE_VEC2,
                                OIIO_VECSEMANTICS_RATIONAL, 0, 0 };

TypeDesc OIIO_TYPE_POINTER = { OIIO_BASETYPE_PTR, OIIO_AGGREGATE_SCALAR,
                               OIIO_VECSEMANTICS_NOSEMANTICS, 0, 0 };
}