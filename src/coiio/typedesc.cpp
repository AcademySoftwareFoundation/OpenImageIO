#include <OpenImageIO/typedesc.h>

#include "typedesc.h"
#include "util.hpp"

// Sanity check that our types are equivalent
static_assert(sizeof(TypeDesc) == sizeof(OIIO::TypeDesc));
static_assert(alignof(TypeDesc) == alignof(OIIO::TypeDesc));
static_assert(offsetof(TypeDesc, basetype)
              == offsetof(OIIO::TypeDesc, basetype));
static_assert(offsetof(TypeDesc, aggregate)
              == offsetof(OIIO::TypeDesc, aggregate));
static_assert(offsetof(TypeDesc, vecsemantics)
              == offsetof(OIIO::TypeDesc, vecsemantics));
static_assert(offsetof(TypeDesc, reserved)
              == offsetof(OIIO::TypeDesc, reserved));
static_assert(offsetof(TypeDesc, arraylen)
              == offsetof(OIIO::TypeDesc, arraylen));

extern "C" {

TypeDesc
TypeDesc_from_string(const char* typestring)
{
    return pun<TypeDesc>(OIIO::TypeDesc(typestring));
}
}