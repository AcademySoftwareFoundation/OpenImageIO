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


#ifndef TEXTURE_PVT_H
#define TEXTURE_PVT_H



class TextureSystemImpl : public TextureSystem {
public:
    TextureSystemImpl (void) { }
    virtual ~TextureSystemImpl () { };

    // Set options
    virtual void max_open_files (int nfiles) { m_max_open_files = nfiles; }
    virtual void max_memory_MB (float size) {
        m_max_memory_MB = size;
        m_max_memory_bytes = (int)(size * 1024 * 1024);
    }
    virtual void searchpath (const ustring &path) { m_searchpath = path; }
    
    // Retrieve options
    virtual int max_open_files () const { return m_max_open_files; }
    virtual float max_memory_MB () const { return m_max_memory_MB; }
    virtual ustring searchpath () const { return m_searchpath; }

    /// Retrieve filtered texture lookups for several points.
    virtual void texture (ustring filename, TextureOptions &options,
                          Runflag *runflags, int firstactive, int lastactive,
                          VaryingRef<float> s, VaryingRef<float> t,
                          VaryingRef<float> dsdx, VaryingRef<float> dtdx,
                          VaryingRef<float> dsdy, VaryingRef<float> dtdy,
                          float *result, int threadid=-1) { }

private:
    void init ();

    int m_max_open_files;
    float m_max_memory_MB;
    size_t m_max_memory_bytes;
    ustring m_searchpath;
};


#endif // TEXTURE_PVT_H
