/*
  Copyright 2013 Mark Boorer and the other authors and contributors.
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

#include "imageio.h"
#include <iostream>
#include "libraw/libraw.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

class RawInput : public ImageInput {
public:
	RawInput () : m_image(NULL) {}
	virtual ~RawInput() { close(); }
	virtual const char * format_name (void) const { return "raw"; }
	virtual bool open (const std::string &name, ImageSpec &newspec);
	virtual bool close();
	virtual bool read_native_scanline (int y, int z, void *data);

private:
	bool process();
	LibRaw m_processor;
	libraw_processed_image_t *m_image;
};



// Export version number and create function symbols
OIIO_PLUGIN_EXPORTS_BEGIN

	OIIO_EXPORT int raw_imageio_version = OIIO_PLUGIN_VERSION;
	OIIO_EXPORT ImageInput *raw_input_imageio_create () {
		return new RawInput;
	}
	OIIO_EXPORT const char *raw_input_extensions[] = {
        "bay", "bmq", "cr2", "crw", "cs1", "dc2", "dcr", "dng",
        "erf", "fff", "hdr", "k25", "kdc", "mdc", "mos", "mrw",
        "nef", "orf", "pef", "pxn", "raf", "raw", "rdc", "sr2",
        "srf", "x3f", "arw", "3fr", "cine", "ia", "kc2", "mef",
        "nrw", "qtk", "rw2", "sti", "rwl", "srw", "drf", "dsc",
        "ptx", "cap", "iiq", "rwz", NULL
	};

OIIO_PLUGIN_EXPORTS_END



bool
RawInput::open(const std::string &name, ImageSpec &newspec)
{
	int ret;

	// open the image
	if ( (ret = m_processor.open_file(name.c_str()) ) != LIBRAW_SUCCESS) {
		error ("Could not open file \"%s\", %s", name.c_str(), libraw_strerror(ret));
		return false;
	}

	if ( (ret = m_processor.unpack() ) != LIBRAW_SUCCESS) {
		error ("Could not unpack \"%s\", %s",name.c_str(), libraw_strerror(ret));
		return false;
	}

	// Set file information
	m_spec = ImageSpec(m_processor.imgdata.sizes.iwidth,
	                           m_processor.imgdata.sizes.iheight,
	                           3, // LibRaw should only give us 3 channels
	                           TypeDesc::UINT16);

	// Output 16 bit images
	m_processor.imgdata.params.output_bps = 16;


	// Set the gamma curve to Linear
	m_spec.attribute("oiio:ColorSpace","Linear");
	m_processor.imgdata.params.gamm[0] = 1.0;
	m_processor.imgdata.params.gamm[1] = 1.0;

	// Check to see if the user has explicitly set the output colorspace primaries
	ImageIOParameter *csp = newspec.find_attribute ("raw:ColorSpace", TypeDesc::STRING, true);
	if (csp) {
		std::string cs = *(const char**) csp->data();
		if (cs == "raw") {
			m_processor.imgdata.params.output_color = 0;
		}
		else if (cs == "sRGB") {
			m_processor.imgdata.params.output_color = 1;
		}
		else if (cs == "Adobe") {
			m_processor.imgdata.params.output_color = 2;
		}
		else if (cs == "Wide") {
			m_processor.imgdata.params.output_color = 3;
		}
		else if (cs == "ProPhoto") {
			m_processor.imgdata.params.output_color = 4;
		}
		else if (cs == "XYZ") {
			m_processor.imgdata.params.output_color = 5;
		}
		else {
			error("raw:ColorSpace set to unknown value");
			return false;
		}
		// Set the attribute in the output spec
		m_spec.attribute("raw:ColorSpace", cs);
	}
	else {
		// By default we use sRGB primaries for simplicity
		m_processor.imgdata.params.output_color = 1;
	}

	// Interpolation quality
	// note: LibRaw must be compiled with demosaic pack GPL2 to use
	// demosaic algorithms 5-9. It must be compiled with demosaic pack GPL3 for 
	// algorithm 10. If either of these packs are not includeded, it will silently use option 3 - AHD
	ImageIOParameter *dm = newspec.find_attribute ("raw:Demosaic", TypeDesc::STRING, true);
	if (dm) {
		std::string demosaic = *(const char**) dm->data();
		if (demosaic == "linear") {
			m_processor.imgdata.params.user_qual = 0;
		}
		else if (demosaic == "VNG") {
			m_processor.imgdata.params.user_qual = 1;
		}
		else if (demosaic == "PPG") {
			m_processor.imgdata.params.user_qual = 2;
		}
		else if (demosaic == "AHD") {
			m_processor.imgdata.params.user_qual = 3;
		}
		else if (demosaic == "DCB") {
			m_processor.imgdata.params.user_qual = 4;
		}
		else if (demosaic == "Modified AHD") {
			m_processor.imgdata.params.user_qual = 5;
		}
		else if (demosaic == "AFD") {
			m_processor.imgdata.params.user_qual = 6;
		}
		else if (demosaic == "VCD") {
			m_processor.imgdata.params.user_qual = 7;
		}
		else if (demosaic == "Mixed") {
			m_processor.imgdata.params.user_qual = 8;
		}
		else if (demosaic == "LMMSE") {
			m_processor.imgdata.params.user_qual = 9;
		}
		else if (demosaic == "AMaZE") {
			m_processor.imgdata.params.user_qual = 10;
		}
		else {
			error("raw:Demosaic set to unknown value");
			return false;
		}
		// Set the attribute in the output spec
		m_spec.attribute("raw:Demosaic", demosaic);
	}
	else {
		m_processor.imgdata.params.user_qual = 3;
	}


	// Exposure adjustment
	ImageIOParameter *ex = newspec.find_attribute ("raw:Exposure", TypeDesc::FLOAT, true);
	
	if (ex) {
		float exposure = *(float*)ex->data();
		
		if (exposure < 0.25f || exposure > 8.0f)
		{
			error("raw:Exposure invalid value. range 0.25f - 8.0f");
			return false;
		}
		
		m_processor.imgdata.params.exp_correc = 1; // enable exposure correction
		m_processor.imgdata.params.exp_shift = exposure; // set exposure correction

		// Set the attribute in the output spec
		m_spec.attribute("raw:Exposure", exposure);
	}

	newspec = m_spec;
	return true;
}


bool
RawInput::close()
{
	if (m_image) {
		LibRaw::dcraw_clear_mem(m_image);
		m_image = NULL;
	}
	return true;
}


bool
RawInput::process()
{
	if (!m_image) {
		int ret = m_processor.dcraw_process();
		
		if (ret != LIBRAW_SUCCESS) {
			error("Processing image failed, %s", libraw_strerror(ret));
			return false;
		}

		m_image = m_processor.dcraw_make_mem_image(&ret);

		if (!m_image) {
			error("LibRaw failed to create in memory image");
			return false;
		}

		if(m_image->type != LIBRAW_IMAGE_BITMAP) {
			error("LibRaw did not return expected image type");
			return false;
		}

		if(m_image->colors != 3) {
			error("LibRaw did not return 3 channel image");
			return false;
		}

	}
	return true;
}


bool
RawInput::read_native_scanline (int y, int z, void *data)
{
	if ( y < 0 || y >= m_spec.height) // out of range scanline
		return false;

	// Check the state of the internal RAW reader.
	// Have to load the entire image at once, so only do this once
	if (! m_image) { 
		if (!process()) {
			return false;
		}
	}

	int length = m_spec.width*m_image->colors; // Should always be 3 colors

	// Because we are reading UINT16's, we need to cast image->data
	unsigned short *scanline = &(((unsigned short *)m_image->data)[length*y]);
	memcpy(data, scanline, m_spec.scanline_bytes(true));

	return true;
}

OIIO_PLUGIN_NAMESPACE_END

