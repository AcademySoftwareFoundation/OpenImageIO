// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio

#include <OpenImageIO/paramlist.h>

#include <OpenImageIO/c-paramlist.h>

#include "util.h"

DEFINE_POINTER_CASTS(ParamValue)
#undef DEFINE_POINTER_CASTS

using OIIO::bit_cast;

extern "C" {

OIIO_ParamValue*
OIIO_ParamValue_new(const char* name, OIIO_TypeDesc type, int nvalues,
                    OIIO_ParamValue_Interp interp, const void* value, bool copy)
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
}
