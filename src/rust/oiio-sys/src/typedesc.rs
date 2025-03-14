pub use ffi::*;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[cfg_attr(test, derive(proptest_derive::Arbitrary))]
pub enum BaseType {
    Unknown,
    None,
    UInt8,
    Int8,
    UInt16,
    Int16,
    UInt32,
    Int32,
    UInt64,
    Int64,
    Half,
    Float,
    Double,
    String,
    Ptr,
    UStringHash,
    LastBase,
}

unsafe impl cxx::ExternType for BaseType {
    type Id = cxx::type_id!("oiio_ffi::BaseType");
    type Kind = cxx::kind::Trivial;
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[cfg_attr(test, derive(proptest_derive::Arbitrary))]
pub enum Aggregate {
    Scalar = 1,
    Vec2 = 2,
    Vec3 = 3,
    Vec4 = 4,
    Matrix33 = 9,
    Matrix44 = 16,
}

unsafe impl cxx::ExternType for Aggregate {
    type Id = cxx::type_id!("oiio_ffi::Aggregate");
    type Kind = cxx::kind::Trivial;
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[cfg_attr(test, derive(proptest_derive::Arbitrary))]
pub enum VecSemantics {
    NoSemantics = 0,
    Color,
    Point,
    Vector,
    Normal,
    Timecode,
    Keycode,
    Rational,
    Box,
}

unsafe impl cxx::ExternType for VecSemantics {
    type Id = cxx::type_id!("oiio_ffi::VecSemantics");
    type Kind = cxx::kind::Trivial;
}

#[derive(Debug, Clone, Copy)]
#[cfg_attr(test, derive(proptest_derive::Arbitrary))]
#[repr(C)]
pub struct TypeDesc {
    pub basetype: BaseType,
    pub aggregate: Aggregate,
    pub vecsemantics: VecSemantics,
    pub _reserved: u8,
    pub arraylen: i32,
}

unsafe impl cxx::ExternType for TypeDesc {
    type Id = cxx::type_id!("oiio_ffi::TypeDesc");
    type Kind = cxx::kind::Trivial;
}

#[cxx::bridge(namespace = oiio_ffi)]
mod ffi {

    unsafe extern "C++" {
        include!("oiio-sys/include/typedesc.h");

        // TypeDesc
        type BaseType = crate::typedesc::BaseType;
        type Aggregate = crate::typedesc::Aggregate;
        type VecSemantics = crate::typedesc::VecSemantics;
        type TypeDesc = crate::typedesc::TypeDesc;

        fn typedesc_new(
            btype: BaseType,
            agg: Aggregate,
            semantics: VecSemantics,
            arraylen: i32,
        ) -> TypeDesc;

        fn typedesc_from_basetype_arraylen(btype: BaseType, arraylen: i32) -> TypeDesc;

        fn typedesc_from_basetype_aggregate_arraylen(
            btype: BaseType,
            agg: Aggregate,
            arraylen: i32,
        ) -> TypeDesc;

        fn typedesc_from_string(typestring: &str) -> TypeDesc;

        fn typedesc_clone(t: &TypeDesc) -> TypeDesc;

        fn typedesc_as_str(typedesc: &TypeDesc) -> &str;

        fn typedesc_numelements(typedesc: &TypeDesc) -> usize;

        fn typedesc_basevalues(typedesc: &TypeDesc) -> usize;

        fn typedesc_is_array(typedesc: &TypeDesc) -> bool;

        fn typedesc_is_unsized_array(typedesc: &TypeDesc) -> bool;

        fn typedesc_is_sized_array(typedesc: &TypeDesc) -> bool;

        fn typedesc_size(typedesc: &TypeDesc) -> usize;

        fn typedesc_elementtype(typedesc: &TypeDesc) -> TypeDesc;

        fn typedesc_elementsize(typedesc: &TypeDesc) -> usize;

        fn typedesc_scalartype(typedesc: &TypeDesc) -> TypeDesc;

        fn typedesc_basesize(typedesc: &TypeDesc) -> usize;

        fn typedesc_is_floating_point(typedesc: &TypeDesc) -> bool;

        fn typedesc_is_signed(typedesc: &TypeDesc) -> bool;

        fn typedesc_is_unknown(typedesc: &TypeDesc) -> bool;

        fn typedesc_fromstring(typedesc: &mut TypeDesc, typestring: &str) -> usize;

        fn typedesc_eq(typedesc: &TypeDesc, t: &TypeDesc) -> bool;

        fn typedesc_ne(typedesc: &TypeDesc, t: &TypeDesc) -> bool;

        fn typedesc_eq_basetype(t: &TypeDesc, b: BaseType) -> bool;

        fn basetype_eq_typedesc(b: BaseType, t: &TypeDesc) -> bool;

        fn typedesc_ne_basetype(t: &TypeDesc, b: BaseType) -> bool;

        fn basetype_ne_typedesc(b: BaseType, t: &TypeDesc) -> bool;

        fn typedesc_equivalent(typedesc: &TypeDesc, b: &TypeDesc) -> bool;

        fn typedesc_is_vec2(typedesc: &TypeDesc, b: BaseType) -> bool;

        fn typedesc_is_vec3(typedesc: &TypeDesc, b: BaseType) -> bool;

        fn typedesc_is_vec4(typedesc: &TypeDesc, b: BaseType) -> bool;

        fn typedesc_is_box2(typedesc: &TypeDesc, b: BaseType) -> bool;

        fn typedesc_is_box3(typedesc: &TypeDesc, b: BaseType) -> bool;

        fn typedesc_unarray(typedesc: &mut TypeDesc) -> ();

        fn typedesc_lt(typedesc: &TypeDesc, x: &TypeDesc) -> bool;

        fn typedesc_basetype_merge_2(a: TypeDesc, b: TypeDesc) -> BaseType;

        fn typedesc_basetype_merge_3(a: TypeDesc, b: TypeDesc, c: TypeDesc) -> BaseType;

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

    fn from_string_strategy() -> impl Strategy<Value = (TypeDesc, &'static str)> {
        any::<BaseType>().prop_map(|base_type| {
            let base_type_str = match base_type {
                BaseType::Unknown => "unknown",
                BaseType::None => "void",
                BaseType::UInt8 => "uint8",
                BaseType::Int8 => "int8",
                BaseType::UInt16 => "uint16",
                BaseType::Int16 => "int16",
                BaseType::UInt32 => "uint32",
                BaseType::Int32 => "int32",
                BaseType::UInt64 => "uint64",
                BaseType::Int64 => "int64",
                BaseType::Half => "half",
                BaseType::Float => "float",
                BaseType::Double => "double",
                BaseType::String => "string",
                BaseType::Ptr => "pointer",
                BaseType::UStringHash => "ustringhash",
                BaseType::LastBase => "unknown",
            };

            (typedesc_from_basetype_arraylen(base_type, 0), base_type_str)
        })
    }

    proptest! {
        #[test]
        fn test_typedesc_new_success(btype in any::<BaseType>(), agg in any::<Aggregate>(), semantics in any::<VecSemantics>(), arraylen in i32::MIN..i32::MAX) {
            let result = typedesc_new(btype, agg, semantics, arraylen);

            prop_assert_eq!(result.basetype, btype);
            prop_assert_eq!(result.aggregate, agg);
            prop_assert_eq!(result.vecsemantics, semantics);
            prop_assert_eq!(result.arraylen, arraylen);
        }

        #[test]
        fn test_typedesc_from_basetype_arraylen_success(btype in any::<BaseType>(), arraylen in i32::MIN..i32::MAX) {
            let result = typedesc_from_basetype_arraylen(btype, arraylen);

            prop_assert_eq!(result.basetype, btype);
            prop_assert_eq!(result.aggregate, Aggregate::Scalar);
            prop_assert_eq!(result.vecsemantics, VecSemantics::NoSemantics);
            prop_assert_eq!(result.arraylen, arraylen);
        }

        #[test]
        fn test_typedesc_from_basetype_aggregate_arraylen_success(btype in any::<BaseType>(), agg in any::<Aggregate>(), arraylen in any::<i32>()) {
            let result = typedesc_from_basetype_aggregate_arraylen(btype, agg, arraylen);

            prop_assert_eq!(result.basetype, btype);
            prop_assert_eq!(result.aggregate, agg);
            prop_assert_eq!(result.vecsemantics, VecSemantics::NoSemantics);
            prop_assert_eq!(result.arraylen, arraylen);
        }

        #[test]
        fn test_typedesc_from_string_success((expected_type_desc, input_str) in from_string_strategy()) {
            let result = typedesc_from_string(&input_str);

            prop_assert_eq!(result.basetype, expected_type_desc.basetype);
            prop_assert_eq!(result.aggregate, expected_type_desc.aggregate);
            prop_assert_eq!(result.vecsemantics, expected_type_desc.vecsemantics);
            prop_assert_eq!(result.arraylen, expected_type_desc.arraylen);
        }

        #[test]
        fn test_typedesc_clone_success(type_desc in any::<TypeDesc>()) {
            let result = typedesc_clone(&type_desc);

            prop_assert_eq!(result.basetype, type_desc.basetype);
            prop_assert_eq!(result.aggregate, type_desc.aggregate);
            prop_assert_eq!(result.vecsemantics, type_desc.vecsemantics);
            prop_assert_eq!(result.arraylen, type_desc.arraylen);
            // let result = typedesc_from_string(&input_str);

            // prop_assert_eq!(result.basetype, expected_type_desc.basetype);
            // prop_assert_eq!(result.aggregate, expected_type_desc.aggregate);
            // prop_assert_eq!(result.vecsemantics, expected_type_desc.vecsemantics);
            // prop_assert_eq!(result.arraylen, expected_type_desc.arraylen);
        }
    }
}
