/////////////////////////////////////////////////////////////////////////////
// Copyright 2008 Larry Gritz
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


/////////////////////////////////////////////////////////////////////////////
// Private definitions internal to the jpeg.imageio plugin
/////////////////////////////////////////////////////////////////////////////


#ifndef JPEG_PVT_H
#define JPEG_PVT_H


struct TIFFDirEntry;

void exif_from_APP1 (ImageSpec &spec, unsigned char *buf);
void read_exif_tag (ImageSpec &spec, TIFFDirEntry *dirp,
                    const char *buf, bool swab);
void add_exif_item_to_spec (ImageSpec &spec, const char *name,
                            TIFFDirEntry *dirp, const char *buf, bool swab);

void APP1_exif_from_spec (ImageSpec &spec, std::vector<char> &exif);



#endif /* JPEG_PVT_H */

