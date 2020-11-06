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
OIIOC_API OIIO_ParamValue*
OIIO_ParamValue_new(const char* name, OIIO_TypeDesc type, int nvalues,
                    int interp, const void* value, bool copy);

/// Construct a new OIIO_ParamValue by parsing the given string
///
/// Equivalent C++: `new ParamValue(name, type, string)`
///
OIIOC_API OIIO_ParamValue*
OIIO_ParamValue_from_string(const char* name, OIIO_TypeDesc type,
                            const char* string);

/// Make a copy of the given OIIO_ParamValue
///
/// Equivalent C++: `new ParamValue(*pv)`
///
OIIOC_API OIIO_ParamValue*
OIIO_ParamValue_copy(OIIO_ParamValue* pv);

/// Delete a OIIO_ParamValue
///
/// Equivalent C++: `delete pv`
///
OIIOC_API void
OIIO_ParamValue_delete(const OIIO_ParamValue* pv);

///
/// Equivalent C++: `pv->init(name, type, nvalues, interp, value, copy)`
///
OIIOC_API void
OIIO_ParamValue_init(OIIO_ParamValue* pv, const char* name, OIIO_TypeDesc type,
                     int nvalues, int interp, const void* value, bool copy);

///
/// Equivalent C++: `pv->name()`
///
OIIOC_API const char*
OIIO_ParamValue_name(const OIIO_ParamValue* pv);

///
/// Equivalent C++: `pv->type()`
///
OIIOC_API OIIO_TypeDesc
OIIO_ParamValue_type(const OIIO_ParamValue* pv);

///
/// Equivalent C++: `pv->nvalues()`
///
OIIOC_API int
OIIO_ParamValue_nvalues(const OIIO_ParamValue* pv);

///
/// Equivalent C++: `pv->data()`
///
OIIOC_API const void*
OIIO_ParamValue_data(const OIIO_ParamValue* pv);

///
/// Equivalent C++: `pv->datasize()`
///
OIIOC_API int
OIIO_ParamValue_datasize(const OIIO_ParamValue* pv);

///
/// Equivalent C++: `pv->interp()`
///
OIIOC_API int
OIIO_ParamValue_interp(const OIIO_ParamValue* pv);

///
/// Equivalent C++: `pv->is_nonlocal()`
///
OIIOC_API bool
OIIO_ParamValue_is_nonlocal(const OIIO_ParamValue* pv);

/// Retrive an integer, with converstions from a wide variety of type
/// cases, including unsigned, short, byte. Not float. It will retrive
/// from a string, but only if the string is entirely a valid int
/// format. Unconvertible types return the default value.
///
/// Equivalent C++: `pv->get_int(defaultval)`
///
OIIOC_API int
OIIO_ParamValue_get_int(const OIIO_ParamValue* pv, int defaultval);

/// Retrive an integer, with converstions from a wide variety of type
/// cases, including unsigned, short, byte. Not float. It will retrive
/// from a string, but only if the string is entirely a valid int
/// format. Unconvertible types return the default value.
///
/// Equivalent C++: `pv->get_int_indexed(index, defaultval)`
///
OIIOC_API int
OIIO_ParamValue_get_int_indexed(const OIIO_ParamValue* pv, int index,
                                int defaultval);

/// Retrive a float, with converstions from a wide variety of type
/// cases, including integers. It will retrive from a string, but only
/// if the string is entirely a valid float format. Unconvertible types
/// return the default value.
///
/// Equivalent C++: `pv->get_float(defaultval)`
///
OIIOC_API float
OIIO_ParamValue_get_float(const OIIO_ParamValue* pv, float defaultval);

/// Retrive a float, with converstions from a wide variety of type
/// cases, including integers. It will retrive from a string, but only
/// if the string is entirely a valid float format. Unconvertible types
/// return the default value.
///
/// Equivalent C++: `pv->get_float_indexed(index, defaultval)`
///
OIIOC_API float
OIIO_ParamValue_get_float_indexed(const OIIO_ParamValue* pv, int index,
                                  float defaultval);

/// Convert any type to a string value. An optional maximum number of
/// elements is also passed. In the case of a single string, just the
/// string directly is returned. But for an array of strings, the array
/// is returned as one string that's a comma-separated list of double-
/// quoted, escaped strings.
/// @param pv The ParamValue to get the string from
/// @param max_num_strings The maximum number of strings to get
/// @param buffer Caller-provided storage to put the string(s) into
/// @param buffer_len The length of the string storage. If the generated string
///                     is longer than buffer_len it will be truncated to fit.
OIIOC_API void
OIIO_ParamValue_get_string(const OIIO_ParamValue* pv, int max_num_strings,
                           char* buffer, int buffer_len);

/// Convert any type to a string value. An optional maximum number of
/// elements is also passed. In the case of a single string, just the
/// string directly is returned. But for an array of strings, the array
/// is returned as one string that's a comma-separated list of double-
/// quoted, escaped strings.
/// @param pv The ParamValue to get the string from
/// @param index The index into the array to get the string for
/// @param buffer Caller-provided storage to put the string(s) into
/// @param buffer_len The length of the string storage. If the generated string
///                     is longer than buffer_len it will be truncated to fit.
OIIOC_API void
OIIO_ParamValue_get_string_indexed(const OIIO_ParamValue* pv, int index,
                                   char* buffer, int buffer_len);


#ifdef __cplusplus
}
#endif
