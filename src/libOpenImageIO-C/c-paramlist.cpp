// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio

#include <OpenImageIO/paramlist.h>
#include <OpenImageIO/strutil.h>

#include <OpenImageIO/c-paramlist.h>

#include "util.h"

DEFINE_POINTER_CASTS(ParamValue)
#undef DEFINE_POINTER_CASTS

using OIIO::bit_cast;
using OIIO::Strutil::safe_strcpy;

extern "C" {

OIIO_ParamValue*
OIIO_ParamValue_new(const char* name, OIIO_TypeDesc type, int nvalues,
                    int interp, const void* value, bool copy)
{
    return to_c(new OIIO::ParamValue(
        name, bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(type), nvalues,
        (OIIO::ParamValue::Interp)interp, value, copy));
}



OIIO_ParamValue*
OIIO_ParamValue_from_string(const char* name, OIIO_TypeDesc type,
                            const char* string)
{
    return to_c(new OIIO::ParamValue(
        name, bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(type), string));
}



OIIO_ParamValue*
OIIO_ParamValue_copy(OIIO_ParamValue* pv)
{
    return to_c(new OIIO::ParamValue(*to_cpp(pv)));
}



void
OIIO_ParamValue_delete(const OIIO_ParamValue* pv)
{
    delete to_cpp(pv);
}



void
OIIO_ParamValue_init(OIIO_ParamValue* pv, const char* name, OIIO_TypeDesc type,
                     int nvalues, int interp, const void* value, bool copy)
{
    to_cpp(pv)->init(name, bit_cast<OIIO_TypeDesc, OIIO::TypeDesc>(type),
                     nvalues, (OIIO::ParamValue::Interp)interp, value, copy);
}



const char*
OIIO_ParamValue_name(const OIIO_ParamValue* pv)
{
    return to_cpp(pv)->name().c_str();
}



OIIO_TypeDesc
OIIO_ParamValue_type(const OIIO_ParamValue* pv)
{
    auto td = to_cpp(pv)->type();
    return bit_cast<OIIO::TypeDesc, OIIO_TypeDesc>(td);
}



int
OIIO_ParamValue_nvalues(const OIIO_ParamValue* pv)
{
    return to_cpp(pv)->nvalues();
}



const void*
OIIO_ParamValue_data(const OIIO_ParamValue* pv)
{
    return to_cpp(pv)->data();
}



int
OIIO_ParamValue_datasize(const OIIO_ParamValue* pv)
{
    return to_cpp(pv)->datasize();
}



int
OIIO_ParamValue_interp(const OIIO_ParamValue* pv)
{
    return to_cpp(pv)->interp();
}



bool
OIIO_ParamValue_is_nonlocal(const OIIO_ParamValue* pv)
{
    return to_cpp(pv)->is_nonlocal();
}



int
OIIO_ParamValue_get_int(const OIIO_ParamValue* pv, int defaultval)
{
    return to_cpp(pv)->get_int(defaultval);
}



int
OIIO_ParamValue_get_int_indexed(const OIIO_ParamValue* pv, int index,
                                int defaultval)
{
    return to_cpp(pv)->get_int_indexed(index, defaultval);
}



float
OIIO_ParamValue_get_float(const OIIO_ParamValue* pv, float defaultval)
{
    return to_cpp(pv)->get_float(defaultval);
}



float
OIIO_ParamValue_get_float_indexed(const OIIO_ParamValue* pv, int index,
                                  float defaultval)
{
    return to_cpp(pv)->get_float_indexed(index, defaultval);
}



void
OIIO_ParamValue_get_string(const OIIO_ParamValue* pv, int max_num_strings,
                           char* buffer, int buffer_len)
{
    std::string s = to_cpp(pv)->get_string(max_num_strings);
    safe_strcpy(buffer, s.c_str(), buffer_len);
}



void
OIIO_ParamValue_get_string_indexed(const OIIO_ParamValue* pv, int index,
                                   char* buffer, int buffer_len)
{
    std::string s = to_cpp(pv)->get_string_indexed(index);
    safe_strcpy(buffer, s.c_str(), buffer_len);
}
}
