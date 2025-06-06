// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "rust/cxx.h"
#include <OpenImageIO/typedesc.h>
#include <memory>
#include <string>
#include <vector>

namespace oiio_ffi {
using TypeDesc     = OIIO::TypeDesc;
using BaseType     = OIIO::TypeDesc::BASETYPE;
using Aggregate    = OIIO::TypeDesc::AGGREGATE;
using VecSemantics = OIIO::TypeDesc::VECSEMANTICS;

TypeDesc
typedesc_new(BaseType btype, Aggregate agg, VecSemantics semantics,
             int arraylen) noexcept;

TypeDesc
typedesc_from_basetype_arraylen(BaseType btype, int arraylen) noexcept;

TypeDesc
typedesc_from_basetype_aggregate_arraylen(BaseType btype, Aggregate agg,
                                          int arraylen) noexcept;

TypeDesc
typedesc_from_string(rust::Str typestring);

TypeDesc
typedesc_clone(const TypeDesc& t) noexcept;

rust::Str
typedesc_as_str(const TypeDesc& typedesc);

size_t
typedesc_numelements(const TypeDesc& typedesc) noexcept;

size_t
typedesc_basevalues(const TypeDesc& typedesc) noexcept;

bool
typedesc_is_array(const TypeDesc& typedesc) noexcept;

bool
typedesc_is_unsized_array(const TypeDesc& typedesc) noexcept;

bool
typedesc_is_sized_array(const TypeDesc& typedesc) noexcept;

size_t
typedesc_size(const TypeDesc& typedesc) noexcept;

TypeDesc
typedesc_elementtype(const TypeDesc& typedesc) noexcept;

size_t
typedesc_elementsize(const TypeDesc& typedesc) noexcept;

TypeDesc
typedesc_scalartype(const TypeDesc& typedesc);

size_t
typedesc_basesize(const TypeDesc& typedesc) noexcept;

bool
typedesc_is_floating_point(const TypeDesc& typedesc) noexcept;

bool
typedesc_is_signed(const TypeDesc& typedesc) noexcept;

bool
typedesc_is_unknown(const TypeDesc& typedesc) noexcept;

size_t
typedesc_fromstring(TypeDesc& typedesc, rust::Str typestring);

bool
typedesc_eq(const TypeDesc& typedesc, const TypeDesc& t) noexcept;

bool
typedesc_ne(const TypeDesc& typedesc, const TypeDesc& t) noexcept;

bool
typedesc_eq_basetype(const TypeDesc& t, BaseType b) noexcept;

bool
basetype_eq_typedesc(BaseType b, const TypeDesc& t) noexcept;

bool
typedesc_ne_basetype(const TypeDesc& t, BaseType b) noexcept;

bool
basetype_ne_typedesc(BaseType b, const TypeDesc& t) noexcept;

bool
typedesc_equivalent(const TypeDesc& typedesc, const TypeDesc& b) noexcept;

bool
typedesc_is_vec2(const TypeDesc& typedesc, BaseType b) noexcept;

bool
typedesc_is_vec3(const TypeDesc& typedesc, BaseType b) noexcept;

bool
typedesc_is_vec4(const TypeDesc& typedesc, BaseType b) noexcept;

bool
typedesc_is_box2(const TypeDesc& typedesc, BaseType b) noexcept;

bool
typedesc_is_box3(const TypeDesc& typedesc, BaseType b) noexcept;

void
typedesc_unarray(TypeDesc& typedesc) noexcept;

bool
typedesc_lt(const TypeDesc& typedesc, const TypeDesc& x) noexcept;

BaseType
typedesc_basetype_merge(TypeDesc a, TypeDesc b);

BaseType
typedesc_basetype_merge_3(TypeDesc a, TypeDesc b, TypeDesc c);

bool
typedesc_convert_type(TypeDesc srctype, rust::Slice<const uint8_t> src,
                      TypeDesc dsttype, rust::Slice<uint8_t> dst, int n);
}  // namespace oiio_ffi
