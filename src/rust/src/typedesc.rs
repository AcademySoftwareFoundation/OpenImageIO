use crate::sys;

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub enum BaseType {
    #[default]
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

impl From<BaseType> for sys::typedesc::BaseType {
    fn from(value: BaseType) -> Self {
        match value {
            BaseType::Unknown => Self::Unknown,
            BaseType::None => Self::None,
            BaseType::UInt8 => Self::UInt8,
            BaseType::Int8 => Self::Int8,
            BaseType::Uint16 => Self::Uint16,
            BaseType::Int16 => Self::Int16,
            BaseType::UInt32 => Self::UInt32,
            BaseType::Int32 => Self::Int32,
            BaseType::UInt64 => Self::UInt64,
            BaseType::Int64 => Self::Int64,
            BaseType::Half => Self::Half,
            BaseType::Float => Self::Float,
            BaseType::Double => Self::Double,
            BaseType::String => Self::String,
            BaseType::Ptr => Self::Ptr,
            BaseType::UStringHash => Self::UStringHash,
            BaseType::LastBase => Self::LastBase,
        }
    }
}

impl From<sys::typedesc::BaseType> for BaseType {
    fn from(value: sys::typedesc::BaseType) -> Self {
        match value {
            oiio_sys::typedesc::BaseType::Unknown => Self::Unknown,
            oiio_sys::typedesc::BaseType::None => Self::None,
            oiio_sys::typedesc::BaseType::UInt8 => Self::UInt8,
            oiio_sys::typedesc::BaseType::Int8 => Self::Int8,
            oiio_sys::typedesc::BaseType::Uint16 => Self::Uint16,
            oiio_sys::typedesc::BaseType::Int16 => Self::Int16,
            oiio_sys::typedesc::BaseType::UInt32 => Self::UInt32,
            oiio_sys::typedesc::BaseType::Int32 => Self::Int32,
            oiio_sys::typedesc::BaseType::UInt64 => Self::UInt64,
            oiio_sys::typedesc::BaseType::Int64 => Self::Int64,
            oiio_sys::typedesc::BaseType::Half => Self::Half,
            oiio_sys::typedesc::BaseType::Float => Self::Float,
            oiio_sys::typedesc::BaseType::Double => Self::Double,
            oiio_sys::typedesc::BaseType::String => Self::String,
            oiio_sys::typedesc::BaseType::Ptr => Self::Ptr,
            oiio_sys::typedesc::BaseType::UStringHash => Self::UStringHash,
            oiio_sys::typedesc::BaseType::LastBase => Self::LastBase,
        }
    }
}

impl PartialEq<TypeDesc> for BaseType {
    fn eq(&self, other: &TypeDesc) -> bool {
        sys::typedesc::basetype_eq_typedesc((*self).into(), &(*other).into())
    }

    fn ne(&self, other: &TypeDesc) -> bool {
        sys::typedesc::basetype_ne_typedesc((*self).into(), &(*other).into())
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Aggregate {
    Scalar,
    Vec2,
    Vec3,
    Vec4,
    Matrix33,
    Matrix44,
}

impl From<Aggregate> for sys::typedesc::Aggregate {
    fn from(value: Aggregate) -> Self {
        match value {
            Aggregate::Scalar => Self::Scalar,
            Aggregate::Vec2 => Self::Vec2,
            Aggregate::Vec3 => Self::Vec3,
            Aggregate::Vec4 => Self::Vec4,
            Aggregate::Matrix33 => Self::Matrix33,
            Aggregate::Matrix44 => Self::Matrix44,
        }
    }
}

impl From<sys::typedesc::Aggregate> for Aggregate {
    fn from(value: sys::typedesc::Aggregate) -> Self {
        match value {
            sys::typedesc::Aggregate::Scalar => Self::Scalar,
            sys::typedesc::Aggregate::Vec2 => Self::Vec2,
            sys::typedesc::Aggregate::Vec3 => Self::Vec3,
            sys::typedesc::Aggregate::Vec4 => Self::Vec4,
            sys::typedesc::Aggregate::Matrix33 => Self::Matrix33,
            sys::typedesc::Aggregate::Matrix44 => Self::Matrix44,
        }
    }
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub enum VecSemantics {
    #[default]
    NoSemantics,
    Color,
    Point,
    Vector,
    Normal,
    Timecode,
    Keycode,
    Rational,
    Box,
}

impl From<VecSemantics> for sys::typedesc::VecSemantics {
    fn from(value: VecSemantics) -> Self {
        match value {
            VecSemantics::NoSemantics => Self::NoSemantics,
            VecSemantics::Color => Self::Color,
            VecSemantics::Point => Self::Point,
            VecSemantics::Vector => Self::Vector,
            VecSemantics::Normal => Self::Normal,
            VecSemantics::Timecode => Self::Timecode,
            VecSemantics::Keycode => Self::Keycode,
            VecSemantics::Rational => Self::Rational,
            VecSemantics::Box => Self::Box,
        }
    }
}

impl From<sys::typedesc::VecSemantics> for VecSemantics {
    fn from(value: sys::typedesc::VecSemantics) -> Self {
        match value {
            sys::typedesc::VecSemantics::NoSemantics => Self::NoSemantics,
            sys::typedesc::VecSemantics::Color => Self::Color,
            sys::typedesc::VecSemantics::Point => Self::Point,
            sys::typedesc::VecSemantics::Vector => Self::Vector,
            sys::typedesc::VecSemantics::Normal => Self::Normal,
            sys::typedesc::VecSemantics::Timecode => Self::Timecode,
            sys::typedesc::VecSemantics::Keycode => Self::Keycode,
            sys::typedesc::VecSemantics::Rational => Self::Rational,
            sys::typedesc::VecSemantics::Box => Self::Box,
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub struct TypeDesc {
    pub basetype: BaseType,
    pub aggregate: Aggregate,
    pub vecsemantics: VecSemantics,
    pub arraylen: usize,
}

impl std::fmt::Display for TypeDesc {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let inner = (*self).into();
        f.write_str(sys::typedesc::typedesc_as_str(&inner))
    }
}

impl From<TypeDesc> for sys::typedesc::TypeDesc {
    fn from(value: TypeDesc) -> Self {
        Self {
            basetype: value.basetype.into(),
            aggregate: value.aggregate.into(),
            vecsemantics: value.vecsemantics.into(),
            _reserved: 0,
            arraylen: value.arraylen as i32,
        }
    }
}

impl From<sys::typedesc::TypeDesc> for TypeDesc {
    fn from(value: sys::typedesc::TypeDesc) -> Self {
        Self {
            basetype: value.basetype.into(),
            aggregate: value.aggregate.into(),
            vecsemantics: value.vecsemantics.into(),
            arraylen: value.arraylen as usize,
        }
    }
}

impl PartialEq for TypeDesc {
    fn eq(&self, other: &Self) -> bool {
        sys::typedesc::typedesc_eq(&(*self).into(), &(*other).into())
    }

    fn ne(&self, other: &Self) -> bool {
        sys::typedesc::typedesc_ne(&(*self).into(), &(*other).into())
    }
}

impl PartialEq<BaseType> for TypeDesc {
    fn eq(&self, other: &BaseType) -> bool {
        sys::typedesc::typedesc_eq_basetype(&(*self).into(), (*other).into())
    }

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
    pub fn new(btype: BaseType, agg: Aggregate, semantics: VecSemantics, arraylen: usize) -> Self {
        sys::typedesc::typedesc_new(btype.into(), agg.into(), semantics.into(), arraylen as i32)
            .into()
    }

    pub fn from_basetype_arraylen(btype: BaseType, arraylen: usize) -> Self {
        sys::typedesc::typedesc_from_basetype_arraylen(btype.into(), arraylen as i32).into()
    }

    pub fn from_basetype_aggregate_arraylen(
        btype: BaseType,
        agg: Aggregate,
        arraylen: usize,
    ) -> Self {
        sys::typedesc::typedesc_from_basetype_aggregate_arraylen(
            btype.into(),
            agg.into(),
            arraylen as i32,
        )
        .into()
    }

    pub fn from_string(typestring: &str) -> Self {
        sys::typedesc::typedesc_from_string(typestring).into()
    }

    pub fn numelements(&self) -> usize {
        sys::typedesc::typedesc_numelements(&(*self).into())
    }

    pub fn basevalues(&self) -> usize {
        sys::typedesc::typedesc_basevalues(&(*self).into())
    }

    pub fn is_array(&self) -> bool {
        sys::typedesc::typedesc_is_array(&(*self).into())
    }

    pub fn is_unsized_array(&self) -> bool {
        sys::typedesc::typedesc_is_unsized_array(&(*self).into())
    }

    pub fn is_sized_array(&self) -> bool {
        sys::typedesc::typedesc_is_sized_array(&(*self).into())
    }

    pub fn size(&self) -> usize {
        sys::typedesc::typedesc_size(&(*self).into())
    }

    pub fn elementtype(&self) -> Self {
        sys::typedesc::typedesc_elementtype(&(*self).into()).into()
    }

    pub fn elementsize(&self) -> usize {
        sys::typedesc::typedesc_elementsize(&(*self).into())
    }

    pub fn scalartype(&self) -> Self {
        sys::typedesc::typedesc_scalartype(&(*self).into()).into()
    }

    pub fn basesize(&self) -> usize {
        sys::typedesc::typedesc_basesize(&(*self).into())
    }

    pub fn is_floating_point(&self) -> bool {
        sys::typedesc::typedesc_is_floating_point(&(*self).into())
    }

    pub fn is_signed(&self) -> bool {
        sys::typedesc::typedesc_is_signed(&(*self).into())
    }

    pub fn is_unknown(&self) -> bool {
        sys::typedesc::typedesc_is_unknown(&(*self).into())
    }

    pub fn fromstring(&mut self, typestring: &str) -> usize {
        let mut typedesc = sys::typedesc::TypeDesc::from(*self);
        let result = sys::typedesc::typedesc_fromstring(&mut typedesc, typestring);

        *self = typedesc.into();

        result
    }

    pub fn equivalent(&self, b: &TypeDesc) -> bool {
        sys::typedesc::typedesc_equivalent(&(*self).into(), &(*b).into())
    }

    pub fn is_vec2(&self, b: BaseType) -> bool {
        sys::typedesc::typedesc_is_vec2(&(*self).into(), b.into())
    }

    pub fn is_vec3(&self, b: BaseType) -> bool {
        sys::typedesc::typedesc_is_vec3(&(*self).into(), b.into())
    }

    pub fn is_vec4(&self, b: BaseType) -> bool {
        sys::typedesc::typedesc_is_vec4(&(*self).into(), b.into())
    }

    pub fn is_box2(&self, b: BaseType) -> bool {
        sys::typedesc::typedesc_is_box2(&(*self).into(), b.into())
    }

    pub fn is_box3(&self, b: BaseType) -> bool {
        sys::typedesc::typedesc_is_box3(&(*self).into(), b.into())
    }

    pub fn unarray(&mut self) -> () {
        let mut typedesc = sys::typedesc::TypeDesc::from(*self);
        sys::typedesc::typedesc_unarray(&mut typedesc);
        *self = typedesc.into();
    }

    pub fn basetype_merge_2(a: TypeDesc, b: TypeDesc) -> BaseType {
        sys::typedesc::typedesc_basetype_merge_2(a.into(), b.into()).into()
    }

    pub fn basetype_merge_3(a: TypeDesc, b: TypeDesc, c: TypeDesc) -> BaseType {
        {
            sys::typedesc::typedesc_basetype_merge_3(a.into(), b.into(), c.into()).into()
        }
    }

    pub fn convert_type(
        srctype: TypeDesc,
        src: &[u8],
        dsttype: TypeDesc,
        dst: &mut [u8],
        n: usize,
    ) -> bool {
        sys::typedesc::typedesc_convert_type(srctype.into(), src, dsttype.into(), dst, n as i32)
    }
}
