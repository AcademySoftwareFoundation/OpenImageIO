// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "oiio-sys/include/typedesc.h"
#include "rust/cxx.h"
#include <OpenImageIO/string_view.h>
#include <OpenImageIO/typedesc.h>
#include <memory>
#include <stdexcept>
#include <stdio.h>

namespace oiio_ffi {
TypeDesc
typedesc_new(BaseType btype, Aggregate agg, VecSemantics semantics,
             int arraylen) noexcept
{
    return TypeDesc(btype, agg, semantics, arraylen);
}

TypeDesc
typedesc_from_basetype_arraylen(BaseType btype, int arraylen) noexcept
{
    return TypeDesc(btype, arraylen);
}

TypeDesc
typedesc_from_basetype_aggregate_arraylen(BaseType btype, Aggregate agg,
                                          int arraylen) noexcept
{
    return TypeDesc(btype, agg, arraylen);
}

TypeDesc
typedesc_from_string(rust::Str typestring)
{
    OIIO::string_view typestring_view(typestring.data(), typestring.size());
    return TypeDesc(typestring_view);
}

TypeDesc
typedesc_clone(const TypeDesc& t) noexcept
{
    return TypeDesc(t);
}


rust::Str
typedesc_as_str(const TypeDesc& typedesc)
{
    return rust::Str(typedesc.c_str());
}

size_t
typedesc_numelements(const TypeDesc& typedesc) noexcept
{
    return typedesc.numelements();
}

size_t
typedesc_basevalues(const TypeDesc& typedesc) noexcept
{
    return typedesc.basevalues();
}

bool
typedesc_is_array(const TypeDesc& typedesc) noexcept
{
    return typedesc.is_array();
}

bool
typedesc_is_unsized_array(const TypeDesc& typedesc) noexcept
{
    return typedesc.is_unsized_array();
}

bool
typedesc_is_sized_array(const TypeDesc& typedesc) noexcept
{
    return typedesc.is_sized_array();
}

size_t
typedesc_size(const TypeDesc& typedesc) noexcept
{
    return typedesc.size();
}

TypeDesc
typedesc_elementtype(const TypeDesc& typedesc) noexcept
{
    return typedesc.elementtype();
}

size_t
typedesc_elementsize(const TypeDesc& typedesc) noexcept
{
    return typedesc.elementsize();
}

TypeDesc
typedesc_scalartype(const TypeDesc& typedesc)
{
    return typedesc.scalartype();
}

size_t
typedesc_basesize(const TypeDesc& typedesc) noexcept
{
    return typedesc.basesize();
}

bool
typedesc_is_floating_point(const TypeDesc& typedesc) noexcept
{
    return typedesc.is_floating_point();
}

bool
typedesc_is_signed(const TypeDesc& typedesc) noexcept
{
    return typedesc.is_signed();
}

bool
typedesc_is_unknown(const TypeDesc& typedesc) noexcept
{
    return typedesc.is_unknown();
}

size_t
typedesc_fromstring(TypeDesc& typedesc, rust::Str typestring)
{
    return typedesc.fromstring(
        std::string_view(typestring.data(), typestring.size()));
}

bool
typedesc_eq(const TypeDesc& typedesc, const TypeDesc& t) noexcept
{
    return typedesc == t;
}

bool
typedesc_ne(const TypeDesc& typedesc, const TypeDesc& t) noexcept
{
    return typedesc != t;
}

bool
typedesc_eq_basetype(const TypeDesc& t, BaseType b) noexcept
{
    return t == b;
}

bool
basetype_eq_typedesc(BaseType b, const TypeDesc& t) noexcept
{
    return b == t;
}

bool
typedesc_ne_basetype(const TypeDesc& t, BaseType b) noexcept
{
    return t != b;
}

bool
basetype_ne_typedesc(BaseType b, const TypeDesc& t) noexcept
{
    return b != t;
}

bool
typedesc_equivalent(const TypeDesc& typedesc, const TypeDesc& b) noexcept
{
    return typedesc.equivalent(b);
}

bool
typedesc_is_vec2(const TypeDesc& typedesc, BaseType b) noexcept
{
    return typedesc.is_vec2(b);
}

bool
typedesc_is_vec3(const TypeDesc& typedesc, BaseType b) noexcept
{
    return typedesc.is_vec3(b);
}

bool
typedesc_is_vec4(const TypeDesc& typedesc, BaseType b) noexcept
{
    return typedesc.is_vec4(b);
}

bool
typedesc_is_box2(const TypeDesc& typedesc, BaseType b) noexcept
{
    return typedesc.is_box2(b);
}

bool
typedesc_is_box3(const TypeDesc& typedesc, BaseType b) noexcept
{
    return typedesc.is_box3(b);
}

void
typedesc_unarray(TypeDesc& typedesc) noexcept
{
    typedesc.unarray();
}

bool
typedesc_lt(const TypeDesc& typedesc, const TypeDesc& x) noexcept
{
    return typedesc < x;
}

BaseType
typedesc_basetype_merge(TypeDesc a, TypeDesc b)
{
    return TypeDesc::basetype_merge(a, b);
}

BaseType
typedesc_basetype_merge_3(TypeDesc a, TypeDesc b, TypeDesc c)
{
    return TypeDesc::basetype_merge(a, b, c);
}

bool
typedesc_convert_type(TypeDesc srctype, rust::Slice<const uint8_t> src,
                      TypeDesc dsttype, rust::Slice<uint8_t> dst, int n)
{
    return convert_type(srctype, src.data(), dsttype, dst.data(), n);
}
}  // namespace oiio_ffi
