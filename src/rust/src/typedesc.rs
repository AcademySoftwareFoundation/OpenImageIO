// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

use crate::sys;

/// BaseType is a simple enum describing the base data types that
/// correspond (mostly) to the C/C++ built-in types.
#[derive(Debug, Clone, Copy, Default)]
#[cfg_attr(test, derive(proptest_derive::Arbitrary))]
pub enum BaseType {
    #[default]
    /// unknown type
    Unknown,
    /// void/no type
    None,
    /// 8-bit unsigned int values ranging from 0..255,
    /// (C/C++ `unsigned char`).
    UInt8,
    /// 8-bit int values ranging from -128..127,
    ///   (C/C++ `char`).
    Int8,
    /// 16-bit int values ranging from 0..65535,
    ///   (C/C++ `unsigned short`).
    UInt16,
    /// 16-bit int values ranging from -32768..32767,
    ///   (C/C++ `short`).
    Int16,
    /// 32-bit unsigned int values (C/C++ `unsigned int`).
    UInt32,
    /// signed 32-bit int values (C/C++ `int`).
    Int32,
    /// 64-bit unsigned int values (C/C++
    ///   `unsigned long long` on most architectures).
    UInt64,
    /// signed 64-bit int values (C/C++ `long long`
    ///   on most architectures).
    Int64,
    /// 16-bit IEEE floating point values (OpenEXR `half`).
    Half,
    /// 32-bit IEEE floating point values, (C/C++ `float`).
    Float,
    /// 64-bit IEEE floating point values, (C/C++ `double`).
    Double,
    /// Character string.
    String,
    /// A pointer value.
    Ptr,
    /// A uint64 that is the hash of a ustring.
    UStringHash,
    LastBase,
}

impl PartialEq for BaseType {
    fn eq(&self, other: &Self) -> bool {
        sys::typedesc::BaseType::from(*self) == sys::typedesc::BaseType::from(*other)
    }
}

impl Eq for BaseType {}

impl PartialOrd for BaseType {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for BaseType {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        sys::typedesc::BaseType::from(*self).cmp(&sys::typedesc::BaseType::from(*other))
    }
}

impl PartialEq<TypeDesc> for BaseType {
    /// Compare a TypeDesc to a basetype (it's the same if it has the
    /// same base type and is not an aggregate or an array).
    fn eq(&self, other: &TypeDesc) -> bool {
        sys::typedesc::basetype_eq_typedesc((*self).into(), &(*other).into())
    }

    /// Compare a TypeDesc to a basetype (it's the same if it has the
    /// same base type and is not an aggregate or an array).
    fn ne(&self, other: &TypeDesc) -> bool {
        sys::typedesc::basetype_ne_typedesc((*self).into(), &(*other).into())
    }
}

impl From<BaseType> for sys::typedesc::BaseType {
    fn from(value: BaseType) -> Self {
        match value {
            BaseType::Unknown => Self::UNKNOWN,
            BaseType::None => Self::NONE,
            BaseType::UInt8 => Self::UINT8,
            BaseType::Int8 => Self::INT8,
            BaseType::UInt16 => Self::UINT16,
            BaseType::Int16 => Self::INT16,
            BaseType::UInt32 => Self::UINT32,
            BaseType::Int32 => Self::INT32,
            BaseType::UInt64 => Self::UINT64,
            BaseType::Int64 => Self::INT64,
            BaseType::Half => Self::HALF,
            BaseType::Float => Self::FLOAT,
            BaseType::Double => Self::DOUBLE,
            BaseType::String => Self::STRING,
            BaseType::Ptr => Self::PTR,
            BaseType::UStringHash => Self::USTRINGHASH,
            BaseType::LastBase => Self::LASTBASE,
        }
    }
}

impl From<sys::typedesc::BaseType> for BaseType {
    fn from(value: sys::typedesc::BaseType) -> Self {
        match value {
            sys::typedesc::BaseType::UNKNOWN => Self::Unknown,
            sys::typedesc::BaseType::NONE => Self::None,
            sys::typedesc::BaseType::UINT8 => Self::UInt8,
            sys::typedesc::BaseType::INT8 => Self::Int8,
            sys::typedesc::BaseType::UINT16 => Self::UInt16,
            sys::typedesc::BaseType::INT16 => Self::Int16,
            sys::typedesc::BaseType::UINT32 => Self::UInt32,
            sys::typedesc::BaseType::INT32 => Self::Int32,
            sys::typedesc::BaseType::UINT64 => Self::UInt64,
            sys::typedesc::BaseType::INT64 => Self::Int64,
            sys::typedesc::BaseType::HALF => Self::Half,
            sys::typedesc::BaseType::FLOAT => Self::Float,
            sys::typedesc::BaseType::DOUBLE => Self::Double,
            sys::typedesc::BaseType::STRING => Self::String,
            sys::typedesc::BaseType::PTR => Self::Ptr,
            sys::typedesc::BaseType::USTRINGHASH => Self::UStringHash,
            sys::typedesc::BaseType::LASTBASE => Self::LastBase,
            _ => unreachable!(),
        }
    }
}

impl From<BaseType> for u32 {
    fn from(value: BaseType) -> Self {
        sys::typedesc::BaseType::from(value).repr
    }
}

/// Aggregate describes whether our TypeDesc is a simple scalar of one
/// of the BaseType's, or one of several simple aggregates.
///
/// Note that aggregates and arrays are different. A `TypeDesc(FLOAT,3)`
/// is an array of three floats, a `TypeDesc(FLOAT,VEC3)` is a single
/// 3-component vector comprised of floats, and `TypeDesc(FLOAT,3,VEC3)`
/// is an array of 3 vectors, each of which is comprised of 3 floats.
#[derive(Debug, Clone, Copy)]
#[cfg_attr(test, derive(proptest_derive::Arbitrary))]
pub enum Aggregate {
    /// A single scalar value (such as a raw `int` or
    ///   `float` in C).  This is the default.
    Scalar,
    /// 2 values representing a 2D vector.
    Vec2,
    /// 3 values representing a 3D vector.
    Vec3,
    /// 4 values representing a 4D vector.
    Vec4,
    /// 9 values representing a 3x3 matrix.
    Matrix33,
    /// 16 values representing a 4x4 matrix.
    Matrix44,
}

impl PartialEq for Aggregate {
    fn eq(&self, other: &Self) -> bool {
        sys::typedesc::Aggregate::from(*self) == sys::typedesc::Aggregate::from(*other)
    }
}

impl Eq for Aggregate {}

impl PartialOrd for Aggregate {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for Aggregate {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        sys::typedesc::Aggregate::from(*self).cmp(&sys::typedesc::Aggregate::from(*other))
    }
}

impl From<Aggregate> for sys::typedesc::Aggregate {
    fn from(value: Aggregate) -> Self {
        match value {
            Aggregate::Scalar => Self::SCALAR,
            Aggregate::Vec2 => Self::VEC2,
            Aggregate::Vec3 => Self::VEC3,
            Aggregate::Vec4 => Self::VEC4,
            Aggregate::Matrix33 => Self::MATRIX33,
            Aggregate::Matrix44 => Self::MATRIX44,
        }
    }
}

impl From<sys::typedesc::Aggregate> for Aggregate {
    fn from(value: sys::typedesc::Aggregate) -> Self {
        match value {
            sys::typedesc::Aggregate::SCALAR => Self::Scalar,
            sys::typedesc::Aggregate::VEC2 => Self::Vec2,
            sys::typedesc::Aggregate::VEC3 => Self::Vec3,
            sys::typedesc::Aggregate::VEC4 => Self::Vec4,
            sys::typedesc::Aggregate::MATRIX33 => Self::Matrix33,
            sys::typedesc::Aggregate::MATRIX44 => Self::Matrix44,
            _ => unreachable!(),
        }
    }
}

impl From<Aggregate> for u32 {
    fn from(value: Aggregate) -> Self {
        sys::typedesc::Aggregate::from(value).repr
    }
}

/// VecSemantics gives hints about what the data represent (for example,
/// if a spatial vector quantity should transform as a point, direction
/// vector, or surface normal).
#[derive(Debug, Clone, Copy, Default)]
#[cfg_attr(test, derive(proptest_derive::Arbitrary))]
pub enum VecSemantics {
    #[default]
    /// No semantic hints.
    NoSemantics,
    /// Color
    Color,
    /// Point: a spatial location
    Point,
    /// Vector: a spatial direction
    Vector,
    /// Normal: a surface normal
    Normal,
    /// indicates an `int[2]` representing the standard
    ///   4-byte encoding of an SMPTE timecode.
    Timecode,
    /// indicates an `int[7]` representing the standard
    ///   28-byte encoding of an SMPTE keycode.
    Keycode,
    /// A `Vec2` representing a rational number `val[0] / val[1]`
    Rational,
    /// A `Vec2[2]` or `Vec3[2]` that represents a 2D or 3D bounds (min/max)
    Box,
}

impl PartialEq for VecSemantics {
    fn eq(&self, other: &Self) -> bool {
        sys::typedesc::VecSemantics::from(*self) == sys::typedesc::VecSemantics::from(*other)
    }
}

impl Eq for VecSemantics {}

impl PartialOrd for VecSemantics {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for VecSemantics {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        sys::typedesc::VecSemantics::from(*self).cmp(&sys::typedesc::VecSemantics::from(*other))
    }
}

impl From<VecSemantics> for sys::typedesc::VecSemantics {
    fn from(value: VecSemantics) -> Self {
        match value {
            VecSemantics::NoSemantics => Self::NOSEMANTICS,
            VecSemantics::Color => Self::COLOR,
            VecSemantics::Point => Self::POINT,
            VecSemantics::Vector => Self::VECTOR,
            VecSemantics::Normal => Self::NORMAL,
            VecSemantics::Timecode => Self::TIMECODE,
            VecSemantics::Keycode => Self::KEYCODE,
            VecSemantics::Rational => Self::RATIONAL,
            VecSemantics::Box => Self::BOX,
        }
    }
}

impl From<sys::typedesc::VecSemantics> for VecSemantics {
    fn from(value: sys::typedesc::VecSemantics) -> Self {
        match value {
            sys::typedesc::VecSemantics::NOSEMANTICS => Self::NoSemantics,
            sys::typedesc::VecSemantics::COLOR => Self::Color,
            sys::typedesc::VecSemantics::POINT => Self::Point,
            sys::typedesc::VecSemantics::VECTOR => Self::Vector,
            sys::typedesc::VecSemantics::NORMAL => Self::Normal,
            sys::typedesc::VecSemantics::TIMECODE => Self::Timecode,
            sys::typedesc::VecSemantics::KEYCODE => Self::Keycode,
            sys::typedesc::VecSemantics::RATIONAL => Self::Rational,
            sys::typedesc::VecSemantics::BOX => Self::Box,
            _ => unreachable!(),
        }
    }
}

impl From<VecSemantics> for u32 {
    fn from(value: VecSemantics) -> Self {
        sys::typedesc::VecSemantics::from(value).repr
    }
}

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
#[cfg_attr(test, derive(proptest_derive::Arbitrary))]
pub struct TypeDesc {
    /// C data type at the heart of our type
    pub basetype: BaseType,
    /// What kind of AGGREGATE is it?
    pub aggregate: Aggregate,
    /// Hint: What does the aggregate represent?
    pub vecsemantics: VecSemantics,
    /// Array length, 0 = not array, -1 = unsized
    pub arraylen: i32,
}

/// Display the name, for printing and whatnot.  For example,
/// `float`, `int[5]`, `normal`
impl std::fmt::Display for TypeDesc {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let inner = (*self).into();
        f.write_str(sys::typedesc::typedesc_as_str(&inner))
    }
}

impl From<TypeDesc> for sys::typedesc::TypeDesc {
    fn from(value: TypeDesc) -> Self {
        sys::typedesc::typedesc_new(
            value.basetype.into(),
            value.aggregate.into(),
            value.vecsemantics.into(),
            value.arraylen,
        )
    }
}

/// Construct from a string (e.g., `float[3]`).  If no valid
/// type could be assembled, set base to `UNKNOWN`.
///
/// Examples:
///
/// ```
/// # use oiio::typedesc::*;
/// assert_eq!(TypeDesc::from("int"), TypeDesc::new(BaseType::Int32, Aggregate::Scalar, VecSemantics::NoSemantics, 0));
/// assert_eq!(TypeDesc::from("float"), TypeDesc::new(BaseType::Float, Aggregate::Scalar, VecSemantics::NoSemantics, 0));
/// assert_eq!(TypeDesc::from("uint16"), TypeDesc::new(BaseType::UInt16, Aggregate::Scalar, VecSemantics::NoSemantics, 0));
/// assert_eq!(TypeDesc::from("point"), TypeDesc::new(BaseType::Float, Aggregate::Vec3, VecSemantics::Point, 0));
/// ```
///
impl From<&str> for TypeDesc {
    fn from(value: &str) -> Self {
        sys::typedesc::typedesc_from_string(value).into()
    }
}

/// Construct from a string (e.g., `float[3]`).  If no valid
/// type could be assembled, set base to `UNKNOWN`.
///
/// Examples:
///
/// ```
/// # use oiio::typedesc::*;
/// assert_eq!(TypeDesc::from("int".to_string()), TypeDesc::new(BaseType::Int32, Aggregate::Scalar, VecSemantics::NoSemantics, 0));
/// assert_eq!(TypeDesc::from("float".to_string()), TypeDesc::new(BaseType::Float, Aggregate::Scalar, VecSemantics::NoSemantics, 0));
/// assert_eq!(TypeDesc::from("uint16".to_string()), TypeDesc::new(BaseType::UInt16, Aggregate::Scalar, VecSemantics::NoSemantics, 0));
/// assert_eq!(TypeDesc::from("point".to_string()), TypeDesc::new(BaseType::Float, Aggregate::Vec3, VecSemantics::Point, 0));
/// ```
///
impl From<String> for TypeDesc {
    fn from(value: String) -> Self {
        sys::typedesc::typedesc_from_string(&value).into()
    }
}

/// Construct from a string (e.g., `float[3]`).  If no valid
/// type could be assembled, set base to `UNKNOWN`.
///
/// Examples:
///
/// ```
/// # use oiio::typedesc::*;
/// assert_eq!(TypeDesc::from(&"int".to_string()), TypeDesc::new(BaseType::Int32, Aggregate::Scalar, VecSemantics::NoSemantics, 0));
/// assert_eq!(TypeDesc::from(&"float".to_string()), TypeDesc::new(BaseType::Float, Aggregate::Scalar, VecSemantics::NoSemantics, 0));
/// assert_eq!(TypeDesc::from(&"uint16".to_string()), TypeDesc::new(BaseType::UInt16, Aggregate::Scalar, VecSemantics::NoSemantics, 0));
/// assert_eq!(TypeDesc::from(&"point".to_string()), TypeDesc::new(BaseType::Float, Aggregate::Vec3, VecSemantics::Point, 0));
/// ```
///
impl From<&String> for TypeDesc {
    fn from(value: &String) -> Self {
        sys::typedesc::typedesc_from_string(&value).into()
    }
}

impl From<sys::typedesc::TypeDesc> for TypeDesc {
    fn from(value: sys::typedesc::TypeDesc) -> Self {
        Self {
            basetype: sys::typedesc::BaseType {
                repr: value.basetype as u32,
            }
            .into(),
            aggregate: sys::typedesc::Aggregate {
                repr: value.aggregate as u32,
            }
            .into(),
            vecsemantics: sys::typedesc::VecSemantics {
                repr: value.vecsemantics as u32,
            }
            .into(),
            arraylen: value.arraylen,
        }
    }
}

impl PartialEq for TypeDesc {
    /// Compare two TypeDesc values for equality.
    fn eq(&self, other: &Self) -> bool {
        sys::typedesc::typedesc_eq(&(*self).into(), &(*other).into())
    }

    /// Compare two TypeDesc values for inequality.
    fn ne(&self, other: &Self) -> bool {
        sys::typedesc::typedesc_ne(&(*self).into(), &(*other).into())
    }
}

impl PartialEq<BaseType> for TypeDesc {
    /// Compare a TypeDesc to a basetype (it's the same if it has the
    /// same base type and is not an aggregate or an array).
    fn eq(&self, other: &BaseType) -> bool {
        sys::typedesc::typedesc_eq_basetype(&(*self).into(), (*other).into())
    }

    /// Compare a TypeDesc to a basetype (it's the same if it has the
    /// same base type and is not an aggregate or an array).
    fn ne(&self, other: &BaseType) -> bool {
        sys::typedesc::typedesc_ne_basetype(&(*self).into(), (*other).into())
    }
}

impl Eq for TypeDesc {}

impl PartialOrd for TypeDesc {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for TypeDesc {
    /// Test for lexicographic ordering, comes in handy for lots of containers
    /// and algorithms.
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        if self == other {
            core::cmp::Ordering::Equal
        } else if sys::typedesc::typedesc_lt(&(*self).into(), &(*other).into()) {
            core::cmp::Ordering::Less
        } else {
            core::cmp::Ordering::Greater
        }
    }
}

impl TypeDesc {
    /// Construct from a BaseType and optional aggregateness, semantics,
    /// and arrayness.
    pub fn new(btype: BaseType, agg: Aggregate, semantics: VecSemantics, arraylen: i32) -> Self {
        sys::typedesc::typedesc_new(btype.into(), agg.into(), semantics.into(), arraylen).into()
    }

    /// Construct an array of a non-aggregate BaseType.
    pub fn from_basetype_arraylen(btype: BaseType, arraylen: i32) -> Self {
        sys::typedesc::typedesc_from_basetype_arraylen(btype.into(), arraylen).into()
    }

    /// Construct an array from BaseType, Aggregate, and array length,
    /// with unspecified (or moot) semantic hints.
    pub fn from_basetype_aggregate_arraylen(
        btype: BaseType,
        agg: Aggregate,
        arraylen: i32,
    ) -> Self {
        sys::typedesc::typedesc_from_basetype_aggregate_arraylen(btype.into(), agg.into(), arraylen)
            .into()
    }

    /// Return the number of elements: 1 if not an array, or the array
    /// length. Invalid to call this for arrays of undetermined size.
    pub fn numelements(&self) -> usize {
        sys::typedesc::typedesc_numelements(&(*self).into())
    }

    /// Return the number of basetype values: the aggregate count multiplied
    /// by the array length (or 1 if not an array). Invalid to call this
    /// for arrays of undetermined size.
    pub fn basevalues(&self) -> usize {
        sys::typedesc::typedesc_basevalues(&(*self).into())
    }

    /// Does this TypeDesc describe an array?
    pub fn is_array(&self) -> bool {
        sys::typedesc::typedesc_is_array(&(*self).into())
    }

    /// Does this TypeDesc describe an array, but whose length is not
    /// specified?
    pub fn is_unsized_array(&self) -> bool {
        sys::typedesc::typedesc_is_unsized_array(&(*self).into())
    }

    /// Does this TypeDesc describe an array, whose length is specified?
    pub fn is_sized_array(&self) -> bool {
        sys::typedesc::typedesc_is_sized_array(&(*self).into())
    }

    /// Return the size, in bytes, of this type.
    pub fn size(&self) -> usize {
        sys::typedesc::typedesc_size(&(*self).into())
    }

    /// Return the type of one element, i.e., strip out the array-ness.
    pub fn elementtype(&self) -> Self {
        sys::typedesc::typedesc_elementtype(&(*self).into()).into()
    }

    /// Return the size, in bytes, of one element of this type (that is,
    /// ignoring whether it's an array).
    pub fn elementsize(&self) -> usize {
        sys::typedesc::typedesc_elementsize(&(*self).into())
    }

    /// Return just the underlying C scalar type, i.e., strip out the
    /// array-ness and the aggregateness.
    pub fn scalartype(&self) -> Self {
        sys::typedesc::typedesc_scalartype(&(*self).into()).into()
    }

    /// Return the base type size, i.e., stripped of both array-ness
    /// and aggregateness.
    pub fn basesize(&self) -> usize {
        sys::typedesc::typedesc_basesize(&(*self).into())
    }

    /// True if it's a floating-point type (versus a fundamentally
    /// integral type or something else like a string).
    pub fn is_floating_point(&self) -> bool {
        sys::typedesc::typedesc_is_floating_point(&(*self).into())
    }

    /// True if it's a signed type that allows for negative values.
    pub fn is_signed(&self) -> bool {
        sys::typedesc::typedesc_is_signed(&(*self).into())
    }

    /// Shortcut: is it Unknown?
    pub fn is_unknown(&self) -> bool {
        sys::typedesc::typedesc_is_unknown(&(*self).into())
    }

    /// Set `typedesc` to the type described in the string.  Return the
    /// length of the part of the string that describes the type.  If
    /// no valid type could be assembled, return 0 and do not modify
    /// `typedesc`.
    pub fn set_from_string(&mut self, typestring: &str) -> usize {
        let mut typedesc = sys::typedesc::TypeDesc::from(*self);
        let result = sys::typedesc::typedesc_fromstring(&mut typedesc, typestring);

        *self = typedesc.into();

        result
    }

    /// TypeDesc's are equivalent if they are equal, or if their only
    /// inequality is differing vector semantics.
    pub fn equivalent(&self, b: &TypeDesc) -> bool {
        sys::typedesc::typedesc_equivalent(&(*self).into(), &(*b).into())
    }

    /// Is this a 2-vector aggregate (of the given type)?
    pub fn is_vec2(&self, b: BaseType) -> bool {
        sys::typedesc::typedesc_is_vec2(&(*self).into(), b.into())
    }

    /// Is this a 3-vector aggregate (of the given type)?
    pub fn is_vec3(&self, b: BaseType) -> bool {
        sys::typedesc::typedesc_is_vec3(&(*self).into(), b.into())
    }

    /// Is this a 4-vector aggregate (of the given type)?
    pub fn is_vec4(&self, b: BaseType) -> bool {
        sys::typedesc::typedesc_is_vec4(&(*self).into(), b.into())
    }

    /// Is this an array of aggregates that represents a 2D bounding box?
    pub fn is_box2(&self, b: BaseType) -> bool {
        sys::typedesc::typedesc_is_box2(&(*self).into(), b.into())
    }

    /// Is this an array of aggregates that represents a 3D bounding box?
    pub fn is_box3(&self, b: BaseType) -> bool {
        sys::typedesc::typedesc_is_box3(&(*self).into(), b.into())
    }

    /// Demote the type to a non-array
    pub fn unarray(&mut self) -> () {
        let mut typedesc = sys::typedesc::TypeDesc::from(*self);
        sys::typedesc::typedesc_unarray(&mut typedesc);
        *self = typedesc.into();
    }

    /// Given base data types of a and b, return a basetype that is a best
    /// guess for one that can handle both without any loss of range or
    /// precision.
    pub fn basetype_merge(a: TypeDesc, b: TypeDesc) -> BaseType {
        sys::typedesc::typedesc_basetype_merge(a.into(), b.into()).into()
    }

    /// Given base data types of a and b, return a basetype that is a best
    /// guess for one that can handle both without any loss of range or
    /// precision.
    pub fn basetype_merge_3(a: TypeDesc, b: TypeDesc, c: TypeDesc) -> BaseType {
        {
            sys::typedesc::typedesc_basetype_merge_3(a.into(), b.into(), c.into()).into()
        }
    }

    /// Given data pointed to by src and described by srctype, copy it to the
    /// memory pointed to by dst and described by dsttype, and return true if a
    /// conversion is possible, false if it is not. If the types are equivalent,
    /// this is a straightforward memory copy. If the types differ, there are
    /// several non-equivalent type conversions that will nonetheless succeed:
    /// * If dsttype is a string (and therefore dst points to a ustring or a
    ///   `char*`): it will always succeed, producing a string akin to calling
    ///   `typedesc.to_string()`.
    /// * If dsttype is int32 or uint32: other integer types will do their best
    ///   (caveat emptor if you mix signed/unsigned). Also a source string will
    ///   convert to int if and only if its characters form a valid integer.
    /// * If dsttype is float: inteegers and other float types will do
    ///   their best conversion; strings will convert if and only if their
    ///   characters form a valid float number.
    pub fn convert_type(
        srctype: TypeDesc,
        src: &[u8],
        dsttype: TypeDesc,
        dst: &mut [u8],
        n: i32,
    ) -> bool {
        sys::typedesc::typedesc_convert_type(srctype.into(), src, dsttype.into(), dst, n)
    }
}

#[cfg(test)]
mod tests {
    use proptest::prelude::*;

    use super::*;

    fn from_string_strategy() -> impl Strategy<Value = (TypeDesc, &'static str)> {
        any::<BaseType>().prop_filter_map("BaseType::LastBase is unsupported", |base_type| {
            let base_type_str = match base_type {
                BaseType::Unknown => "unknown",
                BaseType::None => "void",
                BaseType::UInt8 => "uint8",
                BaseType::Int8 => "int8",
                BaseType::UInt16 => "uint16",
                BaseType::Int16 => "int16",
                BaseType::UInt32 => "uint",
                BaseType::Int32 => "int",
                BaseType::UInt64 => "uint64",
                BaseType::Int64 => "int64",
                BaseType::Half => "half",
                BaseType::Float => "float",
                BaseType::Double => "double",
                BaseType::String => "string",
                BaseType::Ptr => "pointer",
                BaseType::UStringHash => "ustringhash",
                BaseType::LastBase => return None,
            };

            Some((
                TypeDesc::from_basetype_arraylen(base_type, 0),
                base_type_str,
            ))
        })
    }

    proptest! {
        #[test]
        fn test_typedesc_new_success(
            btype in any::<BaseType>(),
            agg in any::<Aggregate>(),
            semantics in any::<VecSemantics>(),
            arraylen in i32::MIN..i32::MAX,
        ) {
            let result = TypeDesc::new(btype, agg, semantics, arraylen);

            prop_assert_eq!(result.basetype, btype);
            prop_assert_eq!(result.aggregate, agg);
            prop_assert_eq!(result.vecsemantics, semantics);
            prop_assert_eq!(result.arraylen, arraylen);
        }

        #[test]
        fn test_typedesc_from_basetype_arraylen_success(
            btype in any::<BaseType>(),
            arraylen in i32::MIN..i32::MAX,
        ) {
            let result = TypeDesc::from_basetype_arraylen(btype, arraylen);

            prop_assert_eq!(result.basetype, btype);
            prop_assert_eq!(result.aggregate, Aggregate::Scalar);
            prop_assert_eq!(result.vecsemantics, VecSemantics::NoSemantics);
            prop_assert_eq!(result.arraylen, arraylen);
        }

        #[test]
        fn test_typedesc_from_basetype_aggregate_arraylen_success(
            btype in any::<BaseType>(),
            agg in any::<Aggregate>(),
            arraylen in any::<i32>()
        ) {
            let result = TypeDesc::from_basetype_aggregate_arraylen(btype, agg, arraylen);

            prop_assert_eq!(result.basetype, btype);
            prop_assert_eq!(result.aggregate, agg);
            prop_assert_eq!(result.vecsemantics, VecSemantics::NoSemantics);
            prop_assert_eq!(result.arraylen, arraylen);
        }

        #[test]
        fn test_typedesc_from_string_success(
            (expected_type_desc, input_str) in from_string_strategy()
        ) {
            let result = TypeDesc::from(input_str);

            prop_assert_eq!(result.basetype, expected_type_desc.basetype);
            prop_assert_eq!(result.aggregate, expected_type_desc.aggregate);
            prop_assert_eq!(result.vecsemantics, expected_type_desc.vecsemantics);
            prop_assert_eq!(result.arraylen, expected_type_desc.arraylen);
        }


        #[test]
        fn test_typedesc_clone_success(
            type_desc in any::<TypeDesc>(),
        ) {
            let result = type_desc.clone();

            prop_assert_eq!(result.basetype, type_desc.basetype);
            prop_assert_eq!(result.aggregate, type_desc.aggregate);
            prop_assert_eq!(result.vecsemantics, type_desc.vecsemantics);
            prop_assert_eq!(result.arraylen, type_desc.arraylen);
        }

        #[test]
        fn test_typedesc_as_str_success(
            (input_type_desc, expected_str) in from_string_strategy()
        ) {
            let result = input_type_desc.to_string();

            prop_assert_eq!(result, expected_str);
        }

        #[test]
        fn test_typedesc_numelements_success(
            btype in any::<BaseType>(),
            agg in any::<Aggregate>(),
            semantics in any::<VecSemantics>(),
            arraylen in 1..i32::MAX
        ) {
            let typedesc = TypeDesc::new(btype, agg, semantics, arraylen);
            let result = typedesc.numelements();

            prop_assert_eq!(result, arraylen as usize);
        }

        #[test]
        fn test_typedesc_basevalues_success(
            btype in any::<BaseType>(),
            agg in any::<Aggregate>(),
            semantics in any::<VecSemantics>(),
            arraylen in 1..i32::MAX
        ) {
            let typedesc = TypeDesc::new(btype, agg, semantics, arraylen);
            let result = typedesc.basevalues();

            prop_assert_eq!(result, arraylen as usize * u32::from(agg) as usize);
        }

        #[test]
        fn test_typedesc_is_array_success(
            btype in any::<BaseType>(),
            agg in any::<Aggregate>(),
            semantics in any::<VecSemantics>(),
            arraylen in -2..2
        ) {
            let typedesc = TypeDesc::new(btype, agg, semantics, arraylen);
            let result = typedesc.is_array();

            prop_assert_eq!(result, arraylen != 0);
        }

        #[test]
        fn test_typedesc_is_unsized_array_success(
            btype in any::<BaseType>(),
            agg in any::<Aggregate>(),
            semantics in any::<VecSemantics>(),
            arraylen in -2..2
        ) {
            let typedesc = TypeDesc::new(btype, agg, semantics, arraylen);
            let result = typedesc.is_unsized_array();

            prop_assert_eq!(result, arraylen < 0);
        }

        #[test]
        fn test_typedesc_is_sized_array_success(
            btype in any::<BaseType>(),
            agg in any::<Aggregate>(),
            semantics in any::<VecSemantics>(),
            arraylen in -2..2
        ) {
            let typedesc = TypeDesc::new(btype, agg, semantics, arraylen);
            let result = typedesc.is_sized_array();

            prop_assert_eq!(result, arraylen > 0);
        }

        #[test]
        fn test_typedesc_size_success(
            btype in any::<BaseType>(),
            agg in any::<Aggregate>(),
            semantics in any::<VecSemantics>(),
            arraylen in 1..2
        ) {
            let typedesc = TypeDesc::new(btype, agg, semantics, arraylen);
            let result = typedesc.size();

            prop_assert_eq!(result, arraylen as usize * typedesc.elementsize());
        }

        #[test]
        fn test_typedesc_elementtype_success(
            btype in any::<BaseType>(),
            agg in any::<Aggregate>(),
            semantics in any::<VecSemantics>(),
            arraylen in i32::MIN..i32::MAX
        ) {
            let typedesc = TypeDesc::new(btype, agg, semantics, arraylen);
            let result = typedesc.elementtype();

            prop_assert_eq!(result.basetype, btype);
            prop_assert_eq!(result.aggregate, agg);
            prop_assert_eq!(result.vecsemantics, semantics);
            prop_assert_eq!(result.arraylen, 0);
        }

        #[test]
        fn test_typedesc_elementsize_success(
            btype in any::<BaseType>(),
            agg in any::<Aggregate>(),
            semantics in any::<VecSemantics>(),
            arraylen in i32::MIN..i32::MAX
        ) {
            let typedesc = TypeDesc::new(btype, agg, semantics, arraylen);
            let result = typedesc.elementsize();

            prop_assert_eq!(result, u32::from(agg) as usize * typedesc.basesize());
        }

        #[test]
        fn test_typedesc_scalartype_success(
            btype in any::<BaseType>(),
            agg in any::<Aggregate>(),
            semantics in any::<VecSemantics>(),
            arraylen in i32::MIN..i32::MAX
        ) {
            let typedesc = TypeDesc::new(btype, agg, semantics, arraylen);
            let result = typedesc.scalartype();

            prop_assert_eq!(result.basetype, btype);
            prop_assert_eq!(result.aggregate, Aggregate::Scalar);
            prop_assert_eq!(result.vecsemantics, VecSemantics::NoSemantics);
            prop_assert_eq!(result.arraylen, 0);
        }

        #[test]
        fn test_typedesc_basesize_success(
            btype in any::<BaseType>().prop_filter("BaseType::LastBase not supported", |btype| !matches!(*btype, crate::typedesc::BaseType::LastBase)),
            agg in any::<Aggregate>(),
            semantics in any::<VecSemantics>(),
            arraylen in i32::MIN..i32::MAX
        ) {
            let typedesc = TypeDesc::new(btype, agg, semantics, arraylen);
            let result = typedesc.basesize();

            let expected = match btype {
                BaseType::Unknown => 0,
                BaseType::None => 0,
                BaseType::UInt8 => size_of::<u8>(),
                BaseType::Int8 => size_of::<i8>(),
                BaseType::UInt16 => size_of::<u16>(),
                BaseType::Int16 => size_of::<i16>(),
                BaseType::UInt32 => size_of::<u32>(),
                BaseType::Int32 => size_of::<i32>(),
                BaseType::UInt64 => size_of::<u64>(),
                BaseType::Int64 => size_of::<i64>(),
                BaseType::Half => size_of::<f32>() / 2,
                BaseType::Float => size_of::<f32>(),
                BaseType::Double => size_of::<f64>(),
                BaseType::String => size_of::<*const std::ffi::c_char>(),
                BaseType::Ptr => size_of::<*const std::ffi::c_char>(),
                BaseType::UStringHash => 8, // TODO: Get the size of the UStringHash type.
                BaseType::LastBase => unreachable!(),
            };

            prop_assert_eq!(result, expected);
        }

        #[test]
        fn test_typedesc_is_floating_point_success(
            btype in any::<BaseType>(),
            agg in any::<Aggregate>(),
            semantics in any::<VecSemantics>(),
            arraylen in i32::MIN..i32::MAX
        ) {
            let typedesc = TypeDesc::new(btype, agg, semantics, arraylen);
            let result = typedesc.is_floating_point();

            prop_assert_eq!(result, matches!(btype, BaseType::Half | BaseType::Float | BaseType::Double));
        }

        #[test]
        fn test_typedesc_is_unknown_success(
            btype in any::<BaseType>(),
            agg in any::<Aggregate>(),
            semantics in any::<VecSemantics>(),
            arraylen in i32::MIN..i32::MAX
        ) {
            let typedesc = TypeDesc::new(btype, agg, semantics, arraylen);
            let result = typedesc.is_unknown();

            prop_assert_eq!(result, matches!(btype, BaseType::Unknown));
        }

        #[test]
        fn test_typedesc_fromstring_success(
            btype in any::<BaseType>(),
            agg in any::<Aggregate>(),
            semantics in any::<VecSemantics>(),
            arraylen in i32::MIN..i32::MAX,
            (expected_type_desc, input_str) in from_string_strategy()
        ) {
            let mut typedesc = TypeDesc::new(btype, agg, semantics, arraylen);
            typedesc.set_from_string(&input_str);

            prop_assert_eq!(typedesc.basetype, expected_type_desc.basetype);
            prop_assert_eq!(typedesc.aggregate, expected_type_desc.aggregate);
            prop_assert_eq!(typedesc.vecsemantics, expected_type_desc.vecsemantics);
            prop_assert_eq!(typedesc.arraylen, expected_type_desc.arraylen);
        }

        #[test]
        fn test_typedesc_eq_success(
            btype_a in any::<BaseType>(),
            agg_a in any::<Aggregate>(),
            semantics_a in any::<VecSemantics>(),
            arraylen_a in i32::MIN..i32::MAX,
            btype_b in any::<BaseType>(),
            agg_b in any::<Aggregate>(),
            semantics_b in any::<VecSemantics>(),
            arraylen_b in i32::MIN..i32::MAX,
        ) {
            let typedesc_a = TypeDesc::new(btype_a, agg_a, semantics_a, arraylen_a);
            let typedesc_b = TypeDesc::new(btype_b, agg_b, semantics_b, arraylen_b);

            if btype_a == btype_b && agg_a == agg_b && semantics_a == semantics_b && arraylen_a == arraylen_b {
                prop_assert!(typedesc_a == typedesc_b);
            } else {
                prop_assert!(!(typedesc_a == typedesc_b));
            }
        }

        #[test]
        fn test_typedesc_ne_success(
            btype_a in any::<BaseType>(),
            agg_a in any::<Aggregate>(),
            semantics_a in any::<VecSemantics>(),
            arraylen_a in i32::MIN..i32::MAX,
            btype_b in any::<BaseType>(),
            agg_b in any::<Aggregate>(),
            semantics_b in any::<VecSemantics>(),
            arraylen_b in i32::MIN..i32::MAX,
        ) {
            let typedesc_a = TypeDesc::new(btype_a, agg_a, semantics_a, arraylen_a);
            let typedesc_b = TypeDesc::new(btype_b, agg_b, semantics_b, arraylen_b);

            if btype_a == btype_b && agg_a == agg_b && semantics_a == semantics_b && arraylen_a == arraylen_b {
                prop_assert!(!(typedesc_a != typedesc_b));
            } else {
                prop_assert!(typedesc_a != typedesc_b);
            }
        }

        #[test]
        fn test_typedesc_eq_basetype_success(
            btype_a in any::<BaseType>(),
            agg_a in any::<Aggregate>(),
            semantics_a in any::<VecSemantics>(),
            arraylen_a in -2..2,
            btype_b in any::<BaseType>(),
        ) {
            let typedesc_a = TypeDesc::new(btype_a, agg_a, semantics_a, arraylen_a);

            if btype_a == btype_b && agg_a == Aggregate::Scalar && !typedesc_a.is_array() {
                prop_assert!(typedesc_a == btype_b);
                prop_assert!(btype_b == typedesc_a);
            } else {
                prop_assert!(!(typedesc_a == btype_b));
                prop_assert!(!(btype_b == typedesc_a));
            }
        }

        #[test]
        fn test_typedesc_ne_basetype_success(
            btype_a in any::<BaseType>(),
            agg_a in any::<Aggregate>(),
            semantics_a in any::<VecSemantics>(),
            arraylen_a in -2..2,
            btype_b in any::<BaseType>(),
        ) {
            let typedesc_a = TypeDesc::new(btype_a, agg_a, semantics_a, arraylen_a);

            if btype_a == btype_b && agg_a == Aggregate::Scalar && !typedesc_a.is_array() {
                prop_assert!(!(typedesc_a != btype_b));
                prop_assert!(!(btype_b != typedesc_a));
            } else {
                prop_assert!(typedesc_a != btype_b);
                prop_assert!(btype_b != typedesc_a);
            }
        }

        #[test]
        fn test_typedesc_equivalent_success(
            btype_a in any::<BaseType>(),
            agg_a in any::<Aggregate>(),
            semantics_a in any::<VecSemantics>(),
            btype_b in any::<BaseType>(),
            agg_b in any::<Aggregate>(),
            semantics_b in any::<VecSemantics>(),
        ) {
            let typedesc_a = TypeDesc::new(btype_a, agg_a, semantics_a, 0);
            let typedesc_b = TypeDesc::new(btype_b, agg_b, semantics_b, 0);

            if btype_a == btype_b && agg_a == agg_b {
                prop_assert!(typedesc_a.equivalent(&typedesc_b));
            } else {
                prop_assert!(!typedesc_a.equivalent(&typedesc_b));
            }
        }

        #[test]
        fn test_typedesc_is_vec2_success(
            btype in any::<BaseType>(),
            btype_b in any::<BaseType>(),
            agg in any::<Aggregate>(),
            semantics in any::<VecSemantics>(),
            arraylen in -2..2
        ) {
            let result = TypeDesc::new(btype, agg, semantics, arraylen);

            if result.aggregate == Aggregate::Vec2 && result.basetype == btype_b && !result.is_array() {
                prop_assert!(result.is_vec2(btype_b));
            } else {
                prop_assert!(!result.is_vec2(btype_b));

            }
        }

        #[test]
        fn test_typedesc_is_vec3_success(
            btype in any::<BaseType>(),
            btype_b in any::<BaseType>(),
            agg in any::<Aggregate>(),
            semantics in any::<VecSemantics>(),
            arraylen in -2..2
        ) {
            let result = TypeDesc::new(btype, agg, semantics, arraylen);

            if result.aggregate == Aggregate::Vec3 && result.basetype == btype_b && !result.is_array() {
                prop_assert!(result.is_vec3(btype_b));
            } else {
                prop_assert!(!result.is_vec3(btype_b));

            }
        }

        #[test]
        fn test_typedesc_is_vec4_success(
            btype in any::<BaseType>(),
            btype_b in any::<BaseType>(),
            agg in any::<Aggregate>(),
            semantics in any::<VecSemantics>(),
            arraylen in -2..2
        ) {
            let result = TypeDesc::new(btype, agg, semantics, arraylen);

            if result.aggregate == Aggregate::Vec4 && result.basetype == btype_b && !result.is_array() {
                prop_assert!(result.is_vec4(btype_b));
            } else {
                prop_assert!(!result.is_vec4(btype_b));

            }
        }

        #[test]
        fn test_typedesc_is_box2_success(
            btype in any::<BaseType>(),
            btype_b in any::<BaseType>(),
            agg in any::<Aggregate>(),
            semantics in any::<VecSemantics>(),
            arraylen in -2..4
        ) {
            let result = TypeDesc::new(btype, agg, semantics, arraylen);

            if result.aggregate == Aggregate::Vec2 && result.vecsemantics == VecSemantics::Box && result.basetype == btype_b && arraylen == 2 {
                prop_assert!(result.is_box2(btype_b));
            } else {
                prop_assert!(!result.is_box2(btype_b));

            }
        }

        #[test]
        fn test_typedesc_is_box3_success(
            btype in any::<BaseType>(),
            btype_b in any::<BaseType>(),
            agg in any::<Aggregate>(),
            semantics in any::<VecSemantics>(),
            arraylen in -2..4
        ) {
            let result = TypeDesc::new(btype, agg, semantics, arraylen);

            if result.aggregate == Aggregate::Vec3 && result.vecsemantics == VecSemantics::Box && result.basetype == btype_b && arraylen == 2 {
                prop_assert!(result.is_box3(btype_b));
            } else {
                prop_assert!(!result.is_box3(btype_b));

            }
        }

        #[test]
        fn test_typedesc_unarray_success(
            btype in any::<BaseType>(),
            agg in any::<Aggregate>(),
            semantics in any::<VecSemantics>(),
            arraylen in i32::MIN..i32::MAX
        ) {
            let mut result = TypeDesc::new(btype, agg, semantics, arraylen);
            result.unarray();

            prop_assert_eq!(result.arraylen, 0);
        }

        #[test]
        fn test_typedesc_lt_success(
            btype_a in any::<BaseType>(),
            agg_a in any::<Aggregate>(),
            semantics_a in any::<VecSemantics>(),
            arraylen_a in i32::MIN..i32::MAX,
            btype_b in any::<BaseType>(),
            agg_b in any::<Aggregate>(),
            semantics_b in any::<VecSemantics>(),
            arraylen_b in i32::MIN..i32::MAX,
        ) {
            let typedesc_a = TypeDesc::new(btype_a, agg_a, semantics_a, arraylen_a);
            let typedesc_b = TypeDesc::new(btype_b, agg_b, semantics_b, arraylen_b);

            if btype_a != btype_b {
                prop_assert_eq!(typedesc_a < typedesc_b, btype_a < btype_b);
            } else if agg_a != agg_b {
                prop_assert_eq!(typedesc_a < typedesc_b, agg_a < agg_b);
            } else if arraylen_a != arraylen_b {
                prop_assert_eq!(typedesc_a < typedesc_b, arraylen_a < arraylen_b);
            } else if semantics_a != semantics_b {
                prop_assert_eq!(typedesc_a < typedesc_b, semantics_a < semantics_b);
            } else {
                prop_assert_eq!(typedesc_a < typedesc_b, false);
            }
        }

        #[test]
        fn test_typedesc_basetype_merge_success(
            mut btype_a in any::<BaseType>(),
            agg_a in any::<Aggregate>(),
            semantics_a in any::<VecSemantics>(),
            arraylen_a in i32::MIN..i32::MAX,
            mut btype_b in any::<BaseType>(),
            agg_b in any::<Aggregate>(),
            semantics_b in any::<VecSemantics>(),
            arraylen_b in i32::MIN..i32::MAX,
        ) {
            let typedesc_a = TypeDesc::new(btype_a, agg_a, semantics_a, arraylen_a);
            let typedesc_b = TypeDesc::new(btype_b, agg_b, semantics_b, arraylen_b);
            let result = TypeDesc::basetype_merge(typedesc_a, typedesc_b);

            if btype_a == btype_b {
                prop_assert_eq!(result, btype_a);
                prop_assert_eq!(result, btype_b);
            } else if btype_a == BaseType::Unknown {
                prop_assert_eq!(result, btype_b);
            } else if btype_b == BaseType::Unknown {
                prop_assert_eq!(result, btype_a);
            } else {
                let size_a = TypeDesc::new(btype_a, Aggregate::Scalar, VecSemantics::NoSemantics, 0).size();
                let size_b = TypeDesc::new(btype_b, Aggregate::Scalar, VecSemantics::NoSemantics, 0).size();

                if size_a < size_b {
                    std::mem::swap(&mut btype_a, &mut btype_b);
                }

                if btype_a == BaseType::Double || btype_a == BaseType::Float {
                    prop_assert_eq!(result, btype_a);
                } else if btype_a == BaseType::UInt32 && (btype_b == BaseType::UInt16 || btype_b == BaseType::UInt8) {
                    prop_assert_eq!(result, btype_a);
                } else if btype_a == BaseType::Int32 && (btype_b == BaseType::Int16 || btype_b == BaseType::Int8 || btype_b == BaseType::UInt16 || btype_b == BaseType::UInt8) {
                    prop_assert_eq!(result, btype_a);
                } else if (btype_a == BaseType::UInt16 || btype_a == BaseType::Half) && btype_b == BaseType::UInt8 {
                    prop_assert_eq!(result, btype_a);
                } else if (btype_a == BaseType::Int16 || btype_a == BaseType::Half) && (btype_b == BaseType::Int8 || btype_b == BaseType::UInt8) {
                    prop_assert_eq!(result, btype_a);
                } else {
                    prop_assert_eq!(result, BaseType::Float);
                }
            }
        }

        #[test]
        fn test_typedesc_basetype_merge_3_success(
            btype_a in any::<BaseType>(),
            agg_a in any::<Aggregate>(),
            semantics_a in any::<VecSemantics>(),
            arraylen_a in i32::MIN..i32::MAX,
            btype_b in any::<BaseType>(),
            agg_b in any::<Aggregate>(),
            semantics_b in any::<VecSemantics>(),
            arraylen_b in i32::MIN..i32::MAX,
            mut btype_c in any::<BaseType>(),
            agg_c in any::<Aggregate>(),
            semantics_c in any::<VecSemantics>(),
            arraylen_c in i32::MIN..i32::MAX,
        ) {
            let typedesc_a = TypeDesc::new(btype_a, agg_a, semantics_a, arraylen_a);
            let typedesc_b = TypeDesc::new(btype_b, agg_b, semantics_b, arraylen_b);
            let typedesc_c = TypeDesc::new(btype_c, agg_c, semantics_c, arraylen_c);
            let result = TypeDesc::basetype_merge_3(typedesc_a, typedesc_b, typedesc_c);
            let mut btype_inner = TypeDesc::basetype_merge(typedesc_a, typedesc_b);

            if btype_inner == btype_c {
                prop_assert_eq!(result, btype_inner);
                prop_assert_eq!(result, btype_c);
            } else if btype_inner == BaseType::Unknown {
                prop_assert_eq!(result, btype_c);
            } else if btype_c == BaseType::Unknown {
                prop_assert_eq!(result, btype_inner);
            } else {
                let size_a = TypeDesc::new(btype_inner, Aggregate::Scalar, VecSemantics::NoSemantics, 0).size();
                let size_b = TypeDesc::new(btype_c, Aggregate::Scalar, VecSemantics::NoSemantics, 0).size();

                if size_a < size_b {
                    std::mem::swap(&mut btype_inner, &mut btype_c);
                }

                if btype_inner == BaseType::Double || btype_inner == BaseType::Float {
                    prop_assert_eq!(result, btype_inner);
                } else if btype_inner == BaseType::UInt32 && (btype_c == BaseType::UInt16 || btype_c == BaseType::UInt8) {
                    prop_assert_eq!(result, btype_inner);
                } else if btype_inner == BaseType::Int32 && (btype_c == BaseType::Int16 || btype_c == BaseType::Int8 || btype_c == BaseType::UInt16 || btype_c == BaseType::UInt8) {
                    prop_assert_eq!(result, btype_inner);
                } else if (btype_inner == BaseType::UInt16 || btype_inner == BaseType::Half) && btype_c == BaseType::UInt8 {
                    prop_assert_eq!(result, btype_inner);
                } else if (btype_inner == BaseType::Int16 || btype_inner == BaseType::Half) && (btype_c == BaseType::Int8 || btype_c == BaseType::UInt8) {
                    prop_assert_eq!(result, btype_inner);
                } else {
                    prop_assert_eq!(result, BaseType::Float);
                }
            }
        }

        #[test]
        fn test_typedesc_convert_type_success(
            value_in in i32::MIN..i32::MAX,
        ) {
            let mut value_out = 0.0f32.to_ne_bytes();
            let src_type = TypeDesc::new(BaseType::Int32, Aggregate::Scalar, VecSemantics::NoSemantics, 0);
            let dst_type = TypeDesc::new(BaseType::Float, Aggregate::Scalar, VecSemantics::NoSemantics, 0);

            prop_assert!(TypeDesc::convert_type(src_type, &value_in.to_ne_bytes(), dst_type, &mut value_out, 1));
            prop_assert_eq!(f32::from_ne_bytes(value_out), value_in as f32);
        }
    }
}
