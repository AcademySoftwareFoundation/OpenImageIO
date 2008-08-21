/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz.
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// (This is the MIT open source license.)
/////////////////////////////////////////////////////////////////////////////




#ifndef PARAMLIST_H
#define PARAMLIST_H

#include <vector>

#include "export.h"
#include "paramtype.h"
#include "ustring.h"


// FIXME: We should clearly put this in a namespace.  But maybe not "Gelato".
// namespace Gelato {

/// ParamValue holds a parameter and a pointer to its value(s)
///
/// Nomenclature: if you have an array of 4 colors for each of 15 points...
///   There are 15 VALUES
///   Each value has an array of 4 ELEMENTS, ecah of which is a color
///   A color has 3 COMPONENTS (R, G, B)
///   
class DLLPUBLIC ParamValue {
public:
    ParamValue () : m_type(PT_UNKNOWN), m_nvalues(0), m_nonlocal(false) { }
    ParamValue (const ustring &_name, ParamType _type,
                int _nvalues, const void *_value, bool _copy=true) {
        init_noclear (_name, _type, _nvalues, _value, _copy);
    }
    ParamValue (const std::string &_name, ParamType _type,
                int _nvalues, const void *_value, bool _copy=true) {
        init_noclear (ustring(_name.c_str()), _type, _nvalues, _value, _copy);
    }
    ParamValue (const char *_name, ParamType _type,
                int _nvalues, const void *_value, bool _copy=true) {
        init_noclear (ustring(_name), _type, _nvalues, _value, _copy);
    }
    ParamValue (const ParamValue &p, bool _copy=true) {
        init_noclear (p.name(), p.type(), p.nvalues(), p.data(), _copy);
    }
    ~ParamValue () { clear_value(); }
    void init (ustring _name, ParamType _type,
               int _nvalues, const void *_value, bool _copy=true) {
        clear_value ();
        init_noclear (_name, _type, _nvalues, _value, _copy);
    }
    void init (std::string _name, ParamType _type,
               int _nvalues, const void *_value, bool _copy=true) {
        init (ustring(_name), _type, _nvalues, _value, _copy);
    }
    const ParamValue& operator= (const ParamValue &p) {
        init (p.name(), p.type(), p.nvalues(), p.data(), p.m_copy);
        return *this;
    }

    const ustring &name () const { return m_name; }
    ParamType type () const { return m_type; }
    int nvalues () const { return m_nvalues; }
    const void *data () const { return m_nonlocal ? m_data.ptr : &m_data; }
    int datasize () const { return m_nvalues * m_type.datasize(); }

    friend void swap (ParamValue &a, ParamValue &b) {
        std::swap (a.m_name,     b.m_name);
        std::swap (a.m_type,     b.m_type);
        std::swap (a.m_nvalues,  b.m_nvalues);
        std::swap (a.m_data.ptr, b.m_data.ptr);
        std::swap (a.m_copy,     b.m_copy);
        std::swap (a.m_nonlocal, b.m_nonlocal);
    }

private: 
    ustring m_name;           ///< data name
    ParamType m_type;         ///< data type, which may itself be an array
    int m_nvalues;            ///< number of values of the given type
    union {
        ptrdiff_t localval;
        const void *ptr;
    } m_data;             ///< Our data, either a pointer or small local value
    bool m_copy, m_nonlocal;

    void init_noclear (ustring _name, ParamType _type,
                       int _nvalues, const void *_value, bool _copy=true);
    void clear_value();
};



class DLLPUBLIC ParamValueList {
    typedef std::vector<ParamValue> Rep;
public:
    ParamValueList () { }

    typedef Rep::iterator        iterator;
    typedef Rep::const_iterator  const_iterator;
    typedef ParamValue           value_type;
    typedef value_type &         reference;
    typedef const value_type &   const_reference;
    typedef value_type *         pointer;
    typedef const value_type *   const_pointer;

    iterator begin () { return m_vals.begin(); }
    iterator end () { return m_vals.end(); }
    const_iterator begin () const { return m_vals.begin(); }
    const_iterator end () const { return m_vals.end(); }

    reference front () { return m_vals.front(); }
    reference back () { return m_vals.back(); }
    const_reference front () const { return m_vals.front(); }
    const_reference back () const { return m_vals.back(); }

    reference operator[] (int i) { return m_vals[i]; }
    const_reference operator[] (int i) const { return m_vals[i]; }
    reference operator[] (size_t i) { return m_vals[i]; }
    const_reference operator[] (size_t i) const { return m_vals[i]; }

    void resize (size_t newsize) { m_vals.resize (newsize); }
    size_t size () const { return m_vals.size(); }

    void clear () { m_vals.clear(); }

    // Even more radical than clear, free ALL memory associated with the
    // list itself.
    void free () { Rep tmp; std::swap (m_vals, tmp); }

private:
    Rep m_vals;
};




// FIXME: We should clearly put this in a namespace.  But maybe not "Gelato".
// }; /* end namespace Gelato */

#endif /* !defined(PARAMLIST_H) */
