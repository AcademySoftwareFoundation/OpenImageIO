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


#ifndef VARYINGREF_H
#define VARYINGREF_H


template<class T>
class VaryingRef {
public:
    VaryingRef () { }
    VaryingRef (T *ptr, int step=0) { init (ptr,step); }
    VaryingRef (T &ptr) { init (&ptr, 0); }
    void init (T *ptr, int step=0) {
        m_ptr = ptr;
        m_step = step;
    }

    bool is_null () const { return (m_ptr == 0); }

    operator bool() const { return (m_ptr != 0); }

    bool is_varying () const { return (m_step != 0); }
    bool is_uniform () const { return (m_step == 0); }
    VaryingRef & operator++ () {  // Prefix form ++i
        *((char **)&m_ptr) += m_step;
        return *this;
    }
    void operator++ (int) {  // Postfix form i++ : return nothing to avoid copy
        // VaryingRef<T> tmp = *this;
        *((char **)&m_ptr) += m_step;
        // return tmp;
    }
    T & operator* () const { return *m_ptr; }
    T & operator[] (int i) const { return *(T *) ((char *)m_ptr + i*m_step); }

    T * ptr () const { return m_ptr; }

private:
    T  *m_ptr;    ///< Pointer to value
    int m_step;   ///< Distance between successive values -- in BYTES!
};


#endif // VARYINGREF_H
