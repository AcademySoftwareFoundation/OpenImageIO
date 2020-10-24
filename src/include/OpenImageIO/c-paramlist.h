// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio

#pragma once

#include <OpenImageIO/export.h>

#include <OpenImageIO/c-typedesc.h>

#ifdef __cplusplus
extern "C" {
#endif

/// OIIO_ParamValue holds a parameter and a pointer to its value(s)
///
/// Nomenclature: if you have an array of 4 colors for each of 15 points...
///  - There are 15 VALUES
///  - Each value has an array of 4 ELEMENTS, each of which is a color
///  - A color has 3 COMPONENTS (R, G, B)
///
typedef struct OIIO_ParamValue OIIO_ParamValue;

enum OIIO_ParamValue_Interp {
    OIIO_ParamValue_INTERP_CONSTANT = 0,
    OIIO_ParamValue_INTERP_PERPIECE = 1,
    OIIO_ParamValue_INTERP_LINEAR   = 2,
    OIIO_ParamValue_INTERP_VERTEX   = 3
};

/// Construct a new OIIO_ParamValue
OIIO_API OIIO_ParamValue*
OIIO_ParamValue_new(const char* name, OIIO_TypeDesc type, int nvalues,
                    OIIO_ParamValue_Interp interp, const void* value,
                    bool copy);

/// Construct a new OIIO_ParamValue by parsing the given string
///
/// Equivalent C++: `new ParamValue(name, type, string)`
///
OIIO_API OIIO_ParamValue*
OIIO_ParamValue_from_string(const char* name, OIIO_TypeDesc type,
                            const char* string);

/// Make a copy of the given OIIO_ParamValue
///
/// Equivalent C++: `new ParamValue(*pv)`
///
OIIO_API OIIO_ParamValue*
OIIO_ParamValue_copy(OIIO_ParamValue* pv);

/// Delete a OIIO_ParamValue
///
/// Equivalent C++: `delete pv`
///
OIIO_API void
OIIO_ParamValue_delete(const OIIO_ParamValue* pv);

#ifdef __cplusplus
}
#endif
