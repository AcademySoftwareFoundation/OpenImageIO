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

#include "OpenImageIO/dassert.h"
#include "OpenImageIO/ustring.h"
#include "OpenImageIO/paramlist.h"


OIIO_NAMESPACE_ENTER
{


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
                m_data.localval = 0;
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



void
ParamValue::clear_value ()
{
    if (m_copy && m_nonlocal && m_data.ptr)
        free ((void *)m_data.ptr);
    m_data.ptr = NULL;
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


}
OIIO_NAMESPACE_EXIT
