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


/// \file
///
/// Define the ParamValue and ParamValueList classes, which are used to
/// store lists of arbitrary name/data pairs for internal storage of
/// parameter lists, attributes, geometric primitive data, etc.


#ifndef OPENIMAGEIO_PARAMLIST_H
#define OPENIMAGEIO_PARAMLIST_H

#if defined(_MSC_VER)
// Ignore warnings about DLL exported classes with member variables that are template classes.
// This happens with the Rep m_vals member variable of ParamValueList below, which is a std::vector<T>.
#  pragma warning (disable : 4251)
#endif

#include <vector>

#include "export.h"
#include "typedesc.h"
#include "ustring.h"


OIIO_NAMESPACE_ENTER
{

/// ParamValue holds a parameter and a pointer to its value(s)
///
/// Nomenclature: if you have an array of 4 colors for each of 15 points...
///  - There are 15 VALUES
///  - Each value has an array of 4 ELEMENTS, ecah of which is a color
///  - A color has 3 COMPONENTS (R, G, B)
///
class DLLPUBLIC ParamValue {
public:
    /// Interpolation types
    ///
    enum Interp {
        INTERP_CONSTANT = 0, ///< Constant for all pieces/faces
        INTERP_PERPIECE = 1, ///< Piecewise constant per piece/face
        INTERP_LINEAR = 2,   ///< Linearly interpolated across each piece/face
        INTERP_VERTEX = 3    ///< Interpolated like vertices
    };

    ParamValue () : m_type(TypeDesc::UNKNOWN), m_nvalues(0), 
                    m_interp(INTERP_CONSTANT), m_copy(false), m_nonlocal(false)
    {
        m_data.ptr = NULL;
    }
    ParamValue (const ustring &_name, TypeDesc _type,
                int _nvalues, const void *_value, bool _copy=true) {
        init_noclear (_name, _type, _nvalues, _value, _copy);
    }
    ParamValue (const std::string &_name, TypeDesc _type,
                int _nvalues, const void *_value, bool _copy=true) {
        init_noclear (ustring(_name.c_str()), _type, _nvalues, _value, _copy);
    }
    ParamValue (const char *_name, TypeDesc _type,
                int _nvalues, const void *_value, bool _copy=true) {
        init_noclear (ustring(_name), _type, _nvalues, _value, _copy);
    }
    ParamValue (const ParamValue &p, bool _copy=true) {
        init_noclear (p.name(), p.type(), p.nvalues(), p.data(), _copy);
    }
    ~ParamValue () { clear_value(); }
    void init (ustring _name, TypeDesc _type,
               int _nvalues, const void *_value, bool _copy=true) {
        clear_value ();
        init_noclear (_name, _type, _nvalues, _value, _copy);
    }
    void init (std::string _name, TypeDesc _type,
               int _nvalues, const void *_value, bool _copy=true) {
        init (ustring(_name), _type, _nvalues, _value, _copy);
    }
    const ParamValue& operator= (const ParamValue &p) {
        init (p.name(), p.type(), p.nvalues(), p.data(), p.m_copy);
        return *this;
    }

    const ustring &name () const { return m_name; }
    TypeDesc type () const { return m_type; }
    int nvalues () const { return m_nvalues; }
    const void *data () const { return m_nonlocal ? m_data.ptr : &m_data; }
    int datasize () const { return m_nvalues * static_cast<int>(m_type.size()); }

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
    TypeDesc m_type;          ///< data type, which may itself be an array
    int m_nvalues;            ///< number of values of the given type
    union {
        ptrdiff_t localval;
        const void *ptr;
    } m_data;             ///< Our data, either a pointer or small local value
    unsigned char m_interp;   ///< Interpolation type
    bool m_copy, m_nonlocal;

    void init_noclear (ustring _name, TypeDesc _type,
                       int _nvalues, const void *_value, bool _copy=true);
    void clear_value();
};



/// A list of ParamValue entries, that can be iterated over or searched.
///
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

    /// Add space for one more ParamValue to the list, and return a
    /// reference to its slot.
    reference grow () {
        resize (size()+1);
        return back ();
    }

    /// Add a ParamValue to the end of the list.
    ///
    void push_back (ParamValue &p) { m_vals.push_back (p); }
    
    /// Removes from the ParamValueList container a single element.
    /// 
    iterator erase (iterator position) { return m_vals.erase (position); }
    
    /// Removes from the ParamValueList container a range of elements ([first,last)).
    /// 
    iterator erase (iterator first, iterator last) { return m_vals.erase (first, last); }
    
    /// Remove all the values in the list.
    ///
    void clear () { m_vals.clear(); }

    /// Even more radical than clear, free ALL memory associated with the
    /// list itself.
    void free () { Rep tmp; std::swap (m_vals, tmp); }

private:
    Rep m_vals;
};


}
OIIO_NAMESPACE_EXIT

#endif /* !defined(OPENIMAGEIO_PARAMLIST_H) */
