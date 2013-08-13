///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2003, Industrial Light & Magic, a division of Lucas
// Digital Ltd. LLC
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Industrial Light & Magic nor the names of
// its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission. 
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
//
//	PhotoRealistic RenderMan display driver that outputs
//	floating-point image files, using ILM's IlmImf library.
//
//	When you use this display driver for RGBA or Z output, you should
//	turn RGBA and Z quantization off by adding the following lines to
//	your RIB file:
//
//	    Quantize "rgba" 0 0 0 0
//	    Quantize "z"    0 0 0 0
//
//	Like Pixar's Tiff driver, this display driver can output image
//	channels other than R, G, B and A; for details on RIB file and
//	shader syntax, see the Renderman Release Notes (New Display
//	System, RGBAZ Output Images, Arbitrary Output Variables).
//
//	This driver maps Renderman's output variables to image channels
//	as follows:
//
//	Renderman output	image channel		image channel
//	variable name		name			type
//	--------------------------------------------------------------
//
//	"r"			"R"			HALF
//
//	"g"			"G"			HALF
//
//	"b"			"B"			HALF
//
//	"a"			"A"			HALF
//
//	"z"			"Z"			FLOAT
//
//	other			same as output		preferred type
//				variable name 		(see below)
//
//	By default, the "preferred" channel type is HALF; the
//	preferred type can be changed by adding an "exrpixeltype"
//	argument to the Display command in the RIB file.
//	For example:
//
//	    Declare "exrpixeltype" "string"
//
//	    # Store point positions in FLOAT format
//	    Display "gnome.points.exr" "exr" "P" "exrpixeltype" "float"
//
//	The default compression method for the image's pixel data
//	is defined in ImfHeader.h.  You can select a different
//	compression method by adding an "exrcompression" argument
//	to the Display command.  For example:
//
//	    Declare "exrcompression" "string"
//
//	    # Store RGBA using run-length encoding
//	    Display "gnome.rgba.exr" "exr" "rgba" "exrcompression" "rle"
//
//	See function DspyImageOpen(), below, for a list of valid
//	"exrpixeltype" and "exrcompression" values.
//
//-----------------------------------------------------------------------------

#undef NDEBUG // enable assert()

#include <ImfOutputFile.h>
#include <ImfChannelList.h>
#include <ImfIntAttribute.h>
#include <ImfFloatAttribute.h>
#include <ImfMatrixAttribute.h>
#include <ImfLut.h>
#include <ImfArray.h>
#include <ImathFun.h>
#include <Iex.h>
#include <half.h>
#include <halfFunction.h>
#include <string>
#include <map>
#include <vector>
#include <ndspy.h>

using namespace Imath;
using namespace Imf;
using namespace std;

namespace {

typedef map <string, int>   	    	ChannelOffsetMap;
typedef vector <halfFunction <half> *>	ChannelLuts;

//
// Define halfFunctions for the identity and piz12
//
half	    	    halfID( half x ) { return x; }

halfFunction <half> id( halfID );
halfFunction <half> piz12( round12log );

class Image
{
  public:

     Image (const char filename[],
	    const Header &header,
	    ChannelOffsetMap &rmanChannelOffsets,
	    int rmanPixelSize,
	    ChannelLuts &channelLuts);

    const Header &	header () const;

    void		writePixels (int xMin, int xMaxPlusone,
				     int yMin, int yMaxPlusone,
				     int entrySize,
				     const unsigned char *data);
  private:

    OutputFile		_file;
    Array <char> 	_buffer;
    vector <int>	_rmanChannelOffsets;
    vector <int>	_bufferChannelOffsets;
    int			_rmanPixelSize;
    int			_bufferPixelSize;
    int			_bufferXMin;
    int			_bufferNumPixels;
    int			_numPixelsReceived;
    ChannelLuts     	_channelLuts;
};


Image::Image (const char filename[],
	      const Header &header,
	      ChannelOffsetMap &rmanChannelOffsets,
	      int rmanPixelSize,
	      ChannelLuts &channelLuts
)
:
    _file (filename, header),
    _rmanPixelSize (rmanPixelSize),
    _bufferPixelSize (0),
    _bufferXMin (header.dataWindow().min.x),
    _bufferNumPixels (header.dataWindow().max.x - _bufferXMin + 1),
    _numPixelsReceived (0),
    _channelLuts (channelLuts)
{

    V2i dwSize = header.dataWindow().size();

    for (ChannelList::ConstIterator i = header.channels().begin();
	 i != header.channels().end();
	 ++i)
    {
	switch (i.channel().type)
	{
	  case HALF:

	    _rmanChannelOffsets.push_back (rmanChannelOffsets[i.name()]);
	    _bufferChannelOffsets.push_back (_bufferPixelSize);
	    _bufferPixelSize += sizeof (float); // Note: to avoid alignment
	    break;				// problems when float and half
						// channels are mixed, halfs
	  case FLOAT:				// are not packed densely.

	    _rmanChannelOffsets.push_back (rmanChannelOffsets[i.name()]);
	    _bufferChannelOffsets.push_back (_bufferPixelSize);
	    _bufferPixelSize += sizeof (float);
	    break;

	  default:

	    assert (false);  			// unsupported channel type
	    break;
	}
    }

    _buffer.resizeErase (_bufferNumPixels * _bufferPixelSize);

    FrameBuffer  fb;
    int     	 j = 0;
    int     	 yStride = 0;
    char    	*base = &_buffer[0] -
    	    	    	 _bufferXMin * _bufferPixelSize;


    for (ChannelList::ConstIterator i = header.channels().begin();
	 i != header.channels().end();
	 ++i)
    {
	fb.insert (i.name(),
		   Slice (i.channel().type,			// type
			  base + _bufferChannelOffsets[j],  	// base
			  _bufferPixelSize,			// xStride
			  yStride,	    	    	    	// yStride
			  1,					// xSampling
			  1));					// ySampling
	++j;
    }

    _file.setFrameBuffer (fb);
}


const Header &
Image::header () const
{
    return _file.header();
}


void
Image::writePixels (int xMin, int xMaxPlusone,
		    int yMin, int yMaxPlusone,
		    int entrySize,
	 	    const unsigned char *data)
{
    //
    // We can only deal with one scan line at a time.
    //

    assert (yMin == yMaxPlusone - 1);

    const ChannelList &channels = _file.header().channels();
    int      numPixels = xMaxPlusone - xMin;
    int      j = 0;

    char    *toBase;
    int      toInc;		  

    //
    // Copy the pixels into our internal one-line frame buffer.
    //

    toBase = _buffer + _bufferPixelSize * xMin;
    toInc = _bufferPixelSize;		  

    for (ChannelList::ConstIterator i = channels.begin();
	 i != channels.end();
	 ++i)
    {
	const unsigned char *from = data + _rmanChannelOffsets[j];
	const unsigned char *end  = from + numPixels * entrySize;

	char *to = toBase + _bufferChannelOffsets[j];

	switch (i.channel().type)
	{
	  case HALF:
    	  {
    	    halfFunction <half> &lut = *_channelLuts[j];

	    while (from < end)
	    {
		*(half *) to = lut( ( half )( *(float *) from ) );
		from += entrySize;
		to += toInc;
	    }

	    break;
    	  }

	  case FLOAT:

	    while (from < end)
	    {
		*(float *) to = *(float *) from;
		from += entrySize;
		to += toInc;
	    }

	    break;

	  default:

	    assert (false);  // channel type is not currently supported
	    break;
	}

	++j;
    }

    _numPixelsReceived += numPixels;
    assert (_numPixelsReceived <= _bufferNumPixels);

    if (_numPixelsReceived == _bufferNumPixels)
    {
	//
	// If our one-line frame buffer is full, then write it to
	// the output file.
	//

	_file.writePixels();
	_numPixelsReceived = 0;
    }
}


} // namespace

extern "C" {


PtDspyError
DspyImageOpen (PtDspyImageHandle *pvImage,
	       const char *drivername,
	       const char *filename,
	       int width,
	       int height,
	       int paramCount,
	       const UserParameter *parameters,
	       int formatCount,
	       PtDspyDevFormat *format,
	       PtFlagStuff *flagstuff)
{
    try
    {
	//
	// Build an output file header
	//

	Header	    	     header;
	ChannelOffsetMap     channelOffsets;
    	ChannelLuts 	     channelLuts;
	int 	    	     pixelSize = 0;

    	halfFunction <half> *rgbLUT = &id;
    	halfFunction <half> *otherLUT = &id;


	//
	// Data window
	//

	{
	    Box2i &dw = header.dataWindow();
	    int n = 2;

	    DspyFindIntsInParamList ("origin", &n, &dw.min.x,
				     paramCount, parameters);
	    assert (n == 2);

	    dw.max.x = dw.min.x + width  - 1;
	    dw.max.y = dw.min.y + height - 1;
	}

	//
	// Display window
	//

	{
	    Box2i &dw = header.displayWindow();
	    int n = 2;

	    DspyFindIntsInParamList ("OriginalSize", &n, &dw.max.x,
				     paramCount, parameters);
	    assert (n == 2);

	    dw.min.x  = 0;
	    dw.min.y  = 0;
	    dw.max.x -= 1;
	    dw.max.y -= 1;
	}

	//
	// Camera parameters
	//

	{
	    //
	    // World-to-NDC matrix, world-to-camera matrix,
	    // near and far clipping plane distances
	    //

	    M44f NP, Nl;
	    float near = 0, far = 0;

	    DspyFindMatrixInParamList ("NP", &NP[0][0], paramCount, parameters);
	    DspyFindMatrixInParamList ("Nl", &Nl[0][0], paramCount, parameters);
	    DspyFindFloatInParamList ("near", &near, paramCount, parameters);
	    DspyFindFloatInParamList ("far", &far, paramCount, parameters);

    	    //
	    // The matrices reflect the orientation of the camera at
	    // render time.
	    //

	    header.insert ("worldToNDC", M44fAttribute (NP));
	    header.insert ("worldToCamera", M44fAttribute (Nl));
	    header.insert ("clipNear", FloatAttribute (near));
	    header.insert ("clipFar", FloatAttribute (far));

	    //
	    // Projection matrix
	    //

	    M44f P = Nl.inverse() * NP;

	    //
	    // Derive pixel aspect ratio, screen window width, screen
	    // window center from projection matrix.
	    //

	    Box2f sw (V2f ((-1 - P[3][0] - P[2][0]) / P[0][0],
			   (-1 - P[3][1] - P[2][1]) / P[1][1]),
		      V2f (( 1 - P[3][0] - P[2][0]) / P[0][0],
			   ( 1 - P[3][1] - P[2][1]) / P[1][1]));

	    header.screenWindowWidth() = sw.max.x - sw.min.x;
	    header.screenWindowCenter() = (sw.max + sw.min) / 2;

	    const Box2i &dw = header.displayWindow();

	    header.pixelAspectRatio()   = (sw.max.x - sw.min.x) /
					  (sw.max.y - sw.min.y) *
					  (dw.max.y - dw.min.y + 1) /
					  (dw.max.x - dw.min.x + 1);
	}

	//
	// Line order
	//

	header.lineOrder() = INCREASING_Y;
	flagstuff->flags |= PkDspyFlagsWantsScanLineOrder;

	//
	// Compression
	//

	{
	    char *comp = 0;

	    DspyFindStringInParamList ("exrcompression", &comp,
				       paramCount, parameters);

	    if (comp)
	    {
		if (!strcmp (comp, "none"))
		    header.compression() = NO_COMPRESSION;
		else if (!strcmp (comp, "rle"))
		    header.compression() = RLE_COMPRESSION;
		else if (!strcmp (comp, "zips"))
		    header.compression() = ZIPS_COMPRESSION;
		else if (!strcmp (comp, "zip"))
		    header.compression() = ZIP_COMPRESSION;
		else if (!strcmp (comp, "piz"))
		    header.compression() = PIZ_COMPRESSION;

		else if (!strcmp (comp, "piz12"))
		{
		    header.compression() = PIZ_COMPRESSION;
    	    	    rgbLUT = &piz12;
    	    	}

		else
		    THROW (Iex::ArgExc,
			   "Invalid exrcompression \"" << comp << "\" "
			   "for image file " << filename << ".");
	    }
	}

	//
	// Channel list
	//

	{
	    PixelType pixelType = HALF;
	    char *ptype = 0;

	    DspyFindStringInParamList ("exrpixeltype", &ptype,
				       paramCount, parameters);

	    if (ptype)
	    {
		if (!strcmp (ptype, "float"))
		    pixelType = FLOAT;
		else if (!strcmp (ptype, "half"))
		    pixelType = HALF;
		else
		    THROW (Iex::ArgExc,
			   "Invalid exrpixeltype \"" << ptype << "\" "
			   "for image file " << filename << ".");
	    }

	    ChannelList &channels = header.channels();

	    for (int i = 0; i < formatCount; ++i)
	    {
		if      (!strcmp (format[i].name, "r"))
		{
		    channels.insert ("R", Channel (HALF));
		    channelOffsets["R"] = pixelSize;
    	    	    channelLuts.push_back( rgbLUT );
		}
		else if (!strcmp (format[i].name, "g"))
		{
		    channels.insert ("G", Channel (HALF));
		    channelOffsets["G"] = pixelSize;
    	    	    channelLuts.push_back( rgbLUT );
		}
		else if (!strcmp (format[i].name, "b"))
		{
		    channels.insert ("B", Channel (HALF));
		    channelOffsets["B"] = pixelSize;
    	    	    channelLuts.push_back( rgbLUT );
		}
		else if (!strcmp (format[i].name, "a"))
		{
		    channels.insert ("A", Channel (HALF));
		    channelOffsets["A"] = pixelSize;
    	    	    channelLuts.push_back( otherLUT );
		}
		else if (!strcmp (format[i].name, "z"))
		{
		    channels.insert ("Z", Channel (FLOAT));
		    channelOffsets["Z"] = pixelSize;
    	    	    channelLuts.push_back( otherLUT );
		}
		else
		{
		    //
		    // Unknown channel name; keep its name and store
		    // the channel, unless the name conflicts with
		    // another channel's name.
		    //

		    if (!channels.findChannel (format[i].name))
		    {
			channels.insert (format[i].name, Channel (pixelType));
			channelOffsets[format[i].name] = pixelSize;
    	    		channelLuts.push_back( otherLUT );
		    }
		}

		format[i].type = PkDspyFloat32 | PkDspyByteOrderNative;
		pixelSize += sizeof (float);
	    }
	}

	//
	// Open the output file
	//

	Image *image = new Image (filename,
				  header,
				  channelOffsets,
				  pixelSize,
				  channelLuts);

	*pvImage = (PtDspyImageHandle) image;
    }
    catch (const exception &e)
    {
	DspyError ("OpenEXR display driver", "%s\n", e.what());
	return PkDspyErrorUndefined;
    }

    return PkDspyErrorNone;
}


PtDspyError
DspyImageData (PtDspyImageHandle pvImage,
	       int xmin,
	       int xmax_plusone,
	       int ymin,
	       int ymax_plusone,
	       int entrysize,
	       const unsigned char *data)
{
    try
    {
	Image *image = (Image *) pvImage;

	image->writePixels (xmin, xmax_plusone,
			    ymin, ymax_plusone,
			    entrysize, data);
    }
    catch (const exception &e)
    {
	DspyError ("OpenEXR display driver", "%s\n", e.what());
	return PkDspyErrorUndefined;
    }

    return PkDspyErrorNone;
}


PtDspyError
DspyImageClose (PtDspyImageHandle pvImage)
{
    try
    {
	delete (Image *) pvImage;
    }
    catch (const exception &e)
    {
	DspyError ("OpenEXR display driver", "%s\n", e.what());
	return PkDspyErrorUndefined;
    }

    return PkDspyErrorNone;
}


PtDspyError
DspyImageQuery (PtDspyImageHandle pvImage,
		PtDspyQueryType querytype,
		int datalen,
		void *data)
{
    if (datalen > 0 && data)
    {
	switch (querytype)
	{
	  case PkOverwriteQuery:
	    {
		PtDspyOverwriteInfo overwriteInfo;

		if (datalen > sizeof(overwriteInfo))
		    datalen = sizeof(overwriteInfo);

		overwriteInfo.overwrite = 1;
		overwriteInfo.interactive = 0;

		memcpy(data, &overwriteInfo, datalen);
	    }
	    break;

	  case PkSizeQuery:
	    {
		PtDspySizeInfo sizeInfo; 

		if (datalen > sizeof(sizeInfo))
		    datalen = sizeof(sizeInfo);

		const Image *image = (const Image *) pvImage;

		if (image)
		{
		    const Box2i &dw = image->header().dataWindow();

		    sizeInfo.width  = dw.max.x - dw.min.x + 1;
		    sizeInfo.height = dw.max.y - dw.min.y + 1;

		    //
		    // Renderman documentation does not specify if
		    // sizeInfo.aspectRatio refers to the pixel or
		    // the image aspect ratio, but sample code in
		    // the documentation suggests pixel aspect ratio.
		    //

		    sizeInfo.aspectRatio = image->header().pixelAspectRatio();
		}
		else
		{
		    sizeInfo.width  = 640;
		    sizeInfo.height = 480;
		    sizeInfo.aspectRatio = 1.0f;
		}

		memcpy(data, &sizeInfo, datalen);
	    }
	    break;

	  default:

	    return PkDspyErrorUnsupported;
	}
    }
    else
    {
	return PkDspyErrorBadParams;
    }

    return PkDspyErrorNone;
}


} // extern C
