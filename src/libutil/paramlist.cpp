// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/half.h>
#include <OpenImageIO/paramlist.h>
#include <OpenImageIO/ustring.h>


OIIO_NAMESPACE_BEGIN


void
ParamValue::init_noclear(ustring _name, TypeDesc _type, int _nvalues,
                         const void* _value, Copy _copy,
                         FromUstring _from_ustring) noexcept
{
    init_noclear(_name, _type, _nvalues, INTERP_CONSTANT, _value, _copy,
                 _from_ustring);
}



void
ParamValue::init_noclear(ustring _name, TypeDesc _type, int _nvalues,
                         Interp _interp, const void* _value, Copy _copy,
                         FromUstring _from_ustring) noexcept
{
    m_name      = _name;
    m_type      = _type;
    m_nvalues   = _nvalues;
    m_interp    = _interp;
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
            void* ptr = malloc(size);
            if (_value)
                memcpy(ptr, _value, size);  //NOSONAR
            else
                memset(ptr, 0, size);
            m_data.ptr = ptr;
            m_copy     = true;
            m_nonlocal = true;
        }
        if (m_type.basetype == TypeDesc::STRING && !_from_ustring) {
            // Convert non-ustrings to ustrings
            for (ustring& u : as_span<ustring>())
                u = ustring(u.c_str());
        }
    } else {
        // Big enough to warrant a malloc, but the caller said don't
        // make a copy
        m_data.ptr = _value;
        m_copy     = false;
        m_nonlocal = true;
    }
}



const ParamValue&
ParamValue::operator=(const ParamValue& p) noexcept
{
    if (this != &p) {
        clear_value();
        init_noclear(p.name(), p.type(), p.nvalues(), p.interp(), p.data(),
                     Copy(p.m_copy), FromUstring(true));
    }
    return *this;
}



const ParamValue&
ParamValue::operator=(ParamValue&& p) noexcept
{
    if (this != &p) {
        clear_value();
        init_noclear(p.name(), p.type(), p.nvalues(), p.interp(), p.data(),
                     Copy(false), FromUstring(true));
        m_copy       = p.m_copy;
        m_nonlocal   = p.m_nonlocal;
        p.m_data.ptr = nullptr;  // make sure the old one won't free
    }
    return *this;
}



namespace Strutil {
template<>
inline short
from_string<short>(string_view s)
{
    return static_cast<short>(Strutil::stoi(s));
}


template<>
inline unsigned short
from_string<unsigned short>(string_view s)
{
    return static_cast<unsigned short>(Strutil::stoi(s));
}
}  // namespace Strutil



// helper to parse a list from a string
template<class T>
static void
parse_elements(string_view value, ParamValue& p)
{
    auto data = p.as_span<T>();
    value.remove_prefix(value.find_first_not_of(" \t"));
    for (auto&& d : data) {
        // Make a temporary copy so we for sure have a 0-terminated string.
        std::string temp = value;
        // Grab the first value from it
        d = Strutil::from_string<T>(temp);
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
        parse_elements<int>(value, *this);
    } else if (type.basetype == TypeDesc::UINT) {
        parse_elements<unsigned int>(value, *this);
    } else if (type.basetype == TypeDesc::FLOAT) {
        parse_elements<float>(value, *this);
    } else if (type.basetype == TypeDesc::DOUBLE) {
        parse_elements<double>(value, *this);
    } else if (type.basetype == TypeDesc::INT64) {
        parse_elements<int64_t>(value, *this);
    } else if (type.basetype == TypeDesc::UINT64) {
        parse_elements<uint64_t>(value, *this);
    } else if (type.basetype == TypeDesc::INT16) {
        parse_elements<short>(value, *this);
    } else if (type.basetype == TypeDesc::UINT16) {
        parse_elements<unsigned short>(value, *this);
    } else if (type == TypeDesc::STRING) {
        ustring s(value);
        init(name, type, 1, &s);
    } else if (type == TypeDesc::USTRINGHASH) {
        ustringhash s(value);
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
    int val = defaultval;
    convert_type(type().elementtype(),
                 (const char*)data() + index * type().basesize(), TypeInt,
                 &val);
    return val;
}



float
ParamValue::get_float(float defaultval) const
{
    return get_float_indexed(0, defaultval);
}



float
ParamValue::get_float_indexed(int index, float defaultval) const
{
    float val = defaultval;
    convert_type(type().elementtype(),
                 (const char*)data() + index * type().basesize(), TypeFloat,
                 &val);
    return val;
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
            out += Strutil::fmt::format(formatString, f[0]);
        }
    }
}


template<>
void
formatType<half>(const ParamValue& p, int beginindex, int endindex,
                 const char* formatString, std::string& out)
{
    TypeDesc element = p.type().elementtype();
    const half* f    = (const half*)p.data() + beginindex * element.aggregate;
    for (int i = beginindex; i < endindex; ++i) {
        if (i > beginindex)
            out += ", ";
        for (int c = 0; c < (int)element.aggregate; ++c, ++f) {
            if (c)
                out += " ";
            out += Strutil::fmt::format(formatString, float(f[0]));
        }
    }
}
}  // namespace



std::string
ParamValue::get_string(int maxsize) const
{
    int nfull  = int(type().basevalues()) * nvalues();
    int n      = maxsize ? std::min(nfull, maxsize) : nfull;
    TypeDesc t = type();
    if (nvalues() > 1 || n < nfull) {
        t.aggregate = TypeDesc::SCALAR;
        t.arraylen  = n;
    }
    static const tostring_formatting fmt = { "%d", "%g", "\"%s\"", "%p",
                                             "",   "",   ", ",     "",
                                             "",   ", ", true,     "%u" };
    std::string out                      = tostring(t, data(), fmt);
    if (n < nfull)
        out += Strutil::fmt::format(", ... [{} x {}]", nfull,
                                    type().scalartype());
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
        return get<ustring>(index).string();
    } else if (element.basetype == TypeDesc::FLOAT) {
        formatType<float>(*this, index, index + 1, "{}", out);
    } else if (element.basetype == TypeDesc::DOUBLE) {
        formatType<double>(*this, index, index + 1, "{}", out);
    } else if (element.basetype == TypeDesc::HALF) {
        formatType<half>(*this, index, index + 1, "{}", out);
    } else if (element.basetype == TypeDesc::INT) {
        if (element == TypeRational) {
            const int* val = (const int*)data() + 2 * index;
            out            = Strutil::fmt::format("{}/{}", val[0], val[1]);
        } else {
            formatType<int>(*this, index, index + 1, "{}", out);
        }
    } else if (element.basetype == TypeDesc::UINT) {
        if (element.vecsemantics == TypeDesc::RATIONAL
            && element.aggregate == TypeDesc::VEC2) {
            const int* val = (const int*)data() + 2 * index;
            out            = Strutil::fmt::format("{}/{}", val[0], val[1]);
        } else if (type() == TypeTimeCode) {
            out += tostring(TypeTimeCode, data());
        } else {
            formatType<unsigned int>(*this, index, index + 1, "{}", out);
        }
    } else if (element.basetype == TypeDesc::UINT16) {
        formatType<unsigned short>(*this, index, index + 1, "{}", out);
    } else if (element.basetype == TypeDesc::INT16) {
        formatType<short>(*this, index, index + 1, "{}", out);
    } else if (element.basetype == TypeDesc::UINT64) {
        formatType<unsigned long long>(*this, index, index + 1, "{}", out);
    } else if (element.basetype == TypeDesc::INT64) {
        formatType<long long>(*this, index, index + 1, "{}", out);
    } else if (element.basetype == TypeDesc::UINT8) {
        formatType<unsigned char>(*this, index, index + 1, "{}", out);
    } else if (element.basetype == TypeDesc::INT8) {
        formatType<char>(*this, index, index + 1, "{}", out);
    } else if (element.basetype == TypeDesc::PTR) {
        out += "ptr ";
        formatType<void*>(*this, index, index + 1, "{:p}", out);
    } else if (element.basetype == TypeDesc::USTRINGHASH) {
        return get<ustringhash>(index).string();
    } else {
        out += Strutil::fmt::format(
            "<unknown data type> (base {:d}, agg {:d} vec {:d})",
            type().basetype, type().aggregate, type().vecsemantics);
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
    if (type() == TypeDesc::USTRINGHASH)
        return ustring(get<ustringhash>());
    return ustring(get_string(maxsize));
}



ustring
ParamValue::get_ustring_indexed(int index) const
{
    // Special case for retrieving a string already in ustring form,
    // super inexpensive.
    if (type() == TypeDesc::STRING)
        return get<ustring>(index);
    if (type() == TypeDesc::USTRINGHASH)
        return ustring(get<ustringhash>());
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



template<>
size_t
pvt::heapsize<ParamValue>(const ParamValue& pv)
{
    return (pv.m_nonlocal && pv.m_copy)
               ? pv.m_nvalues * static_cast<int>(pv.m_type.size())
               : 0;
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



ParamValueSpan::const_iterator
ParamValueSpan::find(ustring name, TypeDesc type, bool casesensitive) const
{
    if (casesensitive) {
        for (const_iterator i = cbegin(), e = cend(); i != e; ++i) {
            if (i->name() == name && (type == TypeUnknown || type == i->type()))
                return i;
        }
    } else {
        for (const_iterator i = cbegin(), e = cend(); i != e; ++i) {
            if (Strutil::iequals(i->name(), name)
                && (type == TypeUnknown || type == i->type()))
                return i;
        }
    }
    return cend();
}



ParamValueSpan::const_iterator
ParamValueSpan::find(string_view name, TypeDesc type, bool casesensitive) const
{
    if (casesensitive) {
        return find(ustring(name), type, casesensitive);
    } else {
        for (const_iterator i = cbegin(), e = cend(); i != e; ++i) {
            if (Strutil::iequals(i->name(), name)
                && (type == TypeUnknown || type == i->type()))
                return i;
        }
    }
    return cend();
}



int
ParamValueSpan::get_int(ustring name, int defaultval, bool casesensitive,
                        bool convert) const
{
    auto p = find(name, convert ? TypeUnknown : TypeInt, casesensitive);
    return (p == cend()) ? defaultval : p->get_int(defaultval);
}



int
ParamValueSpan::get_int(string_view name, int defaultval, bool casesensitive,
                        bool convert) const
{
    auto p = find(name, convert ? TypeUnknown : TypeInt, casesensitive);
    return (p == cend()) ? defaultval : p->get_int(defaultval);
}



float
ParamValueSpan::get_float(string_view name, float defaultval,
                          bool casesensitive, bool convert) const
{
    auto p = find(name, convert ? TypeUnknown : TypeFloat, casesensitive);
    return (p == cend()) ? defaultval : p->get_float(defaultval);
}



float
ParamValueSpan::get_float(ustring name, float defaultval, bool casesensitive,
                          bool convert) const
{
    auto p = find(name, convert ? TypeUnknown : TypeFloat, casesensitive);
    return (p == cend()) ? defaultval : p->get_float(defaultval);
}



string_view
ParamValueSpan::get_string(string_view name, string_view defaultval,
                           bool casesensitive, bool convert) const
{
    auto p = find(name, convert ? TypeUnknown : TypeString, casesensitive);
    return (p == cend()) ? defaultval : string_view(p->get_ustring());
}



string_view
ParamValueSpan::get_string(ustring name, string_view defaultval,
                           bool casesensitive, bool convert) const
{
    auto p = find(name, convert ? TypeUnknown : TypeString, casesensitive);
    return (p == cend()) ? defaultval : string_view(p->get_ustring());
}



ustring
ParamValueSpan::get_ustring(string_view name, string_view defaultval,
                            bool casesensitive, bool convert) const
{
    auto p = find(name, convert ? TypeUnknown : TypeString, casesensitive);
    return (p == cend()) ? ustring(defaultval) : p->get_ustring();
}



ustring
ParamValueSpan::get_ustring(ustring name, string_view defaultval,
                            bool casesensitive, bool convert) const
{
    auto p = find(name, convert ? TypeUnknown : TypeString, casesensitive);
    return (p == cend()) ? ustring(defaultval) : p->get_ustring();
}



bool
ParamValueSpan::get_bool(ustring name, bool defaultval,
                         bool casesensitive) const
{
    auto p = find(name, TypeUnknown, casesensitive);
    if (p == cend())
        return defaultval;
    if (p->type().basetype == TypeDesc::INT)
        return p->get_int() ? 1 : 0;
    return Strutil::eval_as_bool(p->get_string());
}



bool
ParamValueSpan::get_bool(string_view name, bool defaultval,
                         bool casesensitive) const
{
    auto p = find(name, TypeUnknown, casesensitive);
    if (p == cend())
        return defaultval;
    if (p->type().basetype == TypeDesc::INT)
        return p->get_int() ? 1 : 0;
    return Strutil::eval_as_bool(p->get_string());
}



bool
ParamValueSpan::getattribute(string_view name, TypeDesc type, void* value,
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
ParamValueSpan::getattribute(string_view name, std::string& value,
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
ParamValueSpan::getattribute_indexed(string_view name, int index, TypeDesc type,
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
ParamValueSpan::getattribute_indexed(string_view name, int index,
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



OIIO_NAMESPACE_END
