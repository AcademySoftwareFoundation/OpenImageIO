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

#include <vector>

#include <OpenImageIO/export.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/ustring.h>


OIIO_NAMESPACE_BEGIN

/// ParamValue holds a parameter and a pointer to its value(s)
///
/// Nomenclature: if you have an array of 4 colors for each of 15 points...
///  - There are 15 VALUES
///  - Each value has an array of 4 ELEMENTS, each of which is a color
///  - A color has 3 COMPONENTS (R, G, B)
///
class OIIO_API ParamValue {
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
    ParamValue (const ustring &_name, TypeDesc _type, int _nvalues,
                Interp _interp, const void *_value, bool _copy=true) {
        init_noclear (_name, _type, _nvalues, _interp, _value, _copy);
    }
    ParamValue (string_view _name, TypeDesc _type,
                int _nvalues, const void *_value, bool _copy=true) {
        init_noclear (ustring(_name), _type, _nvalues, _value, _copy);
    }
    ParamValue (string_view _name, TypeDesc _type, int _nvalues,
                Interp _interp, const void *_value, bool _copy=true) {
        init_noclear (ustring(_name), _type, _nvalues, _interp, _value, _copy);
    }
    ParamValue (const ParamValue &p, bool _copy=true) {
        init_noclear (p.name(), p.type(), p.nvalues(), p.interp(), p.data(), _copy);
    }
    ~ParamValue () { clear_value(); }
    void init (ustring _name, TypeDesc _type, int _nvalues,
               Interp _interp, const void *_value, bool _copy=true) {
        clear_value ();
        init_noclear (_name, _type, _nvalues, _interp, _value, _copy);
    }
    void init (ustring _name, TypeDesc _type,
               int _nvalues, const void *_value, bool _copy=true) {
        init (_name, _type, _nvalues, INTERP_CONSTANT, _value, _copy);
    }
    void init (string_view _name, TypeDesc _type,
               int _nvalues, const void *_value, bool _copy=true) {
        init (ustring(_name), _type, _nvalues, _value, _copy);
    }
    void init (string_view _name, TypeDesc _type, int _nvalues,
               Interp _interp, const void *_value, bool _copy=true) {
        init (ustring(_name), _type, _nvalues, _interp, _value, _copy);
    }
    const ParamValue& operator= (const ParamValue &p) {
        init (p.name(), p.type(), p.nvalues(), p.interp(), p.data(), p.m_copy);
        return *this;
    }

    // FIXME -- some time in the future (after more cleanup), we should make
    // name() return a string_view, and use uname() for the rare time when
    // the caller truly requires the ustring.
    const ustring &name () const { return m_name; }
    const ustring &uname () const { return m_name; }
    TypeDesc type () const { return m_type; }
    int nvalues () const { return m_nvalues; }
    const void *data () const { return m_nonlocal ? m_data.ptr : &m_data; }
    int datasize () const { return m_nvalues * static_cast<int>(m_type.size()); }
    Interp interp () const { return (Interp)m_interp; }
    void interp (Interp i) { m_interp = (unsigned char )i; }

    friend void swap (ParamValue &a, ParamValue &b) {
        std::swap (a.m_name,     b.m_name);
        std::swap (a.m_type,     b.m_type);
        std::swap (a.m_nvalues,  b.m_nvalues);
        std::swap (a.m_interp,   b.m_interp);
        std::swap (a.m_data.ptr, b.m_data.ptr);
        std::swap (a.m_copy,     b.m_copy);
        std::swap (a.m_nonlocal, b.m_nonlocal);
    }

private:
    ustring m_name;           ///< data name
    TypeDesc m_type;          ///< data type, which may itself be an array
    int m_nvalues;            ///< number of values of the given type
    unsigned char m_interp;   ///< Interpolation type
    bool m_copy, m_nonlocal;
    union {
        ptrdiff_t localval;
        const void *ptr;
    } m_data;             ///< Our data, either a pointer or small local value

    void init_noclear (ustring _name, TypeDesc _type,
                       int _nvalues, const void *_value, bool _copy=true);
    void init_noclear (ustring _name, TypeDesc _type,int _nvalues,
                       Interp _interp, const void *_value,
                       bool _copy=true);
    void clear_value();
};



/// A list of ParamValue entries, that can be iterated over or searched.
/// It's really just a std::vector<ParamValue>, but with a few more handy
/// methods.
class OIIO_API ParamValueList : public std::vector<ParamValue> {
public:
    ParamValueList () { }

    /// Add space for one more ParamValue to the list, and return a
    /// reference to its slot.
    reference grow () {
        resize (size()+1);
        return back ();
    }

    /// Find the first entry with matching name, and if type != UNKNOWN,
    /// then also with matching type. The name search is case sensitive if
    /// casesensitive == true.
    iterator find (string_view name, TypeDesc type = TypeDesc::UNKNOWN,
                   bool casesensitive = true);
    iterator find (ustring name, TypeDesc type = TypeDesc::UNKNOWN,
                   bool casesensitive = true);
    const_iterator find (string_view name, TypeDesc type = TypeDesc::UNKNOWN,
                         bool casesensitive = true) const;
    const_iterator find (ustring name, TypeDesc type = TypeDesc::UNKNOWN,
                         bool casesensitive = true) const;

    /// Even more radical than clear, free ALL memory associated with the
    /// list itself.
    void free () { clear(); shrink_to_fit(); }
};


OIIO_NAMESPACE_END

#endif /* !defined(OPENIMAGEIO_PARAMLIST_H) */
