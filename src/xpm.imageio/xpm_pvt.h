/*
  Copyright 2011 Piotr Zych.
  All Rights Reserved.
  Based on BSD-licensed software Copyright 2004 NVIDIA Corp.

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

/*
  XPM format specyfication:
  http://en.wikipedia.org/wiki/X_PixMap
 */

#ifndef OPENIMAGEIO_XPM_H
#define	OPENIMAGEIO_XPM_H

#include <stdint.h>
#include "imageio.h"


OIIO_PLUGIN_NAMESPACE_BEGIN


class XPMinput;

namespace XPM_pvt {
    
//Macro to swap bytes in pixel
#define SWAP32(x) ((((x) & 0xff) << 24) | (((x) & 0xff00) << 8) | (((x) & 0xff0000) >> 8) | (((x) >> 24) & 0xff))

    struct XPM_data {
        uint32_t width;
        uint32_t height;
        uint32_t color_table_size;
        uint32_t char_count;
    };

    class Parser
    {
    public:
            Parser(FILE* file, XPMinput &input):
            m_file(file), m_input(input)
            {  }

            bool open()
            {
                    return parse();
            }

            inline int get_height()
            {
                    return m_data.height;
            }

            inline int get_width()
            {
                    return m_data.width;
            }

            //Returning count of color used in xpm file
            inline int get_color_count()
            {
                    return m_data.color_table_size;

            }

            //Returning parsed to raw data buffor
            uint32_t *get_raw_data();

            ~Parser()
            {

            }

    private:
            bool parse();

            //Seeking the start of using data
            bool init_parsing();

            bool parse_header(const std::string &header);

            //Parsing form string to raw image data
            bool parse_image_data(std::map<std::string, uint32_t> &color_map, uint32_t* image_data);

            //Geting colors form string-lines
            bool parse_colors(std::map<std::string, uint32_t> &color_map);

            //Geting next string form c-language table
            bool read_next_string(std::string &result);

            FILE * m_file;				///< file handler
            XPM_data m_data;                            ///< XPM image data
            int m_file_start;                           ///< used data offset
            std::vector<std::string> m_image;           ///< lines contain image data
            std::vector<std::string> m_colors;          ///< lines contain colors
            std::map<std::string, uint32_t> m_color_map;///< hash-map containing the string...
                                                        ///< ...and its corresponding color
            uint32_t * m_image_data;                    ///< raw parsed image data
            XPMinput & m_input;                         ///< pointer to XPMinput object
    };

}; //namespace XPM_pvt

class XPMinput : public ImageInput
{
public:
    friend class XPM_pvt::Parser;

    XPMinput()
    { }

    virtual ~XPMinput()
    { }

    virtual const char * format_name(void) const
    {
	    return "xpm";
    }

    virtual bool open(const std::string &name, ImageSpec &newspec);
    virtual bool close();
    virtual bool read_native_scanline(int y, int z, void *data);

private:
    void send_error (const char *err)
    {
	    error("%s", err);
    }

    XPM_pvt::Parser * m_parser;			///<Parser handler
    std::string m_file_name;		///<Stash the file name
    FILE * m_file_ptr;			///<File handler
    uint32_t * m_scanline_data;		///<Scan-line data pointer

};

OIIO_PLUGIN_NAMESPACE_END

#endif	/* OPENIMAGE_XPM_H */

