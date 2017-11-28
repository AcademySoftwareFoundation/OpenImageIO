/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/

#include <cstdio>
#include <cstdlib>

#include <OpenEXR/half.h>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/ustring.h>
#include <OpenImageIO/paramlist.h>


OIIO_NAMESPACE_BEGIN


void
ParamValue::init_noclear (ustring _name, TypeDesc _type,
                          int _nvalues, const void *_value, bool _copy)
{
    init_noclear (_name, _type, _nvalues, INTERP_CONSTANT, _value, _copy);
}



void
ParamValue::init_noclear (ustring _name, TypeDesc _type, int _nvalues,
                          Interp _interp, const void *_value, bool _copy)
{
    m_name = _name;
    m_type = _type;
    m_nvalues = _nvalues;
    m_interp = _interp;
    size_t n = (size_t) (m_nvalues * m_type.numelements());
    size_t size = (size_t) (m_nvalues * m_type.size());
    bool small = (size <= sizeof(m_data));

    if (_copy || small) {
        if (small) {
            if (_value)
                memcpy (&m_data, _value, size);
            else
                memset (&m_data, 0, sizeof(m_data));
            m_copy = false;
            m_nonlocal = false;
        } else {
            m_data.ptr = malloc (size);
            if (_value)
                memcpy ((char *)m_data.ptr, _value, size);
            else
                memset ((char *)m_data.ptr, 0, size);
            m_copy = true;
            m_nonlocal = true;
        }
        if (m_type.basetype == TypeDesc::STRING) {
            ustring *u = (ustring *) data();
            for (size_t i = 0;  i < n;  ++i)
                u[i] = ustring(u[i].c_str());
        }
    } else {
        // Big enough to warrant a malloc, but the caller said don't
        // make a copy
        m_data.ptr = _value;
        m_copy = false;
        m_nonlocal = true;
    }
}



// helper to parse a list from a string
template <class T>
static void
parse_elements (string_view name, TypeDesc type, const char *type_code,
                string_view value, ParamValue &p)
{
    int num_items = type.numelements() * type.aggregate;
    T *data = (T*) p.data();
    // Erase any leading whitespace
    value.remove_prefix (value.find_first_not_of (" \t"));
    for (int i = 0;  i < num_items;  ++i) {
        // Make a temporary copy so we for sure have a 0-terminated string.
        std::string temp = value;
        // Grab the first value from it
        sscanf (temp.c_str(), type_code, &data[i]);
        // Skip the value (eat until we find a delimiter -- space, comma, tab)
        value.remove_prefix (value.find_first_of (" ,\t"));
        // Skip the delimiter
        value.remove_prefix (value.find_first_not_of (" ,\t"));
        if (value.empty())
            break;   // done if nothing left to parse
    }
}



// Construct from parsed string
ParamValue::ParamValue (string_view name, TypeDesc type, string_view value)
    : ParamValue (name, type, 1, nullptr)
{
    if (type.basetype == TypeDesc::INT) {
        parse_elements<int> (name, type, "%d", value, *this);
    } else if (type.basetype == TypeDesc::UINT) {
        parse_elements<unsigned int> (name, type, "%u", value, *this);
    } else if (type.basetype == TypeDesc::FLOAT) {
        parse_elements<float> (name, type, "%f", value, *this);
    } else if (type.basetype == TypeDesc::DOUBLE) {
        parse_elements<double> (name, type, "%lf", value, *this);
    } else if (type.basetype == TypeDesc::INT64) {
        parse_elements<long long> (name, type, "%lld", value, *this);
    } else if (type.basetype == TypeDesc::UINT64) {
        parse_elements<unsigned long long> (name, type, "%llu", value, *this);
    } else if (type.basetype == TypeDesc::INT16) {
        parse_elements<short> (name, type, "%hd", value, *this);
    } else if (type.basetype == TypeDesc::UINT16) {
        parse_elements<unsigned short> (name, type, "%hu", value, *this);
    } else if (type == TypeDesc::STRING) {
        ustring s (value);
        init (name, type, 1, &s);
    }
}



int
ParamValue::get_int (int defaultval) const
{
    return get_int_indexed (0, defaultval);
}



int
ParamValue::get_int_indexed (int index, int defaultval) const
{
    int base = type().basetype;
    if (base == TypeDesc::INT)
        return get<int>(index);
    if (base == TypeDesc::UINT)
        return (int) get<unsigned int>(index);
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
        int val = defaultval;
        if (Strutil::parse_int(str, val) && str.empty())
            return val;
    }
    return defaultval;   // Some nonstandard type, fail
}



float
ParamValue::get_float (float defaultval) const
{
    return get_float_indexed (0, defaultval);
}



float
ParamValue::get_float_indexed (int index, float defaultval) const
{
    int base = type().basetype;
    if (base == TypeDesc::FLOAT)
        return get<float>(index);
    if (base == TypeDesc::HALF)
        return get<half>(index);
    if (base == TypeDesc::DOUBLE)
        return get<double>(index);
    if (base == TypeDesc::INT) {
        if (type().aggregate == TypeDesc::VEC2 &&
            type().vecsemantics == TypeDesc::RATIONAL) {
            int num = get<int>(2*index+0);
            int den = get<int>(2*index+1);
            return den ? float(num)/float(den) : 0.0f;
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
        float val = defaultval;
        if (Strutil::parse_float(str, val) && str.empty())
            return val;
    }

    return defaultval;
}



namespace {  // make an anon namespace

template < typename T >
void formatType (const ParamValue& p, const int n, const char* formatString,
                 std::string& out)
{
    TypeDesc element = p.type().elementtype();
    const T *f = (const T *)p.data();
    for (int i = 0;  i < n;  ++i) {
        if (i)
            out += ", ";
        for (int c = 0;  c < (int)element.aggregate;  ++c, ++f) {
            if (c)
                out += " ";
            out += Strutil::format (formatString, f[0]);
        }
    }
}

} // end anon namespace



std::string
ParamValue::get_string (int maxsize) const
{
    std::string out;
    TypeDesc element = type().elementtype();
    int nfull = int(type().numelements()) * nvalues();
    int n = std::min (nfull, maxsize);
    if (element.basetype == TypeDesc::STRING) {
        // Just a single scalar string -- return it directly, not quoted
        if (n == 1 && ! type().is_array())
            return get<const char *>();
        // Multiple strings or an array -- return a list of double-quoted
        // strings.
        for (int i = 0;  i < n;  ++i) {
            const char *s = ((const char **)data())[i];
            out += Strutil::format ("%s\"%s\"", (i ? ", " : ""),
                                    s ? Strutil::escape_chars(s) : std::string());
        }
    } else if (element.basetype == TypeDesc::FLOAT) {
        formatType< float >(*this, n, "%g", out);
    } else if (element.basetype == TypeDesc::DOUBLE) {
        formatType< double >(*this, n, "%g", out);
    } else if (element.basetype == TypeDesc::HALF) {
        formatType< half >(*this, n, "%g", out);
    } else if (element.basetype == TypeDesc::INT) {
        if (element.vecsemantics == TypeDesc::RATIONAL && element.aggregate == TypeDesc::VEC2) {
            const int *val = (const int *)data();
            for (int i = 0;  i < n;  ++i, val += 2) {
                if (i) out += ", ";
                out += Strutil::format ("%d/%d", val[0], val[1]);
            }
        } else {
            formatType< int >(*this, n, "%d", out);
        }
    } else if (element.basetype == TypeDesc::UINT) {
        if (element.vecsemantics == TypeDesc::RATIONAL && element.aggregate == TypeDesc::VEC2) {
            const int *val = (const int *)data();
            for (int i = 0;  i < n;  ++i, val += 2) {
                if (i) out += ", ";
                out += Strutil::format ("%d/%d", val[0], val[1]);
            }
        } else {
            formatType< unsigned int >(*this, n, "%u", out);
        }
    } else if (element.basetype == TypeDesc::UINT16) {
        formatType< unsigned short >(*this, n, "%u", out);
    } else if (element.basetype == TypeDesc::INT16) {
        formatType< short >(*this, n, "%d", out);
    } else if (element.basetype == TypeDesc::UINT64) {
        formatType< unsigned long long >(*this, n, "%llu", out);
    } else if (element.basetype == TypeDesc::INT64) {
        formatType< long long >(*this, n, "%lld", out);
    } else if (element.basetype == TypeDesc::UINT8) {
        formatType< unsigned char >(*this, n, "%d", out);
    } else if (element.basetype == TypeDesc::INT8) {
        formatType< char >(*this, n, "%d", out);
    } else {
        out += Strutil::format ("<unknown data type> (base %d, agg %d vec %d)",
                type().basetype, type().aggregate,
                type().vecsemantics);
    }
    if (n < nfull)
        out += Strutil::format (", ... [%d x %s]", nfull,
                                TypeDesc(TypeDesc::BASETYPE(element.basetype)));
    return out;
}



ustring
ParamValue::get_ustring (int maxsize) const
{
    // Special case for retrieving a string already in ustring form,
    // super inexpensive.
    if (type() == TypeDesc::STRING)
        return get<ustring>();
    return ustring (get_string(maxsize));
}



void
ParamValue::clear_value ()
{
    if (m_copy && m_nonlocal && m_data.ptr)
        free ((void *)m_data.ptr);
    m_data.ptr = nullptr;
    m_copy = false;
    m_nonlocal = false;
}



ParamValueList::const_iterator
ParamValueList::find (ustring name, TypeDesc type, bool casesensitive) const
{
    if (casesensitive) {
        for (const_iterator i = cbegin(), e = cend(); i != e; ++i) {
            if (i->name() == name &&
                  (type == TypeDesc::UNKNOWN || type == i->type()))
                return i;
        }
    } else {
        for (const_iterator i = cbegin(), e = cend(); i != e; ++i) {
            if (Strutil::iequals (i->name(), name) &&
                  (type == TypeDesc::UNKNOWN || type == i->type()))
                return i;
        }
    }
    return cend();
}



ParamValueList::const_iterator
ParamValueList::find (string_view name, TypeDesc type, bool casesensitive) const
{
    if (casesensitive) {
        return find (ustring(name), type, casesensitive);
    } else {
        for (const_iterator i = cbegin(), e = cend(); i != e; ++i) {
            if (Strutil::iequals (i->name(), name) &&
                  (type == TypeDesc::UNKNOWN || type == i->type()))
                return i;
        }
    }
    return cend();
}



ParamValueList::iterator
ParamValueList::find (ustring name, TypeDesc type, bool casesensitive)
{
    if (casesensitive) {
        for (iterator i = begin(), e = end(); i != e; ++i) {
            if (i->name() == name &&
                  (type == TypeDesc::UNKNOWN || type == i->type()))
                return i;
        }
    } else {
        for (iterator i = begin(), e = end(); i != e; ++i) {
            if (Strutil::iequals (i->name(), name) &&
                  (type == TypeDesc::UNKNOWN || type == i->type()))
                return i;
        }
    }
    return end();
}



ParamValueList::iterator
ParamValueList::find (string_view name, TypeDesc type, bool casesensitive)
{
    if (casesensitive) {
        return find (ustring(name), type, casesensitive);
    } else {
        for (iterator i = begin(), e = end(); i != e; ++i) {
            if (Strutil::iequals (i->name(), name) &&
                  (type == TypeDesc::UNKNOWN || type == i->type()))
                return i;
        }
    }
    return end();
}



int
ParamValueList::get_int (string_view name, int defaultval,
                         bool casesensitive, bool convert) const
{
    auto p = find (name, convert ? TypeDesc::UNKNOWN : TypeDesc::INT,
                   casesensitive);
    return (p == cend()) ? defaultval : p->get_int (defaultval);
}



float
ParamValueList::get_float (string_view name, float defaultval,
                           bool casesensitive, bool convert) const
{
    auto p = find (name, convert ? TypeDesc::UNKNOWN : TypeDesc::FLOAT,
                   casesensitive);
    return (p == cend()) ? defaultval : p->get_float (defaultval);
}



string_view
ParamValueList::get_string (string_view name, string_view defaultval,
                            bool casesensitive, bool convert) const
{
    auto p = find (name, convert ? TypeDesc::UNKNOWN : TypeDesc::STRING,
                   casesensitive);
    return (p == cend()) ? defaultval : string_view(p->get_ustring());
}



ustring
ParamValueList::get_ustring (string_view name, string_view defaultval,
                            bool casesensitive, bool convert) const
{
    auto p = find (name, convert ? TypeDesc::UNKNOWN : TypeDesc::STRING,
                   casesensitive);
    return (p == cend()) ? ustring(defaultval) : p->get_ustring();
}



void
ParamValueList::remove (string_view name, TypeDesc type, bool casesensitive)
{
    auto p = find (name, type, casesensitive);
    if (p != end())
        erase (p);
}



bool
ParamValueList::contains (string_view name, TypeDesc type, bool casesensitive)
{
    auto p = find (name, type, casesensitive);
    return (p != end());
}



void
ParamValueList::add_or_replace (const ParamValue& pv, bool casesensitive)
{
    iterator p = find (pv.name(), pv.type(), casesensitive);
    if (p != end())
        *p = pv;
    else
        emplace_back (pv);
}


void
ParamValueList::add_or_replace (ParamValue&& pv, bool casesensitive)
{
    iterator p = find (pv.name(), pv.type(), casesensitive);
    if (p != end())
        *p = pv;
    else
        emplace_back (pv);
}


OIIO_NAMESPACE_END
