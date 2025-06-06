// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

pub use ffi::*;

/// A TypeDesc describes simple data types.
///
/// It frequently comes up (in my experience, with renderers and image
/// handling programs) that you want a way to describe data that is passed
/// through APIs through blind pointers.  These are some simple classes
/// that provide a simple type descriptor system.  This is not meant to
/// be comprehensive -- for example, there is no provision for structs,
/// unions, typed pointers, const, or 'nested' type definitions.  Just simple
/// integer and floating point, *common* aggregates such as 3-points, and
/// reasonably-lengthed arrays thereof.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct TypeDesc {
    /// C data type at the heart of our type
    pub basetype: u8,
    /// What kind of AGGREGATE is it?
    pub aggregate: u8,
    /// Hint: What does the aggregate represent?
    pub vecsemantics: u8,
    /// Reserved for future expansion
    _reserved: u8,
    /// Array length, 0 = not array, -1 = unsized
    pub arraylen: i32,
}

unsafe impl cxx::ExternType for TypeDesc {
    type Id = cxx::type_id!("oiio_ffi::TypeDesc");
    type Kind = cxx::kind::Trivial;
}

#[cxx::bridge(namespace = oiio_ffi)]
mod ffi {
    /// BaseType is a simple enum describing the base data types that
    /// correspond (mostly) to the C/C++ built-in types.
    #[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
    #[repr(u32)]
    pub enum BaseType {
        /// unknown type
        UNKNOWN = 0,
        /// void/no type
        NONE = 1,
        /// 8-bit unsigned int values ranging from 0..255,
        /// (C/C++ `unsigned char`).
        UINT8 = 2,
        /// 8-bit int values ranging from -128..127,
        ///   (C/C++ `char`).
        INT8 = 3,
        /// 16-bit int values ranging from 0..65535,
        ///   (C/C++ `unsigned short`).
        UINT16 = 4,
        /// 16-bit int values ranging from -32768..32767,
        ///   (C/C++ `short`).
        INT16 = 5,
        /// 32-bit unsigned int values (C/C++ `unsigned int`).
        UINT32 = 6,
        /// signed 32-bit int values (C/C++ `int`).
        INT32 = 7,
        /// 64-bit unsigned int values (C/C++
        ///   `unsigned long long` on most architectures).
        UINT64 = 8,
        /// signed 64-bit int values (C/C++ `long long`
        ///   on most architectures).
        INT64 = 9,
        /// 16-bit IEEE floating point values (OpenEXR `half`).
        HALF = 10,
        /// 32-bit IEEE floating point values, (C/C++ `float`).
        FLOAT = 11,
        /// 64-bit IEEE floating point values, (C/C++ `double`).
        DOUBLE = 12,
        /// Character string.
        STRING = 13,
        /// A pointer value.
        PTR = 14,
        /// A uint64 that is the hash of a ustring.
        USTRINGHASH = 15,
        LASTBASE = 16,
    }

    /// Aggregate describes whether our TypeDesc is a simple scalar of one
    /// of the BaseType's, or one of several simple aggregates.
    ///
    /// Note that aggregates and arrays are different. A `TypeDesc(FLOAT,3)`
    /// is an array of three floats, a `TypeDesc(FLOAT,VEC3)` is a single
    /// 3-component vector comprised of floats, and `TypeDesc(FLOAT,3,VEC3)`
    /// is an array of 3 vectors, each of which is comprised of 3 floats.
    #[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
    #[repr(u32)]
    pub enum Aggregate {
        /// A single scalar value (such as a raw `int` or
        ///   `float` in C).  This is the default.
        SCALAR = 1,
        /// 2 values representing a 2D vector.
        VEC2 = 2,
        /// 3 values representing a 3D vector.
        VEC3 = 3,
        /// 4 values representing a 4D vector.
        VEC4 = 4,
        /// 9 values representing a 3x3 matrix.
        MATRIX33 = 9,
        /// 16 values representing a 4x4 matrix.
        MATRIX44 = 16,
    }

    /// VecSemantics gives hints about what the data represent (for example,
    /// if a spatial vector quantity should transform as a point, direction
    /// vector, or surface normal).
    #[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
    #[repr(u32)]
    pub enum VecSemantics {
        /// No semantic hints.
        NOSEMANTICS = 0,
        /// Color
        COLOR = 1,
        /// Point: a spatial location
        POINT = 2,
        /// Vector: a spatial direction
        VECTOR = 3,
        /// Normal: a surface normal
        NORMAL = 4,
        /// indicates an `int[2]` representing the standard
        ///   4-byte encoding of an SMPTE timecode.
        TIMECODE = 5,
        /// indicates an `int[7]` representing the standard
        ///   28-byte encoding of an SMPTE keycode.
        KEYCODE = 6,
        /// A `VEC2` representing a rational number `val[0] / val[1]`
        RATIONAL = 7,
        /// A `VEC2[2]` or `VEC3[2]` that represents a 2D or 3D bounds (min/max)
        BOX = 8,
    }

    unsafe extern "C++" {
        include!("oiio-sys/include/typedesc.h");

        // TypeDesc
        type BaseType;
        type Aggregate;
        type VecSemantics;
        type TypeDesc = crate::typedesc::TypeDesc;

        /// Construct from a BaseType and optional aggregateness, semantics,
        /// and arrayness.
        fn typedesc_new(
            btype: BaseType,
            agg: Aggregate,
            semantics: VecSemantics,
            arraylen: i32,
        ) -> TypeDesc;

        /// Construct an array of a non-aggregate BaseType.
        fn typedesc_from_basetype_arraylen(btype: BaseType, arraylen: i32) -> TypeDesc;

        /// Construct an array from BaseType, Aggregate, and array length,
        /// with unspecified (or moot) semantic hints.
        fn typedesc_from_basetype_aggregate_arraylen(
            btype: BaseType,
            agg: Aggregate,
            arraylen: i32,
        ) -> TypeDesc;

        /// Construct from a string (e.g., `float[3]`).  If no valid
        /// type could be assembled, set base to `UNKNOWN`.
        ///
        /// Examples:
        ///
        /// ```
        /// # use oiio_sys::typedesc::*;
        /// assert!(typedesc_eq(&typedesc_from_string("int"), &typedesc_new(BaseType::INT32, Aggregate::SCALAR, VecSemantics::NOSEMANTICS, 0)));
        /// assert!(typedesc_eq(&typedesc_from_string("float"), &typedesc_new(BaseType::FLOAT, Aggregate::SCALAR, VecSemantics::NOSEMANTICS, 0)));
        /// assert!(typedesc_eq(&typedesc_from_string("uint16"), &typedesc_new(BaseType::UINT16, Aggregate::SCALAR, VecSemantics::NOSEMANTICS, 0)));
        /// assert!(typedesc_eq(&typedesc_from_string("point"), &typedesc_new(BaseType::FLOAT, Aggregate::VEC3, VecSemantics::POINT, 0)));
        /// ```
        ///
        fn typedesc_from_string(typestring: &str) -> TypeDesc;

        /// Clone constructor.
        fn typedesc_clone(t: &TypeDesc) -> TypeDesc;

        /// Display the name, for printing and whatnot.  For example,
        /// `float`, `int[5]`, `normal`
        fn typedesc_as_str(typedesc: &TypeDesc) -> &str;

        /// Return the number of elements: 1 if not an array, or the array
        /// length. Invalid to call this for arrays of undetermined size.
        fn typedesc_numelements(typedesc: &TypeDesc) -> usize;

        /// Return the number of basetype values: the aggregate count multiplied
        /// by the array length (or 1 if not an array). Invalid to call this
        /// for arrays of undetermined size.
        fn typedesc_basevalues(typedesc: &TypeDesc) -> usize;

        /// Does this TypeDesc describe an array?
        fn typedesc_is_array(typedesc: &TypeDesc) -> bool;

        /// Does this TypeDesc describe an array, but whose length is not
        /// specified?
        fn typedesc_is_unsized_array(typedesc: &TypeDesc) -> bool;

        /// Does this TypeDesc describe an array, whose length is specified?
        fn typedesc_is_sized_array(typedesc: &TypeDesc) -> bool;

        /// Return the size, in bytes, of this type.
        fn typedesc_size(typedesc: &TypeDesc) -> usize;

        /// Return the type of one element, i.e., strip out the array-ness.
        fn typedesc_elementtype(typedesc: &TypeDesc) -> TypeDesc;

        /// Return the size, in bytes, of one element of this type (that is,
        /// ignoring whether it's an array).
        fn typedesc_elementsize(typedesc: &TypeDesc) -> usize;

        /// Return just the underlying C scalar type, i.e., strip out the
        /// array-ness and the aggregateness.
        fn typedesc_scalartype(typedesc: &TypeDesc) -> TypeDesc;

        /// Return the base type size, i.e., stripped of both array-ness
        /// and aggregateness.
        fn typedesc_basesize(typedesc: &TypeDesc) -> usize;

        /// True if it's a floating-point type (versus a fundamentally
        /// integral type or something else like a string).
        fn typedesc_is_floating_point(typedesc: &TypeDesc) -> bool;

        /// True if it's a signed type that allows for negative values.
        fn typedesc_is_signed(typedesc: &TypeDesc) -> bool;

        /// Shortcut: is it UNKNOWN?
        fn typedesc_is_unknown(typedesc: &TypeDesc) -> bool;

        /// Set `typedesc` to the type described in the string.  Return the
        /// length of the part of the string that describes the type.  If
        /// no valid type could be assembled, return 0 and do not modify
        /// `typedesc`.
        fn typedesc_fromstring(typedesc: &mut TypeDesc, typestring: &str) -> usize;

        /// Compare two TypeDesc values for equality.
        fn typedesc_eq(typedesc: &TypeDesc, t: &TypeDesc) -> bool;

        /// Compare two TypeDesc values for inequality.
        fn typedesc_ne(typedesc: &TypeDesc, t: &TypeDesc) -> bool;

        /// Compare a TypeDesc to a basetype (it's the same if it has the
        /// same base type and is not an aggregate or an array).
        fn typedesc_eq_basetype(t: &TypeDesc, b: BaseType) -> bool;

        /// Compare a TypeDesc to a basetype (it's the same if it has the
        /// same base type and is not an aggregate or an array).
        fn basetype_eq_typedesc(b: BaseType, t: &TypeDesc) -> bool;

        /// Compare a TypeDesc to a basetype (it's the same if it has the
        /// same base type and is not an aggregate or an array).
        fn typedesc_ne_basetype(t: &TypeDesc, b: BaseType) -> bool;

        /// Compare a TypeDesc to a basetype (it's the same if it has the
        /// same base type and is not an aggregate or an array).
        fn basetype_ne_typedesc(b: BaseType, t: &TypeDesc) -> bool;

        /// TypeDesc's are equivalent if they are equal, or if their only
        /// inequality is differing vector semantics.
        fn typedesc_equivalent(typedesc: &TypeDesc, b: &TypeDesc) -> bool;

        /// Is this a 2-vector aggregate (of the given type)?
        fn typedesc_is_vec2(typedesc: &TypeDesc, b: BaseType) -> bool;

        /// Is this a 3-vector aggregate (of the given type)?
        fn typedesc_is_vec3(typedesc: &TypeDesc, b: BaseType) -> bool;

        /// Is this a 4-vector aggregate (of the given type)?
        fn typedesc_is_vec4(typedesc: &TypeDesc, b: BaseType) -> bool;

        /// Is this an array of aggregates that represents a 2D bounding box?
        fn typedesc_is_box2(typedesc: &TypeDesc, b: BaseType) -> bool;

        /// Is this an array of aggregates that represents a 3D bounding box?
        fn typedesc_is_box3(typedesc: &TypeDesc, b: BaseType) -> bool;

        /// Demote the type to a non-array
        fn typedesc_unarray(typedesc: &mut TypeDesc) -> ();

        /// Test for lexicographic 'less', comes in handy for lots of STL
        /// containers and algorithms.
        fn typedesc_lt(typedesc: &TypeDesc, x: &TypeDesc) -> bool;

        /// Given base data types of a and b, return a basetype that is a best
        /// guess for one that can handle both without any loss of range or
        /// precision.
        fn typedesc_basetype_merge(a: TypeDesc, b: TypeDesc) -> BaseType;

        /// Given base data types of a and b, return a basetype that is a best
        /// guess for one that can handle both without any loss of range or
        /// precision.
        fn typedesc_basetype_merge_3(a: TypeDesc, b: TypeDesc, c: TypeDesc) -> BaseType;

        /// Given data pointed to by src and described by srctype, copy it to the
        /// memory pointed to by dst and described by dsttype, and return true if a
        /// conversion is possible, false if it is not. If the types are equivalent,
        /// this is a straightforward memory copy. If the types differ, there are
        /// several non-equivalent type conversions that will nonetheless succeed:
        /// * If dsttype is a string (and therefore dst points to a ustring or a
        ///   `char*`): it will always succeed, producing a string akin to calling
        ///   `typedesc_tostring()`.
        /// * If dsttype is int32 or uint32: other integer types will do their best
        ///   (caveat emptor if you mix signed/unsigned). Also a source string will
        ///   convert to int if and only if its characters form a valid integer.
        /// * If dsttype is float: integers and other float types will do
        ///   their best conversion; strings will convert if and only if their
        ///   characters form a valid float number.
        fn typedesc_convert_type(
            srctype: TypeDesc,
            src: &[u8],
            dsttype: TypeDesc,
            dst: &mut [u8],
            n: i32,
        ) -> bool;
    }
}

#[cfg(test)]
mod tests {
    use proptest::prelude::*;

    use super::{Aggregate, BaseType, VecSemantics, *};

    fn basetypes() -> impl Strategy<Value = BaseType> {
        prop_oneof![
            Just(BaseType::UNKNOWN),
            Just(BaseType::NONE),
            Just(BaseType::UINT8),
            Just(BaseType::INT8),
            Just(BaseType::UINT16),
            Just(BaseType::INT16),
            Just(BaseType::UINT32),
            Just(BaseType::INT32),
            Just(BaseType::UINT64),
            Just(BaseType::INT64),
            Just(BaseType::HALF),
            Just(BaseType::FLOAT),
            Just(BaseType::DOUBLE),
            Just(BaseType::STRING),
            Just(BaseType::PTR),
            Just(BaseType::USTRINGHASH),
            Just(BaseType::LASTBASE),
        ]
    }

    fn aggregates() -> impl Strategy<Value = Aggregate> {
        prop_oneof![
            Just(Aggregate::SCALAR),
            Just(Aggregate::VEC2),
            Just(Aggregate::VEC3),
            Just(Aggregate::VEC4),
            Just(Aggregate::MATRIX33),
            Just(Aggregate::MATRIX44),
        ]
    }

    fn vecsemantics() -> impl Strategy<Value = VecSemantics> {
        prop_oneof![
            Just(VecSemantics::NOSEMANTICS),
            Just(VecSemantics::COLOR),
            Just(VecSemantics::POINT),
            Just(VecSemantics::VECTOR),
            Just(VecSemantics::NORMAL),
            Just(VecSemantics::TIMECODE),
            Just(VecSemantics::KEYCODE),
            Just(VecSemantics::RATIONAL),
            Just(VecSemantics::BOX),
        ]
    }

    prop_compose! {
        fn typedescs()(
            basetype in basetypes(),
            aggregate in aggregates(),
            vecsemantics in vecsemantics(),
            arraylen in i32::MIN..i32::MAX
        ) -> TypeDesc {
            TypeDesc {
                basetype: basetype.repr as u8,
                aggregate: aggregate.repr as u8,
                vecsemantics: vecsemantics.repr as u8,
                _reserved: 0,
                arraylen,
            }
        }
    }

    fn from_string_strategy() -> impl Strategy<Value = (TypeDesc, &'static str)> {
        basetypes().prop_filter_map("BaseType::LASTBASE is unsupported", |base_type| {
            let base_type_str = match base_type {
                BaseType::UNKNOWN => "unknown",
                BaseType::NONE => "void",
                BaseType::UINT8 => "uint8",
                BaseType::INT8 => "int8",
                BaseType::UINT16 => "uint16",
                BaseType::INT16 => "int16",
                BaseType::UINT32 => "uint",
                BaseType::INT32 => "int",
                BaseType::UINT64 => "uint64",
                BaseType::INT64 => "int64",
                BaseType::HALF => "half",
                BaseType::FLOAT => "float",
                BaseType::DOUBLE => "double",
                BaseType::STRING => "string",
                BaseType::PTR => "pointer",
                BaseType::USTRINGHASH => "ustringhash",
                BaseType::LASTBASE => return None,
                _ => unreachable!(),
            };

            Some((typedesc_from_basetype_arraylen(base_type, 0), base_type_str))
        })
    }

    proptest! {
        #[test]
        fn test_typedesc_new_success(
            btype in basetypes(),
            agg in aggregates(),
            semantics in vecsemantics(),
            arraylen in i32::MIN..i32::MAX
        ) {
            let result = typedesc_new(btype, agg, semantics, arraylen);

            prop_assert_eq!(result.basetype, btype.repr as u8);
            prop_assert_eq!(result.aggregate, agg.repr as u8);
            prop_assert_eq!(result.vecsemantics, semantics.repr as u8);
            prop_assert_eq!(result.arraylen, arraylen);
        }

        #[test]
        fn test_typedesc_from_basetype_arraylen_success(
            btype in basetypes(),
            arraylen in i32::MIN..i32::MAX
        ) {
            let result = typedesc_from_basetype_arraylen(btype, arraylen);

            prop_assert_eq!(result.basetype, btype.repr as u8);
            prop_assert_eq!(result.aggregate, Aggregate::SCALAR.repr as u8);
            prop_assert_eq!(result.vecsemantics, VecSemantics::NOSEMANTICS.repr as u8);
            prop_assert_eq!(result.arraylen, arraylen);
        }

        #[test]
        fn test_typedesc_from_basetype_aggregate_arraylen_success(
            btype in basetypes(),
            agg in aggregates(),
            arraylen in any::<i32>()
        ) {
            let result = typedesc_from_basetype_aggregate_arraylen(btype, agg, arraylen);

            prop_assert_eq!(result.basetype, btype.repr as u8);
            prop_assert_eq!(result.aggregate, agg.repr as u8);
            prop_assert_eq!(result.vecsemantics, VecSemantics::NOSEMANTICS.repr as u8);
            prop_assert_eq!(result.arraylen, arraylen);
        }

        #[test]
        fn test_typedesc_from_string_success(
            (expected_type_desc, input_str) in from_string_strategy()
        ) {
            let result = typedesc_from_string(&input_str);

            prop_assert_eq!(result.basetype, expected_type_desc.basetype);
            prop_assert_eq!(result.aggregate, expected_type_desc.aggregate);
            prop_assert_eq!(result.vecsemantics, expected_type_desc.vecsemantics);
            prop_assert_eq!(result.arraylen, expected_type_desc.arraylen);
        }

        #[test]
        fn test_typedesc_clone_success(
            type_desc in typedescs(),
        ) {
            let result = typedesc_clone(&type_desc);

            prop_assert_eq!(result.basetype, type_desc.basetype);
            prop_assert_eq!(result.aggregate, type_desc.aggregate);
            prop_assert_eq!(result.vecsemantics, type_desc.vecsemantics);
            prop_assert_eq!(result.arraylen, type_desc.arraylen);
        }

        #[test]
        fn test_typedesc_as_str_success(
            (input_type_desc, expected_str) in from_string_strategy()
        ) {
            let result = typedesc_as_str(&input_type_desc);

            prop_assert_eq!(result, expected_str);
        }

        #[test]
        fn test_typedesc_numelements_success(
            btype in basetypes(),
            agg in aggregates(),
            semantics in vecsemantics(),
            arraylen in 1..i32::MAX
        ) {
            let typedesc = typedesc_new(btype, agg, semantics, arraylen);
            let result = typedesc_numelements(&typedesc);

            prop_assert_eq!(result, arraylen as usize);
        }

        #[test]
        fn test_typedesc_basevalues_success(
            btype in basetypes(),
            agg in aggregates(),
            semantics in vecsemantics(),
            arraylen in 1..i32::MAX
        ) {
            let typedesc = typedesc_new(btype, agg, semantics, arraylen);
            let result = typedesc_basevalues(&typedesc);

            prop_assert_eq!(result, arraylen as usize * agg.repr as usize);
        }

        #[test]
        fn test_typedesc_is_array_success(
            btype in basetypes(),
            agg in aggregates(),
            semantics in vecsemantics(),
            arraylen in -2..2
        ) {
            let typedesc = typedesc_new(btype, agg, semantics, arraylen);
            let result = typedesc_is_array(&typedesc);

            prop_assert_eq!(result, arraylen != 0);
        }

        #[test]
        fn test_typedesc_is_unsized_array_success(
            btype in basetypes(),
            agg in aggregates(),
            semantics in vecsemantics(),
            arraylen in -2..2
        ) {
            let typedesc = typedesc_new(btype, agg, semantics, arraylen);
            let result = typedesc_is_unsized_array(&typedesc);

            prop_assert_eq!(result, arraylen < 0);
        }

        #[test]
        fn test_typedesc_is_sized_array_success(
            btype in basetypes(),
            agg in aggregates(),
            semantics in vecsemantics(),
            arraylen in -2..2
        ) {
            let typedesc = typedesc_new(btype, agg, semantics, arraylen);
            let result = typedesc_is_sized_array(&typedesc);

            prop_assert_eq!(result, arraylen > 0);
        }

        #[test]
        fn test_typedesc_size_success(
            btype in basetypes(),
            agg in aggregates(),
            semantics in vecsemantics(),
            arraylen in 1..2
        ) {
            let typedesc = typedesc_new(btype, agg, semantics, arraylen);
            let result = typedesc_size(&typedesc);

            prop_assert_eq!(result, arraylen as usize * typedesc_elementsize(&typedesc));
        }

        #[test]
        fn test_typedesc_elementtype_success(
            btype in basetypes(),
            agg in aggregates(),
            semantics in vecsemantics(),
            arraylen in i32::MIN..i32::MAX
        ) {
            let typedesc = typedesc_new(btype, agg, semantics, arraylen);
            let result = typedesc_elementtype(&typedesc);

            prop_assert_eq!(result.basetype, btype.repr as u8);
            prop_assert_eq!(result.aggregate, agg.repr as u8);
            prop_assert_eq!(result.vecsemantics, semantics.repr as u8);
            prop_assert_eq!(result.arraylen, 0);
        }

        #[test]
        fn test_typedesc_elementsize_success(
            btype in basetypes(),
            agg in aggregates(),
            semantics in vecsemantics(),
            arraylen in i32::MIN..i32::MAX
        ) {
            let typedesc = typedesc_new(btype, agg, semantics, arraylen);
            let result = typedesc_elementsize(&typedesc);

            prop_assert_eq!(result, agg.repr as usize * typedesc_basesize(&typedesc));
        }

        #[test]
        fn test_typedesc_scalartype_success(
            btype in basetypes(),
            agg in aggregates(),
            semantics in vecsemantics(),
            arraylen in i32::MIN..i32::MAX
        ) {
            let typedesc = typedesc_new(btype, agg, semantics, arraylen);
            let result = typedesc_scalartype(&typedesc);

            prop_assert_eq!(result.basetype, btype.repr as u8);
            prop_assert_eq!(result.aggregate, Aggregate::SCALAR.repr as u8);
            prop_assert_eq!(result.vecsemantics, VecSemantics::NOSEMANTICS.repr as u8);
            prop_assert_eq!(result.arraylen, 0);
        }

        #[test]
        fn test_typedesc_basesize_success(
            btype in basetypes().prop_filter("BaseType::LASTBASE not supported", |btype| !matches!(*btype, crate::typedesc::BaseType::LASTBASE)),
            agg in aggregates(),
            semantics in vecsemantics(),
            arraylen in i32::MIN..i32::MAX
        ) {
            let typedesc = typedesc_new(btype, agg, semantics, arraylen);
            let result = typedesc_basesize(&typedesc);

            let expected = match btype {
                BaseType::UNKNOWN => 0,
                BaseType::NONE => 0,
                BaseType::UINT8 => size_of::<u8>(),
                BaseType::INT8 => size_of::<i8>(),
                BaseType::UINT16 => size_of::<u16>(),
                BaseType::INT16 => size_of::<i16>(),
                BaseType::UINT32 => size_of::<u32>(),
                BaseType::INT32 => size_of::<i32>(),
                BaseType::UINT64 => size_of::<u64>(),
                BaseType::INT64 => size_of::<i64>(),
                BaseType::HALF => size_of::<f32>() / 2,
                BaseType::FLOAT => size_of::<f32>(),
                BaseType::DOUBLE => size_of::<f64>(),
                BaseType::STRING => size_of::<*const std::ffi::c_char>(),
                BaseType::PTR => size_of::<*const std::ffi::c_char>(),
                BaseType::USTRINGHASH => 8, // TODO: Get the size of the UStringHash type.
                BaseType::LASTBASE => unreachable!(),
                _ => unreachable!(),
            };

            prop_assert_eq!(result, expected);
        }

        #[test]
        fn test_typedesc_is_floating_point_success(
            btype in basetypes(),
            agg in aggregates(),
            semantics in vecsemantics(),
            arraylen in i32::MIN..i32::MAX
        ) {
            let typedesc = typedesc_new(btype, agg, semantics, arraylen);
            let result = typedesc_is_floating_point(&typedesc);

            prop_assert_eq!(result, matches!(btype, BaseType::HALF | BaseType::FLOAT | BaseType::DOUBLE));
        }

        #[test]
        fn test_typedesc_is_unknown_success(
            btype in basetypes(),
            agg in aggregates(),
            semantics in vecsemantics(),
            arraylen in i32::MIN..i32::MAX
        ) {
            let typedesc = typedesc_new(btype, agg, semantics, arraylen);
            let result = typedesc_is_unknown(&typedesc);

            prop_assert_eq!(result, matches!(btype, BaseType::UNKNOWN));
        }

        #[test]
        fn test_typedesc_fromstring_success(
            btype in basetypes(),
            agg in aggregates(),
            semantics in vecsemantics(),
            arraylen in i32::MIN..i32::MAX,
            (expected_type_desc, input_str) in from_string_strategy()
        ) {
            let mut typedesc = typedesc_new(btype, agg, semantics, arraylen);
            typedesc_fromstring(&mut typedesc, &input_str);

            prop_assert_eq!(typedesc.basetype, expected_type_desc.basetype);
            prop_assert_eq!(typedesc.aggregate, expected_type_desc.aggregate);
            prop_assert_eq!(typedesc.vecsemantics, expected_type_desc.vecsemantics);
            prop_assert_eq!(typedesc.arraylen, expected_type_desc.arraylen);
        }

        #[test]
        fn test_typedesc_eq_success(
            btype_a in basetypes(),
            agg_a in aggregates(),
            semantics_a in vecsemantics(),
            arraylen_a in i32::MIN..i32::MAX,
            btype_b in basetypes(),
            agg_b in aggregates(),
            semantics_b in vecsemantics(),
            arraylen_b in i32::MIN..i32::MAX,
        ) {
            let typedesc_a = typedesc_new(btype_a, agg_a, semantics_a, arraylen_a);
            let typedesc_b = typedesc_new(btype_b, agg_b, semantics_b, arraylen_b);

            if btype_a == btype_b && agg_a == agg_b && semantics_a == semantics_b && arraylen_a == arraylen_b {
                prop_assert!(typedesc_eq(&typedesc_a, &typedesc_b));
            } else {
                prop_assert!(!typedesc_eq(&typedesc_a, &typedesc_b));
            }
        }

        #[test]
        fn test_typedesc_ne_success(
            btype_a in basetypes(),
            agg_a in aggregates(),
            semantics_a in vecsemantics(),
            arraylen_a in i32::MIN..i32::MAX,
            btype_b in basetypes(),
            agg_b in aggregates(),
            semantics_b in vecsemantics(),
            arraylen_b in i32::MIN..i32::MAX,
        ) {
            let typedesc_a = typedesc_new(btype_a, agg_a, semantics_a, arraylen_a);
            let typedesc_b = typedesc_new(btype_b, agg_b, semantics_b, arraylen_b);

            if btype_a == btype_b && agg_a == agg_b && semantics_a == semantics_b && arraylen_a == arraylen_b {
                prop_assert!(!typedesc_ne(&typedesc_a, &typedesc_b));
            } else {
                prop_assert!(typedesc_ne(&typedesc_a, &typedesc_b));
            }
        }

        #[test]
        fn test_typedesc_eq_basetype_success(
            btype_a in basetypes(),
            agg_a in aggregates(),
            semantics_a in vecsemantics(),
            arraylen_a in -2..2,
            btype_b in basetypes(),
        ) {
            let typedesc_a = typedesc_new(btype_a, agg_a, semantics_a, arraylen_a);

            if btype_a == btype_b && agg_a == Aggregate::SCALAR && !typedesc_is_array(&typedesc_a) {
                prop_assert!(typedesc_eq_basetype(&typedesc_a, btype_b));
                prop_assert!(basetype_eq_typedesc(btype_b, &typedesc_a));
            } else {
                prop_assert!(!typedesc_eq_basetype(&typedesc_a, btype_b));
                prop_assert!(!basetype_eq_typedesc(btype_b, &typedesc_a));
            }
        }

        #[test]
        fn test_typedesc_ne_basetype_success(
            btype_a in basetypes(),
            agg_a in aggregates(),
            semantics_a in vecsemantics(),
            arraylen_a in -2..2,
            btype_b in basetypes(),
        ) {
            let typedesc_a = typedesc_new(btype_a, agg_a, semantics_a, arraylen_a);

            if btype_a == btype_b && agg_a == Aggregate::SCALAR && !typedesc_is_array(&typedesc_a) {
                prop_assert!(!typedesc_ne_basetype(&typedesc_a, btype_b));
                prop_assert!(!basetype_ne_typedesc(btype_b, &typedesc_a));
            } else {
                prop_assert!(typedesc_ne_basetype(&typedesc_a, btype_b));
                prop_assert!(basetype_ne_typedesc(btype_b, &typedesc_a));
            }
        }

        #[test]
        fn test_typedesc_equivalent_success(
            btype_a in basetypes(),
            agg_a in aggregates(),
            semantics_a in vecsemantics(),
            btype_b in basetypes(),
            agg_b in aggregates(),
            semantics_b in vecsemantics(),
        ) {
            let typedesc_a = typedesc_new(btype_a, agg_a, semantics_a, 0);
            let typedesc_b = typedesc_new(btype_b, agg_b, semantics_b, 0);

            if btype_a == btype_b && agg_a == agg_b {
                prop_assert!(typedesc_equivalent(&typedesc_a, &typedesc_b));
            } else {
                prop_assert!(!typedesc_equivalent(&typedesc_a, &typedesc_b));
            }
        }

        #[test]
        fn test_typedesc_is_vec2_success(
            btype in basetypes(),
            btype_b in basetypes(),
            agg in aggregates(),
            semantics in vecsemantics(),
            arraylen in -2..2
        ) {
            let result = typedesc_new(btype, agg, semantics, arraylen);

            if result.aggregate == Aggregate::VEC2.repr as u8 && result.basetype == btype_b.repr as u8 && !typedesc_is_array(&result) {
                prop_assert!(typedesc_is_vec2(&result, btype_b));
            } else {
                prop_assert!(!typedesc_is_vec2(&result, btype_b));

            }
        }

        #[test]
        fn test_typedesc_is_vec3_success(
            btype in basetypes(),
            btype_b in basetypes(),
            agg in aggregates(),
            semantics in vecsemantics(),
            arraylen in -2..2
        ) {
            let result = typedesc_new(btype, agg, semantics, arraylen);

            if result.aggregate == Aggregate::VEC3.repr as u8 && result.basetype == btype_b.repr as u8 && !typedesc_is_array(&result) {
                prop_assert!(typedesc_is_vec3(&result, btype_b));
            } else {
                prop_assert!(!typedesc_is_vec3(&result, btype_b));

            }
        }

        #[test]
        fn test_typedesc_is_vec4_success(
            btype in basetypes(),
            btype_b in basetypes(),
            agg in aggregates(),
            semantics in vecsemantics(),
            arraylen in -2..2
        ) {
            let result = typedesc_new(btype, agg, semantics, arraylen);

            if result.aggregate == Aggregate::VEC4.repr as u8 && result.basetype == btype_b.repr as u8 && !typedesc_is_array(&result) {
                prop_assert!(typedesc_is_vec4(&result, btype_b));
            } else {
                prop_assert!(!typedesc_is_vec4(&result, btype_b));

            }
        }

        #[test]
        fn test_typedesc_is_box2_success(
            btype in basetypes(),
            btype_b in basetypes(),
            agg in aggregates(),
            semantics in vecsemantics(),
            arraylen in -2..4
        ) {
            let result = typedesc_new(btype, agg, semantics, arraylen);

            if result.aggregate == Aggregate::VEC2.repr as u8 && result.vecsemantics == VecSemantics::BOX.repr as u8 && result.basetype == btype_b.repr as u8 && arraylen == 2 {
                prop_assert!(typedesc_is_box2(&result, btype_b));
            } else {
                prop_assert!(!typedesc_is_box2(&result, btype_b));

            }
        }

        #[test]
        fn test_typedesc_is_box3_success(
            btype in basetypes(),
            btype_b in basetypes(),
            agg in aggregates(),
            semantics in vecsemantics(),
            arraylen in -2..4
        ) {
            let result = typedesc_new(btype, agg, semantics, arraylen);

            if result.aggregate == Aggregate::VEC3.repr as u8 && result.vecsemantics == VecSemantics::BOX.repr as u8 && result.basetype == btype_b.repr as u8 && arraylen == 2 {
                prop_assert!(typedesc_is_box3(&result, btype_b));
            } else {
                prop_assert!(!typedesc_is_box3(&result, btype_b));

            }
        }

        #[test]
        fn test_typedesc_unarray_success(
            btype in basetypes(),
            agg in aggregates(),
            semantics in vecsemantics(),
            arraylen in i32::MIN..i32::MAX
        ) {
            let mut result = typedesc_new(btype, agg, semantics, arraylen);
            typedesc_unarray(&mut result);

            prop_assert_eq!(result.arraylen, 0);
        }

        #[test]
        fn test_typedesc_lt_success(
            btype_a in basetypes(),
            agg_a in aggregates(),
            semantics_a in vecsemantics(),
            arraylen_a in i32::MIN..i32::MAX,
            btype_b in basetypes(),
            agg_b in aggregates(),
            semantics_b in vecsemantics(),
            arraylen_b in i32::MIN..i32::MAX,
        ) {
            let typedesc_a = typedesc_new(btype_a, agg_a, semantics_a, arraylen_a);
            let typedesc_b = typedesc_new(btype_b, agg_b, semantics_b, arraylen_b);

            if btype_a != btype_b {
                prop_assert_eq!(typedesc_lt(&typedesc_a, &typedesc_b), btype_a < btype_b);
            } else if agg_a != agg_b {
                prop_assert_eq!(typedesc_lt(&typedesc_a, &typedesc_b), agg_a < agg_b);
            } else if arraylen_a != arraylen_b {
                prop_assert_eq!(typedesc_lt(&typedesc_a, &typedesc_b), arraylen_a < arraylen_b);
            } else if semantics_a != semantics_b {
                prop_assert_eq!(typedesc_lt(&typedesc_a, &typedesc_b), semantics_a < semantics_b);
            } else {
                prop_assert_eq!(typedesc_lt(&typedesc_a, &typedesc_b), false);
            }
        }

        #[test]
        fn test_typedesc_basetype_merge_success(
            mut btype_a in basetypes(),
            agg_a in aggregates(),
            semantics_a in vecsemantics(),
            arraylen_a in i32::MIN..i32::MAX,
            mut btype_b in basetypes(),
            agg_b in aggregates(),
            semantics_b in vecsemantics(),
            arraylen_b in i32::MIN..i32::MAX,
        ) {
            let typedesc_a = typedesc_new(btype_a, agg_a, semantics_a, arraylen_a);
            let typedesc_b = typedesc_new(btype_b, agg_b, semantics_b, arraylen_b);
            let result = typedesc_basetype_merge(typedesc_a, typedesc_b);

            if btype_a == btype_b {
                prop_assert_eq!(result, btype_a);
                prop_assert_eq!(result, btype_b);
            } else if btype_a == BaseType::UNKNOWN {
                prop_assert_eq!(result, btype_b);
            } else if btype_b == BaseType::UNKNOWN {
                prop_assert_eq!(result, btype_a);
            } else {
                let size_a = typedesc_size(&typedesc_new(btype_a, Aggregate::SCALAR, VecSemantics::NOSEMANTICS, 0));
                let size_b = typedesc_size(&typedesc_new(btype_b, Aggregate::SCALAR, VecSemantics::NOSEMANTICS, 0));

                if size_a < size_b {
                    std::mem::swap(&mut btype_a, &mut btype_b);
                }

                if btype_a == BaseType::DOUBLE || btype_a == BaseType::FLOAT {
                    prop_assert_eq!(result, btype_a);
                } else if btype_a == BaseType::UINT32 && (btype_b == BaseType::UINT16 || btype_b == BaseType::UINT8) {
                    prop_assert_eq!(result, btype_a);
                } else if btype_a == BaseType::INT32 && (btype_b == BaseType::INT16 || btype_b == BaseType::INT8 || btype_b == BaseType::UINT16 || btype_b == BaseType::UINT8) {
                    prop_assert_eq!(result, btype_a);
                } else if (btype_a == BaseType::UINT16 || btype_a == BaseType::HALF) && btype_b == BaseType::UINT8 {
                    prop_assert_eq!(result, btype_a);
                } else if (btype_a == BaseType::INT16 || btype_a == BaseType::HALF) && (btype_b == BaseType::INT8 || btype_b == BaseType::UINT8) {
                    prop_assert_eq!(result, btype_a);
                } else {
                    prop_assert_eq!(result, BaseType::FLOAT);
                }
            }
        }

        #[test]
        fn test_typedesc_basetype_merge_3_success(
            btype_a in basetypes(),
            agg_a in aggregates(),
            semantics_a in vecsemantics(),
            arraylen_a in i32::MIN..i32::MAX,
            btype_b in basetypes(),
            agg_b in aggregates(),
            semantics_b in vecsemantics(),
            arraylen_b in i32::MIN..i32::MAX,
            mut btype_c in basetypes(),
            agg_c in aggregates(),
            semantics_c in vecsemantics(),
            arraylen_c in i32::MIN..i32::MAX,
        ) {
            let typedesc_a = typedesc_new(btype_a, agg_a, semantics_a, arraylen_a);
            let typedesc_b = typedesc_new(btype_b, agg_b, semantics_b, arraylen_b);
            let typedesc_c = typedesc_new(btype_c, agg_c, semantics_c, arraylen_c);
            let result = typedesc_basetype_merge_3(typedesc_a, typedesc_b, typedesc_c);
            let mut btype_inner = typedesc_basetype_merge(typedesc_a, typedesc_b);

            if btype_inner == btype_c {
                prop_assert_eq!(result, btype_inner);
                prop_assert_eq!(result, btype_c);
            } else if btype_inner == BaseType::UNKNOWN {
                prop_assert_eq!(result, btype_c);
            } else if btype_c == BaseType::UNKNOWN {
                prop_assert_eq!(result, btype_inner);
            } else {
                let size_a = typedesc_size(&typedesc_new(btype_inner, Aggregate::SCALAR, VecSemantics::NOSEMANTICS, 0));
                let size_b = typedesc_size(&typedesc_new(btype_c, Aggregate::SCALAR, VecSemantics::NOSEMANTICS, 0));

                if size_a < size_b {
                    std::mem::swap(&mut btype_inner, &mut btype_c);
                }

                if btype_inner == BaseType::DOUBLE || btype_inner == BaseType::FLOAT {
                    prop_assert_eq!(result, btype_inner);
                } else if btype_inner == BaseType::UINT32 && (btype_c == BaseType::UINT16 || btype_c == BaseType::UINT8) {
                    prop_assert_eq!(result, btype_inner);
                } else if btype_inner == BaseType::INT32 && (btype_c == BaseType::INT16 || btype_c == BaseType::INT8 || btype_c == BaseType::UINT16 || btype_c == BaseType::UINT8) {
                    prop_assert_eq!(result, btype_inner);
                } else if (btype_inner == BaseType::UINT16 || btype_inner == BaseType::HALF) && btype_c == BaseType::UINT8 {
                    prop_assert_eq!(result, btype_inner);
                } else if (btype_inner == BaseType::INT16 || btype_inner == BaseType::HALF) && (btype_c == BaseType::INT8 || btype_c == BaseType::UINT8) {
                    prop_assert_eq!(result, btype_inner);
                } else {
                    prop_assert_eq!(result, BaseType::FLOAT);
                }
            }
        }

        #[test]
        fn test_typedesc_convert_type_success(
            value_in in i32::MIN..i32::MAX,
        ) {
            let mut value_out = 0.0f32.to_ne_bytes();
            let src_type = typedesc_new(BaseType::INT32, Aggregate::SCALAR, VecSemantics::NOSEMANTICS, 0);
            let dst_type = typedesc_new(BaseType::FLOAT, Aggregate::SCALAR, VecSemantics::NOSEMANTICS, 0);

            prop_assert!(typedesc_convert_type(src_type, &value_in.to_ne_bytes(), dst_type, &mut value_out, 1));
            prop_assert_eq!(f32::from_ne_bytes(value_out), value_in as f32);
        }
    }
}
