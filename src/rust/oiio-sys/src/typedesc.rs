pub use ffi::*;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BaseType {
    Unknown,
    None,
    UInt8,
    Int8,
    Uint16,
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

    use super::*;

    fn basetype_strategy() -> impl Strategy<Value = BaseType> {
        prop_oneof![
            Just(BaseType::Unknown),
            Just(BaseType::None),
            Just(BaseType::UInt8),
            Just(BaseType::Int8),
            Just(BaseType::Uint16),
            Just(BaseType::Int16),
            Just(BaseType::UInt32),
            Just(BaseType::Int32),
            Just(BaseType::UInt64),
            Just(BaseType::Int64),
            Just(BaseType::Half),
            Just(BaseType::Float),
            Just(BaseType::Double),
            Just(BaseType::String),
            Just(BaseType::Ptr),
            Just(BaseType::UStringHash),
            Just(BaseType::LastBase),
        ]
    }

    fn aggregate_strategy() -> impl Strategy<Value = Aggregate> {
        prop_oneof![
            Just(Aggregate::Scalar),
            Just(Aggregate::Vec2),
            Just(Aggregate::Vec3),
            Just(Aggregate::Vec4),
            Just(Aggregate::Matrix33),
            Just(Aggregate::Matrix44),
        ]
    }

    fn semantics_strategy() -> impl Strategy<Value = VecSemantics> {
        prop_oneof![
            Just(VecSemantics::NoSemantics),
            Just(VecSemantics::Color),
            Just(VecSemantics::Point),
            Just(VecSemantics::Vector),
            Just(VecSemantics::Normal),
            Just(VecSemantics::Timecode),
            Just(VecSemantics::Keycode),
            Just(VecSemantics::Rational),
            Just(VecSemantics::Box),
        ]
    }

    proptest! {
        #[test]
        fn test_typedesc_new(btype in basetype_strategy(), agg in aggregate_strategy(), semantics in semantics_strategy(), arraylen in 0i32..i32::MAX) {
            let result = typedesc_new(btype, agg, semantics, arraylen);

            prop_assert_eq!(result.basetype, btype);
            prop_assert_eq!(result.aggregate, agg);
            prop_assert_eq!(result.vecsemantics, semantics);
            prop_assert_eq!(result.arraylen, arraylen);
        }
    }
}
