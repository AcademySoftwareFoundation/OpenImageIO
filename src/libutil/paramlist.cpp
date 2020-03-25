// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include <OpenEXR/half.h>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/paramlist.h>
#include <OpenImageIO/ustring.h>


OIIO_NAMESPACE_BEGIN


void
ParamValue::init_noclear(ustring _name, TypeDesc _type, int _nvalues,
                         const void* _value, bool _copy) noexcept
{
    init_noclear(_name, _type, _nvalues, INTERP_CONSTANT, _value, _copy);
}



void
ParamValue::init_noclear(ustring _name, TypeDesc _type, int _nvalues,
                         Interp _interp, const void* _value,
                         bool _copy) noexcept
{
    m_name      = _name;
    m_type      = _type;
    m_nvalues   = _nvalues;
    m_interp    = _interp;
    size_t n    = (size_t)(m_nvalues * m_type.numelements());
    size_t size = (size_t)(m_nvalues * m_type.size());
    bool small  = (size <= sizeof(m_data));

    if (_copy || small) {
        if (small) {
            if (_value)
                memcpy(&m_data, _value, size);
            else
                memset(&m_data, 0, sizeof(m_data));
            m_copy     = false;
            m_nonlocal = false;
        } else {
            m_data.ptr = malloc(size);
            if (_value)
                memcpy((char*)m_data.ptr, _value, size);
            else
                memset((char*)m_data.ptr, 0, size);
            m_copy     = true;
            m_nonlocal = true;
        }
        if (m_type.basetype == TypeDesc::STRING) {
            ustring* u = (ustring*)data();
            for (size_t i = 0; i < n; ++i)
                if (u)
                    u[i] = ustring(u[i].c_str());
        }
    } else {
        // Big enough to warrant a malloc, but the caller said don't
        // make a copy
        m_data.ptr = _value;
        m_copy     = false;
        m_nonlocal = true;
    }
}



// helper to parse a list from a string
template<class T>
static void
parse_elements(const char* type_code, string_view value, ParamValue& p)
{
    TypeDesc type = p.type();
    int num_items = type.numelements() * type.aggregate;
    T* data       = (T*)p.data();
    // Erase any leading whitespace
    value.remove_prefix(value.find_first_not_of(" \t"));
    for (int i = 0; i < num_items; ++i) {
        // Make a temporary copy so we for sure have a 0-terminated string.
        std::string temp = value;
        // Grab the first value from it
        sscanf(temp.c_str(), type_code, &data[i]);
        // Skip the value (eat until we find a delimiter -- space, comma, tab)
        value.remove_prefix(value.find_first_of(" ,\t"));
        // Skip the delimiter
        value.remove_prefix(value.find_first_not_of(" ,\t"));
        if (value.empty())
            break;  // done if nothing left to parse
    }
}



// Construct from parsed string
ParamValue::ParamValue(string_view name, TypeDesc type, string_view value)
    : ParamValue(name, type, 1, nullptr)
{
    if (type.basetype == TypeDesc::INT) {
        parse_elements<int>("%d", value, *this);
    } else if (type.basetype == TypeDesc::UINT) {
        parse_elements<unsigned int>("%u", value, *this);
    } else if (type.basetype == TypeDesc::FLOAT) {
        parse_elements<float>("%f", value, *this);
    } else if (type.basetype == TypeDesc::DOUBLE) {
        parse_elements<double>("%lf", value, *this);
    } else if (type.basetype == TypeDesc::INT64) {
        parse_elements<long long>("%lld", value, *this);
    } else if (type.basetype == TypeDesc::UINT64) {
        parse_elements<unsigned long long>("%llu", value, *this);
    } else if (type.basetype == TypeDesc::INT16) {
        parse_elements<short>("%hd", value, *this);
    } else if (type.basetype == TypeDesc::UINT16) {
        parse_elements<unsigned short>("%hu", value, *this);
    } else if (type == TypeDesc::STRING) {
        ustring s(value);
        init(name, type, 1, &s);
    }
}



int
ParamValue::get_int(int defaultval) const
{
    return get_int_indexed(0, defaultval);
}



int
ParamValue::get_int_indexed(int index, int defaultval) const
{
#if 1 && OIIO_VERSION >= 20101
    int val = defaultval;
    convert_type(type().elementtype(),
                 (const char*)data() + index * type().basesize(), TypeInt,
                 &val);
    return val;
#else
    int base = type().basetype;
    if (base == TypeDesc::INT)
        return get<int>(index);
    if (base == TypeDesc::UINT)
        return (int)get<unsigned int>(index);
    if (base == TypeDesc::INT16)
        return get<short>(index);
    if (base == TypeDesc::UINT16)
        return get<unsigned short>(index);
    if (base == TypeDesc::INT8)
        return get<char>(index);
    if (base == TypeDesc::UINT8)
        return get<unsigned char>(index);
    if (base == TypeDesc::INT64)
        return get<long long>(index);
    if (base == TypeDesc::UINT64)
        return get<unsigned long long>(index);
    if (base == TypeDesc::STRING) {
        // Only succeed for a string if it exactly holds something that
        // excatly parses to an int value.
        string_view str = get<ustring>(index);
        int val         = defaultval;
        if (Strutil::parse_int(str, val) && str.empty())
            return val;
    }
    return defaultval;  // Some nonstandard type, fail
#endif
}



float
ParamValue::get_float(float defaultval) const
{
    return get_float_indexed(0, defaultval);
}



float
ParamValue::get_float_indexed(int index, float defaultval) const
{
#if 1 && OIIO_VERSION >= 20101
    float val = defaultval;
    convert_type(type().elementtype(),
                 (const char*)data() + index * type().basesize(), TypeFloat,
                 &val);
    return val;
#else
    int base = type().basetype;
    if (base == TypeDesc::FLOAT)
        return get<float>(index);
    if (base == TypeDesc::HALF)
        return get<half>(index);
    if (base == TypeDesc::DOUBLE)
        return get<double>(index);
    if (base == TypeDesc::INT) {
        if (type().aggregate == TypeDesc::VEC2
            && type().vecsemantics == TypeDesc::RATIONAL) {
            int num = get<int>(2 * index + 0);
            int den = get<int>(2 * index + 1);
            return den ? float(num) / float(den) : 0.0f;
        }
        return get<int>(index);
    }
    if (base == TypeDesc::UINT)
        return get<unsigned int>(index);
    if (base == TypeDesc::INT16)
        return get<short>(index);
    if (base == TypeDesc::UINT16)
        return get<unsigned short>(index);
    if (base == TypeDesc::INT8)
        return get<char>(index);
    if (base == TypeDesc::UINT8)
        return get<unsigned char>(index);
    if (base == TypeDesc::INT64)
        return get<long long>(index);
    if (base == TypeDesc::UINT64)
        return get<unsigned long long>(index);
    if (base == TypeDesc::STRING) {
        // Only succeed for a string if it exactly holds something
        // that excatly parses to a float value.
        string_view str = get<ustring>(index);
        float val       = defaultval;
        if (Strutil::parse_float(str, val) && str.empty())
            return val;
    }

    return defaultval;
#endif
}



namespace {  // make an anon namespace

template<typename T>
void
formatType(const ParamValue& p, int beginindex, int endindex,
           const char* formatString, std::string& out)
{
    TypeDesc element = p.type().elementtype();
    const T* f       = (const T*)p.data() + beginindex * element.aggregate;
    for (int i = beginindex; i < endindex; ++i) {
        if (i > beginindex)
            out += ", ";
        for (int c = 0; c < (int)element.aggregate; ++c, ++f) {
            if (c)
                out += " ";
            out += Strutil::sprintf(formatString, f[0]);
        }
    }
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

}  // namespace



std::string
ParamValue::get_string(int maxsize) const
{
#if OIIO_VERSION >= 20101
    int nfull  = int(type().numelements()) * nvalues();
    int n      = std::min(nfull, maxsize);
    TypeDesc t = type();
    if (nvalues() > 1 || n < nfull)
        t.arraylen = n;
    static const tostring_formatting fmt
        = { "%d", "%g", "\"%s\"", "%p", "", "", ", ", "", "", ", ", true };
    std::string out = tostring(t, data(), fmt);
    if (n < nfull)
        out += Strutil::sprintf(", ... [%d x %s]", nfull,
                                TypeDesc(TypeDesc::BASETYPE(type().basetype)));
#else
    std::string out;
    TypeDesc element = type().elementtype();
    int nfull        = int(type().numelements()) * nvalues();
    int n            = std::min(nfull, maxsize);
    if (element.basetype == TypeDesc::STRING) {
        // Just a single scalar string -- return it directly, not quoted
        if (n == 1 && !type().is_array())
            return get<const char*>();
        // Multiple strings or an array -- return a list of double-quoted
        // strings.
        for (int i = 0; i < n; ++i) {
            const char* s = ((const char**)data())[i];
            out += Strutil::sprintf("%s\"%s\"", (i ? ", " : ""),
                                    s ? Strutil::escape_chars(s)
                                      : std::string());
        }
    } else if (element.basetype == TypeDesc::FLOAT) {
        formatType<float>(*this, 0, n, "%g", out);
    } else if (element.basetype == TypeDesc::DOUBLE) {
        formatType<double>(*this, 0, n, "%g", out);
    } else if (element.basetype == TypeDesc::HALF) {
        formatType<half>(*this, 0, n, "%g", out);
    } else if (element.basetype == TypeDesc::INT) {
        if (element == TypeRational) {
            const int* val = (const int*)data();
            for (int i = 0; i < n; ++i, val += 2) {
                if (i)
                    out += ", ";
                out += Strutil::sprintf("%d/%d", val[0], val[1]);
            }
        } else {
            formatType<int>(*this, 0, n, "%d", out);
        }
    } else if (element.basetype == TypeDesc::UINT) {
        if (element.vecsemantics == TypeDesc::RATIONAL
            && element.aggregate == TypeDesc::VEC2) {
            const uint32_t* val = (const uint32_t*)data();
            for (int i = 0; i < n; ++i, val += 2) {
                if (i)
                    out += ", ";
                out += Strutil::sprintf("%d/%d", val[0], val[1]);
            }
        } else if (type() == TypeTimeCode) {
            // Replicating the logic in OpenEXR, but this prevents us from
            // needing to link to libIlmImf just to do this.
            unsigned int t = get<unsigned int>(0);
            int hours      = bcdToBinary(bitField(t, 24, 29));
            int minutes    = bcdToBinary(bitField(t, 16, 22));
            int seconds    = bcdToBinary(bitField(t, 8, 14));
            int frame      = bcdToBinary(bitField(t, 0, 5));
            out += Strutil::sprintf("%02d:%02d:%02d:%02d", hours, minutes,
                                    seconds, frame);
        } else {
            formatType<unsigned int>(*this, 0, n, "%u", out);
        }
    } else if (element.basetype == TypeDesc::UINT16) {
        formatType<unsigned short>(*this, 0, n, "%u", out);
    } else if (element.basetype == TypeDesc::INT16) {
        formatType<short>(*this, 0, n, "%d", out);
    } else if (element.basetype == TypeDesc::UINT64) {
        formatType<unsigned long long>(*this, 0, n, "%llu", out);
    } else if (element.basetype == TypeDesc::INT64) {
        formatType<long long>(*this, 0, n, "%lld", out);
    } else if (element.basetype == TypeDesc::UINT8) {
        formatType<unsigned char>(*this, 0, n, "%d", out);
    } else if (element.basetype == TypeDesc::INT8) {
        formatType<char>(*this, 0, n, "%d", out);
    } else if (element.basetype == TypeDesc::PTR) {
        out += "ptr ";
        formatType<void*>(*this, 0, n, "%p", out);
    } else {
        out += Strutil::sprintf("<unknown data type> (base %d, agg %d vec %d)",
                                type().basetype, type().aggregate,
                                type().vecsemantics);
    }
    if (n < nfull)
        out += Strutil::sprintf(", ... [%d x %s]", nfull,
                                TypeDesc(TypeDesc::BASETYPE(type().basetype)));
#endif
    return out;
}



std::string
ParamValue::get_string_indexed(int index) const
{
    std::string out;
    TypeDesc element = type().elementtype();
    int n            = int(type().numelements()) * nvalues();
    if (index < 0 || index >= n)
        return out;
    if (element.basetype == TypeDesc::STRING) {
        return get<const char*>(index);
    } else if (element.basetype == TypeDesc::FLOAT) {
        formatType<float>(*this, index, index + 1, "%g", out);
    } else if (element.basetype == TypeDesc::DOUBLE) {
        formatType<double>(*this, index, index + 1, "%g", out);
    } else if (element.basetype == TypeDesc::HALF) {
        formatType<half>(*this, index, index + 1, "%g", out);
    } else if (element.basetype == TypeDesc::INT) {
        if (element == TypeRational) {
            const int* val = (const int*)data() + 2 * index;
            out            = Strutil::sprintf("%d/%d", val[0], val[1]);
        } else {
            formatType<int>(*this, index, index + 1, "%d", out);
        }
    } else if (element.basetype == TypeDesc::UINT) {
        if (element.vecsemantics == TypeDesc::RATIONAL
            && element.aggregate == TypeDesc::VEC2) {
            const int* val = (const int*)data() + 2 * index;
            out            = Strutil::sprintf("%d/%d", val[0], val[1]);
        } else if (type() == TypeTimeCode) {
            // Replicating the logic in OpenEXR, but this prevents us from
            // needing to link to libIlmImf just to do this.
            unsigned int t = get<unsigned int>(0);
            int hours      = bcdToBinary(bitField(t, 24, 29));
            int minutes    = bcdToBinary(bitField(t, 16, 22));
            int seconds    = bcdToBinary(bitField(t, 8, 14));
            int frame      = bcdToBinary(bitField(t, 0, 5));
            out += Strutil::sprintf("%02d:%02d:%02d:%02d", hours, minutes,
                                    seconds, frame);
        } else {
            formatType<unsigned int>(*this, index, index + 1, "%u", out);
        }
    } else if (element.basetype == TypeDesc::UINT16) {
        formatType<unsigned short>(*this, index, index + 1, "%u", out);
    } else if (element.basetype == TypeDesc::INT16) {
        formatType<short>(*this, index, index + 1, "%d", out);
    } else if (element.basetype == TypeDesc::UINT64) {
        formatType<unsigned long long>(*this, index, index + 1, "%llu", out);
    } else if (element.basetype == TypeDesc::INT64) {
        formatType<long long>(*this, index, index + 1, "%lld", out);
    } else if (element.basetype == TypeDesc::UINT8) {
        formatType<unsigned char>(*this, index, index + 1, "%d", out);
    } else if (element.basetype == TypeDesc::INT8) {
        formatType<char>(*this, index, index + 1, "%d", out);
    } else if (element.basetype == TypeDesc::PTR) {
        out += "ptr ";
        formatType<void*>(*this, index, index + 1, "%p", out);
    } else {
        out += Strutil::sprintf("<unknown data type> (base %d, agg %d vec %d)",
                                type().basetype, type().aggregate,
                                type().vecsemantics);
    }
    return out;
}



ustring
ParamValue::get_ustring(int maxsize) const
{
    // Special case for retrieving a string already in ustring form,
    // super inexpensive.
    if (type() == TypeDesc::STRING)
        return get<ustring>();
    return ustring(get_string(maxsize));
}



ustring
ParamValue::get_ustring_indexed(int index) const
{
    // Special case for retrieving a string already in ustring form,
    // super inexpensive.
    if (type() == TypeDesc::STRING)
        return get<ustring>(index);
    return ustring(get_string_indexed(index));
}



void
ParamValue::clear_value() noexcept
{
    if (m_copy && m_nonlocal && m_data.ptr)
        free((void*)m_data.ptr);
    m_data.ptr = nullptr;
    m_copy     = false;
    m_nonlocal = false;
}



ParamValueList::const_iterator
ParamValueList::find(ustring name, TypeDesc type, bool casesensitive) const
{
    if (casesensitive) {
        for (const_iterator i = cbegin(), e = cend(); i != e; ++i) {
            if (i->name() == name
                && (type == TypeDesc::UNKNOWN || type == i->type()))
                return i;
        }
    } else {
        for (const_iterator i = cbegin(), e = cend(); i != e; ++i) {
            if (Strutil::iequals(i->name(), name)
                && (type == TypeDesc::UNKNOWN || type == i->type()))
                return i;
        }
    }
    return cend();
}



ParamValueList::const_iterator
ParamValueList::find(string_view name, TypeDesc type, bool casesensitive) const
{
    if (casesensitive) {
        return find(ustring(name), type, casesensitive);
    } else {
        for (const_iterator i = cbegin(), e = cend(); i != e; ++i) {
            if (Strutil::iequals(i->name(), name)
                && (type == TypeDesc::UNKNOWN || type == i->type()))
                return i;
        }
    }
    return cend();
}



ParamValueList::iterator
ParamValueList::find(ustring name, TypeDesc type, bool casesensitive)
{
    if (casesensitive) {
        for (iterator i = begin(), e = end(); i != e; ++i) {
            if (i->name() == name
                && (type == TypeDesc::UNKNOWN || type == i->type()))
                return i;
        }
    } else {
        for (iterator i = begin(), e = end(); i != e; ++i) {
            if (Strutil::iequals(i->name(), name)
                && (type == TypeDesc::UNKNOWN || type == i->type()))
                return i;
        }
    }
    return end();
}



ParamValueList::iterator
ParamValueList::find(string_view name, TypeDesc type, bool casesensitive)
{
    if (casesensitive) {
        return find(ustring(name), type, casesensitive);
    } else {
        for (iterator i = begin(), e = end(); i != e; ++i) {
            if (Strutil::iequals(i->name(), name)
                && (type == TypeDesc::UNKNOWN || type == i->type()))
                return i;
        }
    }
    return end();
}



int
ParamValueList::get_int(string_view name, int defaultval, bool casesensitive,
                        bool convert) const
{
    auto p = find(name, convert ? TypeDesc::UNKNOWN : TypeDesc::INT,
                  casesensitive);
    return (p == cend()) ? defaultval : p->get_int(defaultval);
}



float
ParamValueList::get_float(string_view name, float defaultval,
                          bool casesensitive, bool convert) const
{
    auto p = find(name, convert ? TypeDesc::UNKNOWN : TypeDesc::FLOAT,
                  casesensitive);
    return (p == cend()) ? defaultval : p->get_float(defaultval);
}



string_view
ParamValueList::get_string(string_view name, string_view defaultval,
                           bool casesensitive, bool convert) const
{
    auto p = find(name, convert ? TypeDesc::UNKNOWN : TypeDesc::STRING,
                  casesensitive);
    return (p == cend()) ? defaultval : string_view(p->get_ustring());
}



ustring
ParamValueList::get_ustring(string_view name, string_view defaultval,
                            bool casesensitive, bool convert) const
{
    auto p = find(name, convert ? TypeDesc::UNKNOWN : TypeDesc::STRING,
                  casesensitive);
    return (p == cend()) ? ustring(defaultval) : p->get_ustring();
}



void
ParamValueList::remove(string_view name, TypeDesc type, bool casesensitive)
{
    auto p = find(name, type, casesensitive);
    if (p != end())
        erase(p);
}



bool
ParamValueList::contains(string_view name, TypeDesc type,
                         bool casesensitive) const
{
    auto p = find(name, type, casesensitive);
    return (p != end());
}



void
ParamValueList::add_or_replace(const ParamValue& pv, bool casesensitive)
{
    iterator p = find(pv.name(), TypeUnknown, casesensitive);
    if (p != end())
        *p = pv;
    else
        emplace_back(pv);
}


void
ParamValueList::add_or_replace(ParamValue&& pv, bool casesensitive)
{
    iterator p = find(pv.name(), TypeUnknown, casesensitive);
    if (p != end())
        *p = pv;
    else
        emplace_back(pv);
}



bool
ParamValueList::getattribute(string_view name, TypeDesc type, void* value,
                             bool casesensitive) const
{
    auto p = find(name, TypeUnknown, casesensitive);
    if (p != cend()) {
        return convert_type(p->type(), p->data(), type, value);
    } else {
        return false;
    }
}



bool
ParamValueList::getattribute(string_view name, std::string& value,
                             bool casesensitive) const
{
    auto p = find(name, TypeUnknown, casesensitive);
    if (p != cend()) {
        ustring s;
        bool ok = convert_type(p->type(), p->data(), TypeString, &s);
        if (ok)
            value = s.string();
        return ok;
    } else {
        return false;
    }
}



bool
ParamValueList::getattribute_indexed(string_view name, int index, TypeDesc type,
                                     void* value, bool casesensitive) const
{
    auto p = find(name, TypeUnknown, casesensitive);
    if (p != cend()) {
        if (index >= int(p->type().basevalues()))
            return false;
        TypeDesc basetype = p->type().scalartype();
        return convert_type(basetype,
                            (const char*)p->data() + index * basetype.size(),
                            type, value);
    } else {
        return false;
    }
}



bool
ParamValueList::getattribute_indexed(string_view name, int index,
                                     std::string& value,
                                     bool casesensitive) const
{
    auto p = find(name, TypeUnknown, casesensitive);
    if (p != cend()) {
        if (index >= int(p->type().basevalues()))
            return false;
        TypeDesc basetype = p->type().scalartype();
        ustring s;
        bool ok = convert_type(basetype,
                               (const char*)p->data() + index * basetype.size(),
                               TypeString, &s);
        if (ok)
            value = s.string();
        return ok;
    } else {
        return false;
    }
}



void
ParamValueList::sort(bool casesensitive)
{
    if (casesensitive)
        std::sort(begin(), end(),
                  [&](const ParamValue& a, const ParamValue& b) -> bool {
                      bool aprefix = a.name().find(':') != ustring::npos;
                      bool bprefix = b.name().find(':') != ustring::npos;
                      return (aprefix != bprefix) ? bprefix
                                                  : a.name() < b.name();
                  });
    else
        std::sort(begin(), end(),
                  [&](const ParamValue& a, const ParamValue& b) -> bool {
                      bool aprefix = a.name().find(':') != ustring::npos;
                      bool bprefix = b.name().find(':') != ustring::npos;
                      return (aprefix != bprefix)
                                 ? bprefix
                                 : Strutil::iless(a.name(), b.name());
                  });
}



void
ParamValueList::merge(const ParamValueList& other, bool override)
{
    for (const auto& attr : other) {
        if (override || !contains(attr.name()))
            add_or_replace(attr);
    }
}

OIIO_NAMESPACE_END
