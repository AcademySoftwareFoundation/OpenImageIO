// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <cstdio>
#include <cstdlib>
#include <string>

#include <OpenImageIO/detail/fmt.h>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/half.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/ustring.h>


OIIO_NAMESPACE_BEGIN

TypeDesc::TypeDesc(string_view typestring)
    : basetype(UNKNOWN)
    , aggregate(SCALAR)
    , vecsemantics(NOXFORM)
    , reserved(0)
    , arraylen(0)
{
    fromstring(typestring);
}



namespace {

static int basetype_size[TypeDesc::LASTBASE] = {
    0,                           // UNKNOWN
    0,                           // VOID
    sizeof(unsigned char),       // UCHAR
    sizeof(char),                // CHAR
    sizeof(unsigned short),      // USHORT
    sizeof(short),               // SHORT
    sizeof(unsigned int),        // UINT
    sizeof(int),                 // INT
    sizeof(unsigned long long),  // ULONGLONG
    sizeof(long long),           // LONGLONG
    sizeof(float) / 2,           // HALF
    sizeof(float),               // FLOAT
    sizeof(double),              // DOUBLE
    sizeof(char*),               // STRING
    sizeof(void*),               // PTR
    sizeof(ustringhash),         // USTRINGHASH
};

}

size_t
TypeDesc::basesize() const noexcept
{
    if (basetype >= TypeDesc::LASTBASE)
        return 0;
    OIIO_DASSERT(basetype < TypeDesc::LASTBASE);
    return basetype_size[basetype];
}



bool
TypeDesc::is_floating_point() const noexcept
{
    static bool isfloat[TypeDesc::LASTBASE] = {
        0,  // UNKNOWN
        0,  // VOID
        0,  // UCHAR
        0,  // CHAR
        0,  // USHORT
        0,  // SHORT
        0,  // UINT
        0,  // INT
        0,  // ULONGLONG
        0,  // LONGLONG
        1,  // HALF
        1,  // FLOAT
        1,  // DOUBLE
        0,  // STRING
        0,  // PTR
        0,  // USTRINGHASH
    };
    OIIO_DASSERT(basetype < TypeDesc::LASTBASE);
    return isfloat[basetype];
}



bool
TypeDesc::is_signed() const noexcept
{
    static bool issigned[TypeDesc::LASTBASE] = {
        0,  // UNKNOWN
        0,  // VOID
        0,  // UCHAR
        1,  // CHAR
        0,  // USHORT
        1,  // SHORT
        0,  // UINT
        1,  // INT
        0,  // ULONGLONG
        1,  // LONGLONG
        1,  // HALF
        1,  // FLOAT
        1,  // DOUBLE
        0,  // STRING
        0,  // PTR
        0,  // USTRINGHASH
    };
    OIIO_DASSERT(basetype < TypeDesc::LASTBASE);
    return issigned[basetype];
}



namespace {

static const char* basetype_name[] = {
    "unknown",      // UNKNOWN
    "void",         // VOID/NONE
    "uint8",        // UCHAR
    "int8",         // CHAR
    "uint16",       // USHORT
    "int16",        // SHORT
    "uint",         // UINT
    "int",          // INT
    "uint64",       // ULONGLONG
    "int64",        // LONGLONG
    "half",         // HALF
    "float",        // FLOAT
    "double",       // DOUBLE
    "string",       // STRING
    "pointer",      // PTR
    "ustringhash",  // USTRINGHASH
};

static const char* basetype_code[] = {
    "unknown",  // UNKNOWN
    "void",     // VOID/NONE
    "uc",       // UCHAR
    "c",        // CHAR
    "us",       // USHORT
    "s",        // SHORT
    "ui",       // UINT
    "i",        // INT
    "ull",      // ULONGLONG
    "ll",       // LONGLONG
    "h",        // HALF
    "f",        // FLOAT
    "d",        // DOUBLE
    "str",      // STRING
    "ptr",      // PTR
    "uh",       // USTRINGHASH
};

}  // namespace

const char*
TypeDesc::c_str() const
{
    // FIXME : how about a per-thread cache of the last one or two, so
    // we don't have to re-assemble strings all the time?

    // Timecode and Keycode are hard coded
    if (vecsemantics == TIMECODE && (basetype == INT || basetype == UINT)
        && basevalues() == 2) {
        return ustring("timecode").c_str();
    }
    if (vecsemantics == KEYCODE && (basetype == INT || basetype == UINT)
        && basevalues() == 7) {
        return ustring("keycode").c_str();
    }

    int alen = arraylen;
    std::string result;
    if (aggregate == SCALAR)
        result = basetype_name[basetype];
    // else if (aggregate == MATRIX44 && basetype == FLOAT)
    //     result = "matrix";
    // else if (aggregate == MATRIX33 && basetype == FLOAT)
    //     result = "matrix33";
    // else if (aggregate == VEC2 && basetype == FLOAT && vecsemantics == NOXFORM)
    //     result = "float2";
    // else if (aggregate == VEC4 && basetype == FLOAT && vecsemantics == NOXFORM)
    //     result = "float4";
    else if (vecsemantics == NOXFORM && basetype == FLOAT) {
        switch (aggregate) {
        case VEC2: result = "float2"; break;
        case VEC3: result = "float3"; break;
        case VEC4: result = "float4"; break;
        case MATRIX33: result = "matrix33"; break;
        case MATRIX44: result = "matrix"; break;
        }
        if (basetype != FLOAT)
            result += basetype_code[basetype];
    } else if (vecsemantics == NOXFORM) {
        switch (aggregate) {
        case VEC2:
        case VEC3:
        case VEC4:
            result = Strutil::fmt::format("vector{}{}", int(aggregate),
                                          basetype_code[basetype]);
            break;
        case MATRIX33:
            result = Strutil::fmt::format("matrix33{}",
                                          basetype_code[basetype]);
            break;
        case MATRIX44:
            result = Strutil::fmt::format("matrix{}", basetype_code[basetype]);
            break;
        }
    } else {
        // Special names for vector semantics
        const char* vec = "";
        switch (vecsemantics) {
        case COLOR: vec = "color"; break;
        case POINT: vec = "point"; break;
        case VECTOR: vec = "vector"; break;
        case NORMAL: vec = "normal"; break;
        case RATIONAL: vec = "rational"; break;
        case BOX: vec = ""; break;
        default: OIIO_DASSERT(0 && "Invalid vector semantics");
        }
        const char* agg = "";
        switch (aggregate) {
        case VEC2: agg = "2"; break;
        case VEC4: agg = "4"; break;
        case MATRIX33: agg = "matrix33"; break;
        case MATRIX44: agg = "matrix44"; break;
        }
        result = std::string(vec) + std::string(agg);
        if (basetype != FLOAT)
            result += basetype_code[basetype];
    }
    // More unusual cases
    if (vecsemantics == BOX) {
        result = Strutil::fmt::format("box{}{}", int(aggregate),
                                      basetype == FLOAT
                                          ? ""
                                          : basetype_code[basetype]);
        alen   = arraylen > 2 ? (arraylen / 2) : (arraylen < 0 ? -1 : 0);
    }
    if (alen > 0)
        result += Strutil::fmt::format("[{}]", alen);
    else if (alen < 0)
        result += "[]";
    return ustring(result).c_str();
}



// Copy src into dst until you hit the end, find a delimiter character,
// or have copied maxlen-1 characters, whichever comes first.  Add a
// terminating null character.  Return the number of characters copied.
inline size_t
copy_until(const char* src, const char* delim, char* dst, size_t maxlen)
{
    size_t i = 0;
    while (src[i] && i < maxlen - 1) {
        bool found_delim = false;
        for (int d = 0; delim[d]; ++d)
            if (src[i] == delim[d])
                found_delim = true;
        if (found_delim)
            break;
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
    return i;
}



size_t
TypeDesc::fromstring(string_view typestring)
{
    *this            = TypeDesc::UNKNOWN;
    string_view orig = typestring;
    if (typestring.empty()) {
        return 0;
    }

    // The first "word" should be a type name.
    string_view type = Strutil::parse_identifier(typestring);

    // Check the scalar types in our table above
    TypeDesc t;
    for (int i = 0; i < LASTBASE; ++i) {
        if (type == basetype_name[i]) {
            t.basetype = i;
            break;
        }
    }

    // Some special case names for aggregates
    if (t.basetype != UNKNOWN) {
        // already solved
    } else if (type == "color")
        t = OIIO::TypeColor;
    else if (type == "point")
        t = OIIO::TypePoint;
    else if (type == "vector")
        t = OIIO::TypeVector;
    else if (type == "normal")
        t = OIIO::TypeNormal;
    else if (type == "matrix33")
        t = OIIO::TypeMatrix33;
    else if (type == "matrix" || type == "matrix44")
        t = OIIO::TypeMatrix44;
    else if (type == "vector2")
        t = OIIO::TypeVector2;
    else if (type == "vector4")
        t = OIIO::TypeVector4;
    else if (type == "float2")
        t = OIIO::TypeFloat2;
    else if (type == "float4")
        t = OIIO::TypeFloat4;
    else if (type == "timecode")
        t = OIIO::TypeTimeCode;
    else if (type == "rational")
        t = OIIO::TypeRational;
    else if (type == "box2i")
        t = OIIO::TypeBox2i;
    else if (type == "box3i")
        t = OIIO::TypeBox3i;
    else if (type == "box2" || type == "box2f")
        t = OIIO::TypeBox2;
    else if (type == "box3" || type == "box3f")
        t = OIIO::TypeBox3;
    else if (type == "timecode")
        t = OIIO::TypeTimeCode;
    else if (type == "keycode")
        t = OIIO::TypeKeyCode;
    else if (type == "pointer")
        t = OIIO::TypePointer;
    else if (type == "ustringhash")
        t = OIIO::TypeUstringhash;
    else {
        return 0;  // unknown
    }

    // Is there an array length following the type name?
    if (Strutil::parse_char(typestring, '[')) {
        int arraylen = -1;
        Strutil::parse_int(typestring, arraylen);
        if (!Strutil::parse_char(typestring, ']'))
            return 0;  // malformed
        t.arraylen = arraylen;
    }

    *this = t;
    return orig.length() - typestring.length();
}



tostring_formatting::tostring_formatting(
    const char* int_fmt, const char* float_fmt, const char* string_fmt,
    const char* ptr_fmt, const char* aggregate_begin, const char* aggregate_end,
    const char* aggregate_sep, const char* array_begin, const char* array_end,
    const char* array_sep, int flags, const char* uint_fmt)
    : int_fmt(int_fmt)
    , float_fmt(float_fmt)
    , string_fmt(string_fmt)
    , ptr_fmt(ptr_fmt)
    , aggregate_begin(aggregate_begin)
    , aggregate_end(aggregate_end)
    , aggregate_sep(aggregate_sep)
    , array_begin(array_begin)
    , array_end(array_end)
    , array_sep(array_sep)
    , flags(flags)
    , uint_fmt(uint_fmt)
{
}



tostring_formatting::tostring_formatting(
    Notation notation, const char* int_fmt, const char* uint_fmt,
    const char* float_fmt, const char* string_fmt, const char* ptr_fmt,
    const char* aggregate_begin, const char* aggregate_end,
    const char* aggregate_sep, const char* array_begin, const char* array_end,
    const char* array_sep, int flags)
    : tostring_formatting(int_fmt, float_fmt, string_fmt, ptr_fmt,
                          aggregate_begin, aggregate_end, aggregate_sep,
                          array_begin, array_end, array_sep, flags, uint_fmt)
{
    use_sprintf = false;
}



template<class T, class Cast = T>
static std::string
sprint_type(TypeDesc type, const char* format, const tostring_formatting& fmt,
            const T* v)
{
    std::string val;
    if (type.arraylen)
        val += fmt.array_begin;
    const size_t n = type.arraylen ? type.arraylen : 1;
    for (size_t i = 0; i < n; ++i) {
        if (type.aggregate > 1)
            val += fmt.aggregate_begin;
        for (int j = 0; j < (int)type.aggregate; ++j, ++v) {
            val += Strutil::sprintf(format, static_cast<Cast>(*v));
            if (type.aggregate > 1 && j < type.aggregate - 1)
                val += fmt.aggregate_sep;
        }
        if (type.aggregate > 1)
            val += fmt.aggregate_end;
        if (i < n - 1)
            val += fmt.array_sep;
    }
    if (type.arraylen)
        val += fmt.array_end;
    return val;
}



static std::string
sprint_type(TypeDesc type, const char* format, const tostring_formatting& fmt,
            const char** v)
{
    std::string val;
    if (type.arraylen)
        val += fmt.array_begin;
    const size_t n = type.arraylen ? type.arraylen : 1;
    for (size_t i = 0; i < n; ++i) {
        if (type.aggregate > 1)
            val += fmt.aggregate_begin;
        for (int j = 0; j < (int)type.aggregate; ++j, ++v) {
            if (fmt.flags & tostring_formatting::escape_strings)
                val += Strutil::sprintf(format, *v ? Strutil::escape_chars(*v)
                                                   : std::string());
            else
                val += Strutil::sprintf(format, *v ? *v : "");
            if (type.aggregate > 1 && j < type.aggregate - 1)
                val += fmt.aggregate_sep;
        }
        if (type.aggregate > 1)
            val += fmt.aggregate_end;
        if (i < n - 1)
            val += fmt.array_sep;
    }
    if (type.arraylen)
        val += fmt.array_end;
    return val;
}



template<class T>
static std::string
format_type(TypeDesc type, const char* format, const tostring_formatting& fmt,
            const T* v)
{
    std::string val;
    if (type.arraylen)
        val += fmt.array_begin;
    const size_t n = type.arraylen ? type.arraylen : 1;
    for (size_t i = 0; i < n; ++i) {
        if (type.aggregate > 1)
            val += fmt.aggregate_begin;
        for (int j = 0; j < (int)type.aggregate; ++j, ++v) {
            val += Strutil::fmt::format(format, *v);
            if (type.aggregate > 1 && j < type.aggregate - 1)
                val += fmt.aggregate_sep;
        }
        if (type.aggregate > 1)
            val += fmt.aggregate_end;
        if (i < n - 1)
            val += fmt.array_sep;
    }
    if (type.arraylen)
        val += fmt.array_end;
    return val;
}



static std::string
format_type(TypeDesc type, const char* format, const tostring_formatting& fmt,
            const char** v)
{
    std::string val;
    if (type.arraylen)
        val += fmt.array_begin;
    const size_t n = type.arraylen ? type.arraylen : 1;
    for (size_t i = 0; i < n; ++i) {
        if (type.aggregate > 1)
            val += fmt.aggregate_begin;
        for (int j = 0; j < (int)type.aggregate; ++j, ++v) {
            if (fmt.flags & tostring_formatting::escape_strings)
                val += Strutil::fmt::format(format,
                                            *v ? Strutil::escape_chars(*v)
                                               : std::string());
            else
                val += Strutil::fmt::format(format, *v ? *v : "");
            if (type.aggregate > 1 && j < type.aggregate - 1)
                val += fmt.aggregate_sep;
        }
        if (type.aggregate > 1)
            val += fmt.aggregate_end;
        if (i < n - 1)
            val += fmt.array_sep;
    }
    if (type.arraylen)
        val += fmt.array_end;
    return val;
}



// From OpenEXR
inline unsigned int
bitField(unsigned int value, int minBit, int maxBit)
{
    int shift         = minBit;
    unsigned int mask = (~(~0U << (maxBit - minBit + 1)) << minBit);
    return (value & mask) >> shift;
}


// From OpenEXR
inline int
bcdToBinary(unsigned int bcd)
{
    return int((bcd & 0x0f) + 10 * ((bcd >> 4) & 0x0f));
}



std::string
tostring(TypeDesc type, const void* data, const tostring_formatting& fmt)
{
    if (!data)
        return std::string();
    // Perhaps there is a way to use CType<> with a dynamic argument?
    switch (type.basetype) {
    case TypeDesc::UNKNOWN:
        return fmt.use_sprintf
                   ? sprint_type(type, fmt.ptr_fmt, fmt, (void**)data)
                   : format_type(type, fmt.ptr_fmt, fmt, (void**)data);
    case TypeDesc::NONE:
        return fmt.use_sprintf ? sprint_type(type, "None", fmt, (void**)data)
                               : format_type(type, "None", fmt, (void**)data);
    case TypeDesc::UCHAR:
        return fmt.use_sprintf
                   ? sprint_type(type, fmt.uint_fmt ? fmt.uint_fmt : "%u", fmt,
                                 (unsigned char*)data)
                   : format_type(type, fmt.uint_fmt ? fmt.uint_fmt : "%u", fmt,
                                 (unsigned char*)data);
    case TypeDesc::CHAR:
        return fmt.use_sprintf
                   ? sprint_type(type, fmt.int_fmt, fmt, (char*)data)
                   : format_type(type, "{:d}", fmt, (char*)data);
    case TypeDesc::USHORT:
        return fmt.use_sprintf
                   ? sprint_type(type, fmt.uint_fmt ? fmt.uint_fmt : "%u", fmt,
                                 (uint16_t*)data)
                   : format_type(type, fmt.uint_fmt ? fmt.uint_fmt : "%u", fmt,
                                 (uint16_t*)data);
    case TypeDesc::SHORT:
        return fmt.use_sprintf
                   ? sprint_type(type, fmt.int_fmt, fmt, (short*)data)
                   : format_type(type, fmt.int_fmt, fmt, (short*)data);
    case TypeDesc::UINT:
        if (type.vecsemantics == TypeDesc::RATIONAL
            && type.aggregate == TypeDesc::VEC2) {
            std::string out;
            const uint32_t* val = (const uint32_t*)data;
            for (size_t i = 0, e = type.numelements(); i < e; ++i, val += 2) {
                if (i)
                    out += ", ";
                out += Strutil::fmt::format("{}/{}", val[0], val[1]);
            }
            return out;
        } else if (type == TypeTimeCode) {
            // Replicating the logic in OpenEXR, but this prevents us from
            // needing to link to libIlmImf just to do this.
            unsigned int t = *(unsigned int*)data;
            int hours      = bcdToBinary(bitField(t, 24, 29));
            int minutes    = bcdToBinary(bitField(t, 16, 22));
            int seconds    = bcdToBinary(bitField(t, 8, 14));
            int frame      = bcdToBinary(bitField(t, 0, 5));
            return Strutil::fmt::format("{:02d}:{:02d}:{:02d}:{:02d}", hours,
                                        minutes, seconds, frame);
        }
        return fmt.use_sprintf
                   ? sprint_type(type, fmt.uint_fmt ? fmt.uint_fmt : "%u", fmt,
                                 (unsigned int*)data)
                   : format_type(type, fmt.uint_fmt ? fmt.uint_fmt : "%u", fmt,
                                 (unsigned int*)data);
    case TypeDesc::INT:
        if (type.elementtype() == TypeRational) {
            std::string out;
            const int* val = (const int*)data;
            for (size_t i = 0, e = type.numelements(); i < e; ++i, val += 2) {
                if (i)
                    out += ", ";
                out += Strutil::fmt::format("{}/{}", val[0], val[1]);
            }
            return out;
        }
        return fmt.use_sprintf
                   ? sprint_type(type, fmt.int_fmt, fmt, (int*)data)
                   : format_type(type, fmt.int_fmt, fmt, (int*)data);
    case TypeDesc::UINT64:
        return fmt.use_sprintf
                   ? sprint_type(type, fmt.uint_fmt ? fmt.uint_fmt : "%u", fmt,
                                 (const uint64_t*)data)
                   : format_type(type, fmt.uint_fmt ? fmt.uint_fmt : "%u", fmt,
                                 (const uint64_t*)data);
    case TypeDesc::INT64:
        return fmt.use_sprintf
                   ? sprint_type(type, fmt.int_fmt, fmt, (const int64_t*)data)
                   : format_type(type, fmt.int_fmt, fmt, (const int64_t*)data);
    case TypeDesc::HALF:
        return fmt.use_sprintf
                   ? sprint_type<half, float>(type, fmt.float_fmt, fmt,
                                              (const half*)data)
                   : format_type(type, fmt.float_fmt, fmt, (const half*)data);
    case TypeDesc::FLOAT:
        return fmt.use_sprintf
                   ? sprint_type(type, fmt.float_fmt, fmt, (const float*)data)
                   : format_type(type, fmt.float_fmt, fmt, (const float*)data);
    case TypeDesc::DOUBLE:
        return fmt.use_sprintf
                   ? sprint_type(type, fmt.float_fmt, fmt, (const double*)data)
                   : format_type(type, fmt.float_fmt, fmt, (const double*)data);
    case TypeDesc::STRING:
        if (!type.is_array()
            && !(fmt.flags & tostring_formatting::quote_single_string))
            return *(const char**)data ? *(const char**)data : "";
        return fmt.use_sprintf
                   ? sprint_type(type, fmt.string_fmt, fmt, (const char**)data)
                   : format_type(type, fmt.string_fmt, fmt, (const char**)data);
    case TypeDesc::PTR:
        return fmt.use_sprintf
                   ? sprint_type(type, fmt.ptr_fmt, fmt, (void**)data)
                   : format_type(type, fmt.ptr_fmt, fmt, (void**)data);
    case TypeDesc::USTRINGHASH: {
        const char* v = ((const ustringhash*)data)->c_str();
        if (!type.is_array()
            && !(fmt.flags & tostring_formatting::quote_single_string))
            return v ? v : "";
        return fmt.use_sprintf ? sprint_type(type, fmt.string_fmt, fmt, &v)
                               : format_type(type, fmt.string_fmt, fmt, &v);
    }
    default:
#ifndef NDEBUG
        return Strutil::fmt::format(
            "<unknown data type> (base {}, agg {} vec {})", type.basetype,
            type.aggregate, type.vecsemantics);
#endif
        break;
    }
    return "";
}



namespace {

template<typename T = int>
static bool
to_ints(TypeDesc srctype, const void* src, T* dst, size_t n = 1)
{
    if (srctype.basetype == TypeDesc::UINT) {
        for (size_t i = 0; i < n; ++i)
            dst[i] = T(((const unsigned int*)src)[i]);
        return true;
    }
    if (srctype.basetype == TypeDesc::INT16) {
        for (size_t i = 0; i < n; ++i)
            dst[i] = T(((const short*)src)[i]);
        return true;
    }
    if (srctype.basetype == TypeDesc::UINT16) {
        for (size_t i = 0; i < n; ++i)
            dst[i] = T(((const unsigned short*)src)[i]);
        return true;
    }
    if (srctype.basetype == TypeDesc::INT8) {
        for (size_t i = 0; i < n; ++i)
            dst[i] = T(((const char*)src)[i]);
        return true;
    }
    if (srctype.basetype == TypeDesc::UINT8) {
        for (size_t i = 0; i < n; ++i)
            dst[i] = T(((const unsigned char*)src)[i]);
        return true;
    }
    if (srctype.basetype == TypeDesc::INT64) {
        for (size_t i = 0; i < n; ++i)
            dst[i] = T(((const long long*)src)[i]);
        return true;
    }
    if (srctype.basetype == TypeDesc::UINT64) {
        for (size_t i = 0; i < n; ++i)
            dst[i] = T(((const unsigned long long*)src)[i]);
        return true;
    }
    return false;
}


template<typename T = float>
static bool
to_floats(TypeDesc srctype, const void* src, T* dst, size_t n = 1)
{
    if (srctype.basetype == TypeDesc::FLOAT) {
        for (size_t i = 0; i < n; ++i)
            dst[i] = T(((const float*)src)[i]);
        return true;
    }
    if (srctype.basetype == TypeDesc::HALF) {
        for (size_t i = 0; i < n; ++i)
            dst[i] = T(((const half*)src)[i]);
        return true;
    }
    if (srctype.basetype == TypeDesc::DOUBLE) {
        for (size_t i = 0; i < n; ++i)
            dst[i] = T(((const double*)src)[i]);
        return true;
    }
    if (srctype.basetype == TypeDesc::UINT) {
        for (size_t i = 0; i < n; ++i)
            dst[i] = T(((const unsigned int*)src)[i]);
        return true;
    }
    if (srctype.basetype == TypeDesc::INT) {
        for (size_t i = 0; i < n; ++i)
            dst[i] = T(((const int*)src)[i]);
        return true;
    }
    if (srctype.basetype == TypeDesc::INT16) {
        for (size_t i = 0; i < n; ++i)
            dst[i] = T(((const short*)src)[i]);
        return true;
    }
    if (srctype.basetype == TypeDesc::UINT16) {
        for (size_t i = 0; i < n; ++i)
            dst[i] = T(((const unsigned short*)src)[i]);
        return true;
    }
    if (srctype.basetype == TypeDesc::INT8) {
        for (size_t i = 0; i < n; ++i)
            dst[i] = T(((const char*)src)[i]);
        return true;
    }
    if (srctype.basetype == TypeDesc::UINT8) {
        for (size_t i = 0; i < n; ++i)
            dst[i] = T(((const unsigned char*)src)[i]);
        return true;
    }
    if (srctype.basetype == TypeDesc::INT64) {
        for (size_t i = 0; i < n; ++i)
            dst[i] = T(((const long long*)src)[i]);
        return true;
    }
    if (srctype.basetype == TypeDesc::UINT64) {
        for (size_t i = 0; i < n; ++i)
            dst[i] = T(((const unsigned long long*)src)[i]);
        return true;
    }
    return false;
}

}  // namespace



bool
convert_type(TypeDesc srctype, const void* src, TypeDesc dsttype, void* dst,
             int n)
{
    if (n > 1) {
        // Handle multiple values by turning into or expanding array length
        srctype.arraylen = srctype.numelements() * n;
        dsttype.arraylen = dsttype.numelements() * n;
    }

    if (srctype.basetype == dsttype.basetype
        && srctype.basevalues() == dsttype.basevalues()) {
        size_t size = srctype.size();
        memcpy(dst, src, size);
        return size;
    }

    if (dsttype == TypeString) {
        if (srctype == TypeUstringhash)
            (*(ustring*)dst) = ustring::from_hash(*(const ustring::hash_t*)src);
        else
            (*(ustring*)dst) = ustring(tostring(srctype, src));
        return true;
    }

    if (dsttype.basetype == TypeDesc::INT
        && dsttype.basevalues() == srctype.basevalues()) {
        if (to_ints<int>(srctype, src, (int*)dst, dsttype.basevalues()))
            return true;
    }
    if (dsttype == TypeInt && srctype == TypeString) {
        // Only succeed for a string if it exactly holds something that
        // exactly parses to an int value.
        string_view str(((const char**)src)[0]);
        int val = 0;
        if (Strutil::parse_int(str, val) && str.empty()) {
            ((int*)dst)[0] = val;
            return true;
        }
    }
    if (dsttype.basetype == TypeDesc::UINT
        && dsttype.basevalues() == srctype.basevalues()) {
        if (to_ints<uint32_t>(srctype, src, (uint32_t*)dst,
                              dsttype.basevalues()))
            return true;
    }
    // N.B. No uint conversion from string

    if (dsttype.basetype == TypeDesc::FLOAT
        && dsttype.basevalues() == srctype.basevalues()) {
        if (to_floats<float>(srctype, src, (float*)dst, dsttype.basevalues()))
            return true;
    }
    if (dsttype == TypeFloat && srctype == TypeRational) {
        int num          = ((const int*)src)[0];
        int den          = ((const int*)src)[1];
        ((float*)dst)[0] = den ? float(num) / float(den) : 0.0f;
        return true;
    }
    if (dsttype == TypeFloat && srctype == TypeString) {
        // Only succeed for a string if it exactly holds something that
        // exactly parses to a float value.
        string_view str(((const char**)src)[0]);
        float val = 0;
        if (Strutil::parse_float(str, val) && str.empty()) {
            ((float*)dst)[0] = val;
            return true;
        }
    }

    if (dsttype.basetype == TypeDesc::DOUBLE
        && dsttype.basevalues() == srctype.basevalues()) {
        if (to_floats<double>(srctype, src, (double*)dst, dsttype.basevalues()))
            return true;
    }
    return false;
}



bool
TypeDesc::operator<(const TypeDesc& x) const noexcept
{
    if (basetype != x.basetype)
        return basetype < x.basetype;
    if (aggregate != x.aggregate)
        return aggregate < x.aggregate;
    if (arraylen != x.arraylen)
        return arraylen < x.arraylen;
    if (vecsemantics != x.vecsemantics)
        return vecsemantics < x.vecsemantics;
    return false;  // they are equal
}



TypeDesc::BASETYPE
TypeDesc::basetype_merge(TypeDesc at, TypeDesc bt)
{
    BASETYPE a = (BASETYPE)at.basetype;
    BASETYPE b = (BASETYPE)bt.basetype;

    // Same type already? done.
    if (a == b)
        return a;
    if (a == UNKNOWN)
        return b;
    if (b == UNKNOWN)
        return a;
    // Canonicalize so a's size (in bytes) is >= b's size in bytes. This
    // unclutters remaining cases.
    if (TypeDesc(a).size() < TypeDesc(b).size())
        std::swap(a, b);
    // Double or float trump anything else
    if (a == DOUBLE || a == FLOAT)
        return a;
    if (a == UINT32 && (b == UINT16 || b == UINT8))
        return a;
    if (a == INT32 && (b == INT16 || b == UINT16 || b == INT8 || b == UINT8))
        return a;
    if ((a == UINT16 || a == HALF) && b == UINT8)
        return a;
    if ((a == INT16 || a == HALF) && (b == INT8 || b == UINT8))
        return a;
    // Out of common cases. For all remaining edge cases, punt and say that
    // we prefer float.
    return FLOAT;
}

OIIO_NAMESPACE_END
