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

#include <iostream>
#include <sstream>
#include <string>

#include <fstream>
#include <cstring>
#include <vector>
#include <map>
#include <cstdlib>
#include <fmath.h>
#include "errno.h"
#include "xpm_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace XPM_pvt;

namespace XPM_pvt
{

class Parser {
public:

    Parser(): m_file(NULL), m_buffer(NULL), m_last_idx(0), m_buffer_length(0)
    { }

    bool open(FILE* file, XPMinput* input) // opens file and initializing parisng
    {
        m_file = file;
        m_input = input;
        return parse();
    }

    inline int get_height() // returns height of image
    {
        if(m_file)
            return m_header.height;
        else
            return -1;
    }

    inline int get_width() // returns width of image
    {
        if(m_file)
            return m_header.width;
        else
            return -1;
    }
    
    bool get_line(int y, int z, void *data); // gets y-th line from file 
                                             // returns false if ther was problem
                                             // and true if line was read correctly

    ~Parser()
    {
        if (m_buffer)
            delete m_buffer;
        if(m_color_map)
            delete m_color_map;
    }

private:
    bool parse(); // this method initialize parsing of file 
    
    bool parse_header(const std::string &header); // parsing header

    bool parse_color(char* line, int length); // this method gets colors from 
                                              // line and converts value to
                                              // pair <long, uint32_t>

    int find_next_element(); // read file, and save to buffer until sign 
                             // '\"' appears; return count of read signs
    
    long char_to_long(const char* buff, int lenght); // method converts array 
                                                     // of chars (max 4 elements)
                                                     // to long int
    

    FILE * m_file;
    XPM_data m_header;
    long m_color_offset;
    long m_data_offset;
    char * m_buffer;
    int m_last_idx;
    int m_buffer_length;
    std::map<long, uint32_t> * m_color_map;
    XPMinput * m_input; ///< pointer to XPMinput object
};

};


class XPMinput : public ImageInput
{
public:
    friend class XPM_pvt::Parser;

    XPMinput()
    {
        m_file_ptr = NULL;
    }

    virtual ~XPMinput()
    {
        if(m_file_ptr) {
            fclose(m_file_ptr);
        }
    }

    virtual const char * format_name(void) const
    {
        return "xpm";
    }

    virtual bool open(const std::string &name, ImageSpec &newspec);
    
    virtual bool close();
    
    virtual bool read_native_scanline(int y, int z, void *data);

private:
    void send_error(const std::string &err)
    {
        error("%s", err.c_str());
    }

    XPM_pvt::Parser m_parser; // Parser handler
    std::string m_file_name; // Stash the file name
    FILE * m_file_ptr; // File handler
};



bool
XPMinput::open(const std::string &name, ImageSpec &newspec)
{
    m_file_ptr = fopen(name.c_str(), "r");
    
    if (!m_file_ptr)
    {
        send_error(std::string("Cannot open file: ") + std::string(strerror(errno)));
        return false;
    }
    
    if (!m_parser.open(m_file_ptr, this))
    {
        send_error("Cannot parse this file");
        fclose(m_file_ptr);
        return false;
    }
    
    m_spec = ImageSpec(m_parser.get_width(), m_parser.get_height(), 4, 
        TypeDesc::UINT8);
    
    m_spec.attribute("oiio:BitsPerPixel", 32);
    newspec = spec();
    return true;
}



bool
XPMinput::close()
{
    if (m_file_ptr) {
        fclose(m_file_ptr);
        m_file_ptr = NULL;
    }
    
    return true;
}



bool
XPMinput::read_native_scanline(int y, int z, void *data)
{
    size_t width = m_spec.scanline_bytes(true);
    return m_parser.get_line(y, width, data);
}



bool
Parser::parse() 
{
    //buffor for single sign
    char tmp_sign;
    
    //signs counter
    int i=0;
    
    //this loop finds the begining of header
    while (1) {
        
       //take next sign from file, if end of file, return false
       if ((tmp_sign = fgetc(m_file)) == EOF) {
            m_input->send_error("Unexpected end of file");
            return false;
        }
        
        //if sign is '{', it means start of file.
        if (tmp_sign == '{') {
            break;
        }
        
        ++i;
    }
    
    //finds the start of header
    for(int i=0; i<2; ++i)
        find_next_element();
    
    parse_header(m_buffer);
    
    //loop looks for lines with colors, and parse it
    for(unsigned int j=0; j<m_header.color_table_size; ++j) {
        if(find_next_element()<0)
            return false;
        
        int count = find_next_element();
        if(count < 0)
            return false;
        if(!parse_color(m_buffer, count))
            return false;
    }
    
    //sets position to the beginning of image data
    if(find_next_element()<0)
        return false;
    
    //saves position of the begining of image data
    m_data_offset = ftell(m_file);
    
    return true;
}

int
Parser::find_next_element() 
{
    char tmp_sign;
    int i=0;
    while (1) {
        if ((tmp_sign = fgetc(m_file)) == EOF) {
            m_input->send_error("Unexpected end of file");
            return -1;
        }

        if (tmp_sign == '\"')
            break;
        
        if(!m_buffer)
            m_buffer = new char[255];
        
        //if buffer is too short, free memory and create new longer buffer
        if( !(i<m_buffer_length-1) )
        {
            char * new_buff = new char[m_buffer_length+255];
            memcpy(new_buff, m_buffer, m_buffer_length);
            m_buffer_length+=255;
            delete m_buffer;
            m_buffer = new_buff;
        }
        
        m_buffer[i] = tmp_sign;
        ++i;
    }
    
    //cstrings should end 0
    m_buffer[i] = '\0';
    return i;
}

bool
Parser::parse_header(const std::string &header)
{
    std::istringstream line(header);

    line >> m_header.width;
    line >> m_header.height;
    line >> m_header.color_table_size;
    line >> m_header.char_count;
    
    if(!m_buffer)
        m_buffer = new char[255];
    
    m_color_map = new std::map<long, uint32_t>();
    
    if(line.eof()) {
        m_header.hotspot = false;
        return true;
    }
    
    m_header.hotspot = true;
    
    line >> m_header.hotspot_x;
    line >> m_header.hotspot_y;
    
    return true;
}

long 
Parser::char_to_long(const char* buff, int length) 
{
    long result = 0;
    
    if(length>0)
        result |= buff[0] << 24;
    
    if(length>1)
        result |= buff[1] << 16;
    
    if(length>2)
        result |= buff[2] << 8;
    
    if(length>3)
       result |= buff[3];
    
    return result;
}

bool
Parser::parse_color(char * line, int length)
{
    std::string tmp_line (line, length);
    
    //buffer for current signs
    char tmp_signs[m_header.char_count];
    strncpy(tmp_signs, line, m_header.char_count);
    
    //color symbols coverted to long
    long long_buff = char_to_long(tmp_signs, m_header.char_count);
    
    //transparency support
    if (tmp_line.find("None")!=std::string::npos) {
        m_color_map->insert(std::pair<long, uint32_t> (long_buff, 0));
    }
    
    std::istringstream str(std::string(&line[m_header.char_count]));

    while(!str.eof()) {
        
        //buffor for next string from line
        std::string tmp_buff;
        uint32_t converted_value=0;

        str >> tmp_buff;

        if (tmp_buff == "c") {

            str >> tmp_buff;

            if(tmp_buff.length() < 1) {
                m_input->send_error("File corrupted");
                return false;
            }
            
            switch (tmp_buff[0]) {
            case '#': {
                int color_length = tmp_buff.length()-1;
                std::string cr;
                std::string cg;
                std::string cb;
                uint8_t R, G, B;

                switch (color_length) {
                case 3:
                    cr = tmp_buff.substr(1, 1);
                    R = strtoul(cr.c_str(), NULL, 16)<<8;
                    cg = tmp_buff.substr(2, 1);
                    G = strtoul(cg.c_str(), NULL, 16)<<8;
                    cb = tmp_buff.substr(3, 1);
                    B = strtoul(cb.c_str(), NULL, 16)<<8;
                    break;
                case 6:
                    cr = tmp_buff.substr(1, 2);
                    R = strtoul(cr.c_str(), NULL, 16);
                    cg = tmp_buff.substr(3, 2);
                    G = strtoul(cg.c_str(), NULL, 16);
                    cb = tmp_buff.substr(5, 2);
                    B = strtoul(cb.c_str(), NULL, 16);
                    break;
                case 12:
                    cr = tmp_buff.substr(1, 4);
                    R = strtoul(cr.c_str(), NULL, 16)>>8;
                    cg = tmp_buff.substr(5, 4);
                    G = strtoul(cg.c_str(), NULL, 16)>>8;
                    cb = tmp_buff.substr(9, 4);
                    B = strtoul(cb.c_str(), NULL, 16)>>8;
                    break;
                default:
                    m_input->send_error("invalid color format");
                    R = G = B = 0;
                    break;
                }

                converted_value = (255 << 24) | (B << 16) | (G << 8) | 
                    (R << 0);
                break;
            }
            case '%':
                m_input->send_error("no support for hsv color");
                converted_value = (255 << 24);
                break;
            default:
                m_input->send_error("no support for symbolic names color");

                converted_value = (255 << 24);
                break;
            }
        } else if(tmp_buff == "m") {
            m_input->send_error("no support for monochrome color");
            str >> tmp_buff;
        } else if(tmp_buff == "g") {
            m_input->send_error("no support for gray scale color");
            str >> tmp_buff;
        } else if(tmp_buff == "g4") {
            m_input->send_error(
                "no support for symbolic for four-level gray scale color");
            str >> tmp_buff;
        } else if(tmp_buff == "s") {
            m_input->send_error("no support for symbolic color names");
            str >> tmp_buff;
        }
        
        //insert converted color to map
        m_color_map->insert(std::pair<long, uint32_t>(long_buff, converted_value));
    }

    return true;
}

bool
Parser::get_line(int y, int z, void* data)
{

    if(!(m_file && m_data_offset))
        return false;
    
    // if last line was not next to current, 
    // this loop seek place when it should start read data
    if(y!=m_last_idx+1) {
        fseek(m_file, m_data_offset, SEEK_SET);
        for(int i=0; i<y; ++i) {
            for(int j=0; j<2; ++j) {
                int k = find_next_element();
                if(k<0)
                    return false;
            }
        }
    }
    
    m_last_idx=y;
    
    char tmp_buff[m_header.char_count];
    for(int i=0; i<z/4; ++i) {
        for(unsigned int j=0; j<m_header.char_count; ++j) {
            char tmp_sign = fgetc(m_file);
            if (tmp_sign == EOF) {
                m_input->send_error("Unexpected end of file");
                return false;
            }
            
            if(tmp_sign == '\"') {
                m_input->send_error("File corrupted");
                return false;
            }
            tmp_buff[j] = tmp_sign; 
        }
        
        long long_buff = char_to_long(tmp_buff, m_header.char_count);
        uint32_t cl = (*m_color_map)[long_buff];
        ((uint32_t*)data)[i] = cl;        
    }
    
    fgetc(m_file);
    find_next_element();
    
    return true;
}



OIIO_PLUGIN_EXPORTS_BEGIN
DLLEXPORT ImageInput *xpm_input_imageio_create()
{
    return new XPMinput;
}
DLLEXPORT int xpm_imageio_version = OIIO_PLUGIN_VERSION;
DLLEXPORT const char * xpm_input_extensions[] = {
    "xpm", NULL
};
OIIO_PLUGIN_EXPORTS_END

OIIO_PLUGIN_NAMESPACE_END
