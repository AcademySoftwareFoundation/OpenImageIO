// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


#include <cstdio>
#include <cstdlib>
#include <string>

#include <OpenEXR/half.h>

#include <OpenImageIO/dassert.h>
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
    sizeof(void*)                // PTR
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
        0   // PTR
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
        0   // PTR
    };
    OIIO_DASSERT(basetype < TypeDesc::LASTBASE);
    return issigned[basetype];
}



namespace {

static const char* basetype_name[] = {
    "unknown",  // UNKNOWN
    "void",     // VOID/NONE
    "uint8",    // UCHAR
    "int8",     // CHAR
    "uint16",   // USHORT
    "int16",    // SHORT
    "uint",     // UINT
    "int",      // INT
    "uint64",   // ULONGLONG
    "int64",    // LONGLONG
    "half",     // HALF
    "float",    // FLOAT
    "double",   // DOUBLE
    "string",   // STRING
    "pointer"   // PTR
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
    "ptr"       // PTR
};

}  // namespace

const char*
TypeDesc::c_str() const
{
    // FIXME : how about a per-thread cache of the last one or two, so
    // we don't have to re-assemble strings all the time?

    // Timecode and Keycode are hard coded
    static constexpr TypeDesc TypeTimeCodeAlt(UINT, VEC2, TIMECODE);
    if (*this == TypeTimeCode || *this == TypeTimeCodeAlt)
        return ustring("timecode").c_str();
    else if (*this == TypeKeyCode)
        return ustring("keycode").c_str();

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
    else if (vecsemantics == NOXFORM) {
        switch (aggregate) {
        case VEC2: result = "float2"; break;
        case VEC3: result = "float3"; break;
        case VEC4: result = "float4"; break;
        case MATRIX33: result = "matrix33"; break;
        case MATRIX44: result = "matrix"; break;
        }
        if (basetype != FLOAT)
            result += basetype_code[basetype];
    } else {
        // Special names for vector semantics
        const char* vec = "";
        switch (vecsemantics) {
        case COLOR: vec = "color"; break;
        case POINT: vec = "point"; break;
        case VECTOR: vec = "vector"; break;
        case NORMAL: vec = "normal"; break;
        case RATIONAL: vec = "rational"; break;
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
    if (arraylen > 0)
        result += Strutil::sprintf("[%d]", arraylen);
    else if (arraylen < 0)
        result += "[]";
    return ustring(result).c_str();
}



// Copy src into dst until you hit the end, find a delimiter charcter,
// or have copied maxlen-1 characters, whichever comes first.  Add a
// terminating null charcter.  Return the number of characters copied.
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
        t = TypeColor;
    else if (type == "point")
        t = TypePoint;
    else if (type == "vector")
        t = TypeVector;
    else if (type == "normal")
        t = TypeNormal;
    else if (type == "matrix33")
        t = TypeMatrix33;
    else if (type == "matrix" || type == "matrix44")
        t = TypeMatrix44;
    else if (type == "vector2")
        t = TypeVector2;
    else if (type == "vector4")
        t = TypeVector4;
    else if (type == "timecode")
        t = TypeTimeCode;
    else if (type == "rational")
        t = TypeRational;
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
    const char* array_sep, int flags)
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
{
}



template<class T>
inline std::string
sprintt(TypeDesc type, const char* format, const tostring_formatting& fmt,
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
            val += Strutil::sprintf(format, *v);
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



inline std::string
sprintt(TypeDesc type, const char* format, const tostring_formatting& fmt,
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
    // Perhaps there is a way to use CType<> with a dynamic argument?
    switch (type.basetype) {
    case TypeDesc::UNKNOWN:
        return sprintt(type, fmt.ptr_fmt, fmt, (void**)data);
    case TypeDesc::NONE: return sprintt(type, "None", fmt, (void**)data);
    case TypeDesc::UCHAR:
        return sprintt(type, fmt.int_fmt, fmt, (unsigned char*)data);
    case TypeDesc::CHAR: return sprintt(type, fmt.int_fmt, fmt, (char*)data);
    case TypeDesc::USHORT:
        return sprintt(type, fmt.int_fmt, fmt, (uint16_t*)data);
    case TypeDesc::SHORT: return sprintt(type, fmt.int_fmt, fmt, (short*)data);
    case TypeDesc::UINT:
        if (type.vecsemantics == TypeDesc::RATIONAL
            && type.aggregate == TypeDesc::VEC2) {
            std::string out;
            const uint32_t* val = (const uint32_t*)data;
            for (size_t i = 0, e = type.numelements(); i < e; ++i, val += 2) {
                if (i)
                    out += ", ";
                out += Strutil::sprintf("%d/%d", val[0], val[1]);
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
            return Strutil::sprintf("%02d:%02d:%02d:%02d", hours, minutes,
                                    seconds, frame);
        }
        return sprintt(type, fmt.int_fmt, fmt, (unsigned int*)data);
    case TypeDesc::INT:
        if (type.elementtype() == TypeRational) {
            std::string out;
            const int* val = (const int*)data;
            for (size_t i = 0, e = type.numelements(); i < e; ++i, val += 2) {
                if (i)
                    out += ", ";
                out += Strutil::sprintf("%d/%d", val[0], val[1]);
            }
            return out;
        }
        return sprintt(type, fmt.int_fmt, fmt, (int*)data);
    case TypeDesc::ULONGLONG:
        return sprintt(type, fmt.int_fmt, fmt, (const uint64_t*)data);
    case TypeDesc::LONGLONG:
        return sprintt(type, fmt.int_fmt, fmt, (const int64_t*)data);
    case TypeDesc::HALF:
        return sprintt(type, fmt.float_fmt, fmt, (const half*)data);
    case TypeDesc::FLOAT:
        return sprintt(type, fmt.float_fmt, fmt, (const float*)data);
    case TypeDesc::DOUBLE:
        return sprintt(type, fmt.float_fmt, fmt, (const double*)data);
    case TypeDesc::STRING:
        if (!type.is_array()
            && !(fmt.flags & tostring_formatting::quote_single_string))
            return *(const char**)data;
        return sprintt(type, fmt.string_fmt, fmt, (const char**)data);
    case TypeDesc::PTR: return sprintt(type, fmt.ptr_fmt, fmt, (void**)data);
    default:
#ifndef NDEBUG
        return Strutil::sprintf("<unknown data type> (base %d, agg %d vec %d)",
                                type.basetype, type.aggregate,
                                type.vecsemantics);
#endif
        break;
    }
    return "";
}



// Old deprecated one
std::string
tostring(TypeDesc type, const void* data, const char* float_fmt,
         const char* string_fmt, const char aggregate_delim[2],
         const char* aggregate_sep, const char array_delim[2],
         const char* array_sep)
{
    tostring_formatting fmt("%d", float_fmt, string_fmt, "%p",
                            std::string(aggregate_delim + 0, 1).c_str(),
                            std::string(aggregate_delim + 1, 1).c_str(),
                            aggregate_sep,
                            std::string(array_delim + 0, 1).c_str(),
                            std::string(array_delim + 1, 1).c_str(), array_sep);
    return tostring(type, data, fmt);
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
        // excatly parses to an int value.
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
    // N.B. No uint inversion from string

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
        // excatly parses to a float value.
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



const TypeDesc TypeDesc::TypeFloat(TypeDesc::FLOAT);
const TypeDesc TypeDesc::TypeColor(TypeDesc::FLOAT, TypeDesc::VEC3,
                                   TypeDesc::COLOR);
const TypeDesc TypeDesc::TypePoint(TypeDesc::FLOAT, TypeDesc::VEC3,
                                   TypeDesc::POINT);
const TypeDesc TypeDesc::TypeVector(TypeDesc::FLOAT, TypeDesc::VEC3,
                                    TypeDesc::VECTOR);
const TypeDesc TypeDesc::TypeNormal(TypeDesc::FLOAT, TypeDesc::VEC3,
                                    TypeDesc::NORMAL);
const TypeDesc TypeDesc::TypeMatrix33(TypeDesc::FLOAT, TypeDesc::MATRIX33);
const TypeDesc TypeDesc::TypeMatrix44(TypeDesc::FLOAT, TypeDesc::MATRIX44);
const TypeDesc TypeDesc::TypeMatrix = TypeDesc::TypeMatrix44;
const TypeDesc TypeDesc::TypeString(TypeDesc::STRING);
const TypeDesc TypeDesc::TypeInt(TypeDesc::INT);
const TypeDesc TypeDesc::TypeHalf(TypeDesc::HALF);
const TypeDesc TypeDesc::TypeTimeCode(TypeDesc::UINT, TypeDesc::SCALAR,
                                      TypeDesc::TIMECODE, 2);
const TypeDesc TypeDesc::TypeKeyCode(TypeDesc::INT, TypeDesc::SCALAR,
                                     TypeDesc::KEYCODE, 7);
const TypeDesc TypeDesc::TypeFloat4(TypeDesc::FLOAT, TypeDesc::VEC4);
const TypeDesc TypeDesc::TypeRational(TypeDesc::INT, TypeDesc::VEC2,
                                      TypeDesc::RATIONAL);


OIIO_NAMESPACE_END
