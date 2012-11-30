/*
  Copyright 2010 Larry Gritz and the other authors and contributors.
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

#include <string>
#include <fstream>
#include <cstdlib>

#include "export.h"
#include "filesystem.h"
#include "imageio.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

class PNMInput : public ImageInput {
public:
    virtual const char* format_name (void) const { return "pnm"; }
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool close ();
    virtual int current_subimage (void) const { return 0; }
    virtual bool read_native_scanline (int y, int z, void *data);

private:
    std::ifstream m_file;
    std::string m_current_line; ///< Buffer the image pixels
    const char * m_pos;
    unsigned int m_pnm_type, m_max_val;

    bool read_file_scanline (void * data);
    bool read_file_header ();
};



// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

    OIIO_EXPORT ImageInput* pnm_input_imageio_create () { return new PNMInput; }

    OIIO_EXPORT int pnm_imageio_version = OIIO_PLUGIN_VERSION;

    OIIO_EXPORT const char* pnm_input_extensions[] = {
        "ppm","pgm","pbm","pnm", NULL
    };

OIIO_PLUGIN_EXPORTS_END


inline bool
nextLine (std::ifstream &file, std::string &current_line, const char * &pos) 
{   
    if (!file.good())
        return false;
    getline (file, current_line);
    if (file.fail())
        return false;
    pos = current_line.c_str();
    return true;
}



inline const char * 
nextToken (std::ifstream &file, std::string &current_line, const char * &pos)
{		
    while (1) {
        while (isspace (*pos)) 
            pos++;
        if (*pos)
            break;
        else 
            nextLine (file, current_line, pos);
    }
    return pos;
}



inline const char *
skipComments (std::ifstream &file, std::string &current_line, 
              const char * & pos, char comment = '#')
{		
    while (1) {
        nextToken (file, current_line, pos);
        if (*pos == comment)
            nextLine (file, current_line, pos);
        else 
            break;
    }
    return pos;
}



inline bool
nextVal (std::ifstream & file, std::string &current_line,
         const char * &pos, int &val, char comment = '#')
{
    skipComments (file, current_line, pos, comment);
    if (!isdigit (*pos))
        return false;
    val = strtol (pos,(char**) &pos, 10);
    return true;
}



template <class T> 
inline void 
invert (const T *read, T *write, imagesize_t nvals)
{
    for (imagesize_t i=0; i < nvals; i++) 
        write[i] = std::numeric_limits<T>::max() - read[i];
}



template <class T> 
inline bool 
ascii_to_raw (std::ifstream &file, std::string &current_line, const char * &pos,
              T *write, imagesize_t nvals, T max)
{
    if (max)
        for (imagesize_t i=0; i < nvals; i++) {
            int tmp;
            if (!nextVal (file, current_line, pos, tmp))
                return false;
            write[i] = std::min ((int)max, tmp) * std::numeric_limits<T>::max() / max;
        }
    else
        for (imagesize_t i=0; i < nvals; i++) 
            write[i] = std::numeric_limits<T>::max();
    return true;
}



template <class T> 
inline void 
raw_to_raw (const T *read, T *write, imagesize_t nvals, T max)
{
    if (max)
        for (imagesize_t i=0; i < nvals; i++) {
            int tmp = read[i];
            write[i] = std::min ((int)max, tmp) * std::numeric_limits<T>::max() / max;
        }
    else
        for (imagesize_t i=0; i < nvals; i++) 
            write[i] = std::numeric_limits<T>::max();
}



inline void 
unpack (const unsigned char * read, unsigned char * write, imagesize_t size)
{
    imagesize_t w = 0, r = 0;	
    unsigned char bit = 0x7, byte = 0;
    for (imagesize_t x = 0; x < size; x++) {	
        if (bit == 0x7)
            byte = ~read[r++];
        write[w++] = 0 - ((byte & (1 << bit)) >> bit);//assign expanded bit
        bit = (bit - 1) & 0x7; // limit bit to [0; 8[
    }
}



template <class T>
inline bool
read_int (std::istream &in, T &dest, char comment='#')
{
    T ret;
    char c;
    while (!in.eof()) {
        in >> ret;
        if (!in.good()){
            in.clear();
            in >> c;
            if (c == comment)
                in.ignore (std::numeric_limits<std::streamsize>::max(), '\n');
            else
                return false;
        } else {
            dest = ret;
            return true;
        }
    }
    return false;
}



bool 
PNMInput::read_file_scanline (void * data)
{
    try {

    std::vector<unsigned char> buf;
    bool good = true;
    if (!m_file.is_open())
        return false;
    int nsamples = m_spec.width * m_spec.nchannels;

    if (m_pnm_type >= 4 && m_pnm_type <= 6){
        int numbytes;
        if (m_pnm_type == 4)
            numbytes = (m_spec.width + 7) / 8;
        else
            numbytes = m_spec.scanline_bytes();
        buf.resize (numbytes);
        m_file.read ((char*)&buf[0], numbytes);
        if (!m_file.good())
            return false;
    }

    switch (m_pnm_type) {
        //Ascii 
        case 1:
            good &= ascii_to_raw (m_file, m_current_line, m_pos, (unsigned char *) data, 
                                  nsamples, (unsigned char)m_max_val);
            invert ((unsigned char *)data, (unsigned char *)data, nsamples); 
            break;
        case 2:
        case 3:
            if (m_max_val > std::numeric_limits<unsigned char>::max())
                good &= ascii_to_raw (m_file, m_current_line, m_pos, (unsigned short *) data, 
                                      nsamples, (unsigned short)m_max_val);
            else 
                good &= ascii_to_raw (m_file, m_current_line, m_pos, (unsigned char *) data, 
                                      nsamples, (unsigned char)m_max_val);
            break;
        //Raw
        case 4:
            unpack (&buf[0], (unsigned char *)data, nsamples);
            break;
        case 5:
        case 6:
            if (m_max_val > std::numeric_limits<unsigned char>::max())
                raw_to_raw ((unsigned short *)&buf[0], (unsigned short *) data, 
                            nsamples, (unsigned short)m_max_val);
            else 
                raw_to_raw ((unsigned char *)&buf[0], (unsigned char *) data, 
                            nsamples, (unsigned char)m_max_val);
            break;
        default:
            return false;
    }
    return good;

    }
    catch (const std::exception &e) {
        error ("PNM exception: %s", e.what());
        return false;
    }
}



bool
PNMInput::read_file_header ()
{
    try {

    unsigned int width, height;
    char c;
    if (!m_file.is_open())
        return false;

    m_file >> c >> m_pnm_type;
    
    //MagicNumber
    if (c != 'P')
        return false;
    if (!(m_pnm_type >= 1 && m_pnm_type <= 6))
        return false;

    //Size
    if (!read_int (m_file, width))
        return false; 
    if (!read_int (m_file, height))
        return false; 
    
    //Max Val
    if (m_pnm_type != 1 && m_pnm_type != 4) {
        if (!read_int (m_file, m_max_val))
            return false;
    } else
        m_max_val = 1;
    
    //Space before content
    if (!(isspace (m_file.get()) && m_file.good()))
        return false;

    if (m_pnm_type == 3 || m_pnm_type == 6)
        m_spec =  ImageSpec (width, height, 3, 
                (m_max_val > 255) ? TypeDesc::UINT16 : TypeDesc::UINT8);
    else    
        m_spec =  ImageSpec (width, height, 1, 
                (m_max_val > 255) ? TypeDesc::UINT16 : TypeDesc::UINT8);

    if (m_spec.nchannels == 1)
        m_spec.channelnames[0] = "I";
    else
        m_spec.default_channel_names();

    if (m_pnm_type >= 1 && m_pnm_type <= 3)
        m_spec.attribute ("pnm:binary", 0);
    else
        m_spec.attribute ("pnm:binary", 1);

    m_spec.attribute ("oiio:BitsPerSample", ceilf (logf (m_max_val + 1)/logf (2)));
    return true;
    }
    catch (const std::exception &e) {
        error ("PNM exception: %s", e.what());
        return false;
    }
}



bool
PNMInput::open (const std::string &name, ImageSpec &newspec)
{
    if (m_file.is_open()) //close previously opened file
        m_file.close();

    Filesystem::open (m_file, name, std::ios::in|std::ios::binary);

    m_current_line = "";
    m_pos = m_current_line.c_str();

    if (!read_file_header())
        return false;

    newspec = m_spec;
    return true;
}



bool
PNMInput::close ()
{
    m_file.close();
    return true;
}



bool
PNMInput::read_native_scanline (int y, int z, void *data)
{
    if (z)
        return false;
    if (!read_file_scanline (data))
        return false;
    return true;
}

OIIO_PLUGIN_NAMESPACE_END
