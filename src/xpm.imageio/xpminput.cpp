/*
  Copyright 2011 Piotr Zych
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
#include "xpm.h"
#include "typedesc.h"
#include "imageio.h"
#include <fstream>
#include <cstring>
#include <vector>
#include <map>
#include <cstdlib>

//Macro to swap bytes in pixel
#define SWAP32(x) (((x) & 0xff) << 24 | ((x) & 0xff00) << 8 | ((x) & 0xff0000) >> 8 | ((x) >> 24) & 0xff)

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace XPM_pvt;

class Parser;

class XPMinput : public ImageInput
{
public:
	friend class Parser;

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

	Parser * m_parser;			///<Parser handler
	std::string m_file_name;		///<Stash the file name
	FILE * m_file_ptr;			///<File handler
	uint32_t * m_scanline_data;		///<Scan-line data pointer

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
	uint32_t *get_raw_data()
	{
		m_image_data = new uint32_t[m_data.width * m_data.height];
		parse_colors(m_color_map);
		parse_image_data(m_color_map, m_image_data);
		return m_image_data;
	}

	~Parser()
	{

	}
	
private:
	bool parse()
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

	//Seeking the start of using data
	bool init_parsing()
	{
		char tmp_sing;
		int i = 0;

		while (1) {
			if (fread(&tmp_sing, 1, 1, m_file) < 1) {
				m_input.send_error("Unexpected end of file");
				return false;
			}

			if (tmp_sing == EOF) {
				m_input.send_error("File corrupted");
				return false;
			}

			if (tmp_sing == '{') {
				m_file_start = i;
				break;
			}

			++i;
		}

		return true;
	}

	bool parse_header(const std::string &header)
	{
		std::string tmp;
		std::string::const_iterator it;
		int i = 0;

		std::istringstream line(header);

		line >> m_data.width;
		line >> m_data.height;
		line >> m_data.color_table_size;
		line >> m_data.char_count;
		
		return true;
	}

	//Parsing form string to raw image data
	bool parse_image_data(std::map<std::string, uint32_t> &color_map, uint32_t* image_data)
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
				for(int j=0; j<m_data.char_count; ++j)
				{
					string_color+=(*it2);
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

	//Geting colors form string-lines
	bool parse_colors(std::map<std::string, uint32_t> &color_map)
	{
		std::vector<std::string>::iterator it;

		//Find next color value and save it in hash-map
		for (it = m_colors.begin(); it != m_colors.end(); ++it) {
			std::string tmp_line = *it;
			std::string tmp_sings = tmp_line.substr(0, m_data.char_count);
			std::string color = tmp_line.substr(tmp_line.find('#', m_data.char_count) + 1, 6);
			uint32_t tmp_val = SWAP32(strtoul(color.c_str(), NULL, 16) << 8);
			color_map.insert(std::pair<std::string, uint32_t> (tmp_sings, tmp_val));
		}

		return true;
	}

	//Geting next string form c-language table
	bool read_next_string(std::string &result)
	{
		char tmp_sing;
		std::string tmp_line;
		while (1) {
			if (fread(&tmp_sing, 1, 1, m_file) < 1) {
				m_input.send_error("Unexpected end of file");
				return false;
			}

			if (tmp_sing == '\"')
				break;
		}

		while (1) {
			if (fread(&tmp_sing, 1, 1, m_file) < 1) {
				m_input.send_error("Unexpected end of file");
				return false;
			}

			if (tmp_sing == '\"')
				break;

			tmp_line += tmp_sing;
		}

		result = tmp_line;
		return true;
	}

	FILE * m_file;				///< file handler
	XPM_data m_data;			///< XPM image data
	int m_file_start;			///< used data offset
	std::vector<std::string> m_image;	///< lines contain image data
	std::vector<std::string> m_colors;	///< lines contain colors
	std::map<std::string, uint32_t> m_color_map;	///< hash-map containing the string...
						///< ...and its corresponding color
	uint32_t * m_image_data;		///< raw parsed image data
	XPMinput & m_input;			///< pointer to XPMinput object
};

bool XPMinput::open(const std::string &name, ImageSpec &newspec)
{
	m_file_ptr = fopen(name.c_str(), "r");
	m_parser = new Parser(m_file_ptr, *this);
	if (!m_parser->open())
		return false;
	m_spec = ImageSpec(m_parser->get_width(), m_parser->get_height(), 4, TypeDesc::UINT8);
	m_spec.attribute("oiio:BitsPerSample", 24);
	m_spec.attribute("XResolution", m_parser->get_width());
	m_spec.attribute("YResolution", m_parser->get_height());
	m_spec.attribute("ResolutionUnit", "px");
	m_scanline_data = m_parser->get_raw_data();

	newspec = spec();

	return true;
}

bool XPMinput::close()
{
	if (m_file_ptr)
		fclose(m_file_ptr);
	
	delete m_parser;
	return true;
}

bool XPMinput::read_native_scanline(int y, int z, void *data)
{
	size_t width = m_spec.scanline_bytes(true);
	memcpy(data, &((uint8_t *) m_scanline_data)[y * width], width);
	return true;
}

OIIO_PLUGIN_EXPORTS_BEGIN
DLLEXPORT ImageInput *xpm_input_imageio_create()
{
	return new XPMinput;
}
DLLEXPORT int xpm_imageio_version = OIIO_PLUGIN_VERSION;
DLLEXPORT const char * xpm_input_extensions[] =
{
	"xpm", NULL
};
OIIO_PLUGIN_EXPORTS_END

OIIO_PLUGIN_NAMESPACE_END