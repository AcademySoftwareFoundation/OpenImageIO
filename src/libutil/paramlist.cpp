/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz
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
// (this is the MIT license)
/////////////////////////////////////////////////////////////////////////////


#include <cstdio>
#include <cstdlib>

#include "dassert.h"
#include "ustring.h"

#undef EXPORT_H
#define DLL_EXPORT_PUBLIC /* Because we are implementing paramlist */
#include "paramlist.h"
#undef DLL_EXPORT_PUBLIC


// FIXME: We should clearly be using a namespace.  But maybe not "Gelato".
// using namespace Gelato;


void
ParamValue::init (ustring _name, ParamType _type,
                  int _nvalues, const void *_value, bool _copy)
{
    m_name = _name;
    m_type = _type;
    m_nvalues = _nvalues;
    size_t size = (size_t) (m_nvalues * m_type.datasize());
    bool small = (size <= sizeof(m_data));

    if (_copy || small) {
        if (small) {
            memcpy (&m_data, _value, size);
            m_copy = false;
            m_nonlocal = false;
        } else {
            m_data.ptr = malloc (size);
            memcpy ((char *)m_data.ptr, _value, size);
            m_copy = true;
            m_nonlocal = true;
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


