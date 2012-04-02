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

    Parser(): m_file(NULL), m_image_data(NULL)
    {
    }

    bool open(FILE* file, XPMinput *input)
    {
        m_file = file;
        m_input = input;
        return parse();
    }

    inline int get_height()
    {
        if(m_file)
            return m_data.height;
        else
            return -1;
    }

    inline int get_width()
    {
        if(m_file)
            return m_data.width;
        else
            return -1;
    }

    //Returning count of color used in xpm file
    inline int get_color_count()
    {
        if(m_file)
            return m_data.color_table_size;
        else
            return -1;

    }

    //Returning parsed to raw data buffor
    uint32_t *get_raw_data();

    ~Parser()
    {
        if (m_image_data)
            delete m_image_data;
    }

private:
    bool parse();

    //Seeking the start of using data
    bool init_parsing();

    bool parse_header(const std::string &header);

    //Parsing form string to raw image data
    bool parse_image_data(std::map<std::string, uint32_t> &color_map, 
        uint32_t* image_data);

    //Geting colors form string-lines
    bool parse_colors(std::map<std::string, uint32_t> &color_map);

    //Geting next string form c-language table
    bool read_next_string(std::string &result);

    FILE * m_file; ///< file handler
    XPM_data m_data; ///< XPM image data
    int m_file_start; ///< used data offset
    std::vector<std::string> m_image; ///< lines contain image data
    std::vector<std::string> m_colors; ///< lines contain colors
    std::map<std::string, uint32_t> m_color_map;///< hash-map containing the 
                                                ///< ..string and its
                                                ///< ..corresponding color
    uint32_t * m_image_data; ///< raw parsed image data
    XPMinput *m_input; ///< pointer to XPMinput object
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

    XPM_pvt::Parser m_parser; ///<Parser handler
    std::string m_file_name; ///<Stash the file name
    FILE * m_file_ptr; ///<File handler
    uint32_t * m_scanline_data; ///<Scan-line data pointer

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

    m_scanline_data = m_parser.get_raw_data();

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
    memcpy(data, &((uint8_t *) m_scanline_data)[y * width], width);
    return true;
}



uint32_t*
Parser::get_raw_data()
{  
    if(!m_file)
        return NULL;
    
    m_image_data = new uint32_t[m_data.width * m_data.height];
    parse_colors(m_color_map);
    parse_image_data(m_color_map, m_image_data);
    return m_image_data;
}



bool
Parser::parse()
{
    std::string header;
    std::string token;

    if (!init_parsing())
        return false;

    if (!read_next_string(header))
        return false;

    if (!parse_header(header))
        return false;

    //Reading lines contain used colors
    for (unsigned int i = 0; i < m_data.color_table_size; ++i) {
        if (!read_next_string(token))
            return false;

        m_colors.push_back(token);
    }

    //Reading lines contain data
    for (unsigned int i = 0; i < m_data.height; ++i) {
        if (!read_next_string(token))
            return false;

        m_image.push_back(token);
    }

    return true;
}



bool
Parser::init_parsing()
{
    char tmp_sign;
    int i = 0;

    while (1) {
        tmp_sign = fgetc(m_file);
        
        if (feof(m_file)) {
            m_input->send_error("Unexpected end of file");
            return false;
        }

        if (tmp_sign == '{') {
            m_file_start = i;
            break;
        }

        ++i;
    }

    return true;
}



bool
Parser::parse_header(const std::string &header)
{
    std::istringstream line(header);

    line >> m_data.width;
    line >> m_data.height;
    line >> m_data.color_table_size;
    line >> m_data.char_count;
    
    if(line.eof()) {
        m_data.hotspot = false;
        return true;
    }
    
    m_data.hotspot = true;
    
    line >> m_data.hotspot_x;
    line >> m_data.hotspot_y;
    
    return true;
}



bool
Parser::parse_image_data(std::map<std::string, uint32_t> &color_map, uint32_t* 
    image_data)
{
    
    std::vector<std::string>::iterator it1;
    std::string::iterator it2;
    int i = 0;

    for (it1 = m_image.begin(); it1 != m_image.end(); ++it1) {
        //Get next line
        std::string tmp_line = *it1;
        for (it2 = tmp_line.begin(); it2 != tmp_line.end();) {

            //get color form line
            std::string string_color;
            for (unsigned int j = 0; j < m_data.char_count; ++j) {
                string_color += (*it2);
                ++it2;
            }
            //Parse color using hash-map
            uint32_t tmp_color = color_map[string_color];
            image_data[i] = tmp_color;

            ++i;
        }
    }

    return true;
}



bool
Parser::parse_colors(std::map<std::string, uint32_t> &color_map)
{
    std::vector<std::string>::iterator it;

    //Find next color value and save it in hash-map
    for (it = m_colors.begin(); it != m_colors.end(); ++it) {
        std::string tmp_line = *it;
        std::string tmp_sings = tmp_line.substr(0, m_data.char_count);

        /*
         Currently, support for only visual color mode and transparency
         */

        if (tmp_line.find("None")!=std::string::npos) {
            color_map.insert(std::pair<std::string, uint32_t > (tmp_sings, 0));
        }
        
        std::istringstream str(tmp_line.substr(m_data.char_count, 
                tmp_line.length()-1));
        
        while(!str.eof()) {
            std::string tmp_buff;
            uint32_t converted_value=0;
            
            str >> tmp_buff;
            
            if (tmp_buff == "c") {
                
                str >> tmp_buff;
                
                if(tmp_buff.length() < 1)
                {
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
            
            color_map.insert(std::pair<std::string, uint32_t > (tmp_sings, 
                converted_value));
        }
        
    }

    return true;
}

bool
Parser::read_next_string(std::string &result)
{
    char tmp_sign;
    std::string tmp_line;
    while (1) {
        if ((tmp_sign = fgetc(m_file)) == EOF) {
            m_input->send_error("Unexpected end of file");
            return false;
        }

        if (tmp_sign == '\"')
            break;
    }

    while (1) {
        if ((tmp_sign = fgetc(m_file)) == EOF) {
            m_input->send_error("Unexpected end of file");
            return false;
        }

        if (tmp_sign == '\"')
            break;
        if (tmp_sign == '\n')
            continue;
        
        tmp_line += tmp_sign;
    }

    result = tmp_line;
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
