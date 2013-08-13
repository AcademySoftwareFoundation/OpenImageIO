// ===========================================================================
//	ExrFormatGlobals.h			Part of OpenEXR
// ===========================================================================
//
//	Structure in which the EXRFormat plug-in stores its state
//

#pragma once

#include "PSFormatGlobals.h"

#include <ImfRgbaFile.h>
#include <ImfLineOrder.h>
#include <ImfCompression.h>

class RefNumIFStream;


//-------------------------------------------------------------------------------
//	Limits
//-------------------------------------------------------------------------------

const int		kExr_MaxPixelValue_8	=	0xFF;
const int		kExr_MaxPixelValue_16	=	0xFFFF;


//-------------------------------------------------------------------------------
//	Configurable Limits
//-------------------------------------------------------------------------------
//
//	these are globals so they can be changed based on the capabilities
//	of the host.  For example, Commotion has a max pixel depth of 8 and
//	a max pixel value of 0xFF.  Photoshop has a max pixel depth of 16, but
//	only a max pixel value of 0x8000.  Other hosts might support the full
//	16-bit range up to 0xFFFF (combustion?)
//

extern int		gExr_MaxPixelValue;
extern int		gExr_MaxPixelDepth;


//-------------------------------------------------------------------------------
//	Globals struct
//-------------------------------------------------------------------------------

class EXRFormatGlobals : public PSFormatGlobals
{
public:

	void					Reset					();
	void					DefaultIOSettings		();

	int						MaxPixelValue			() const;

	Imf::RgbaInputFile*		inputFile;
	RefNumIFStream*			inputStream;
	
	int						bpc;
	double					exposure;
    double                  gamma;
    bool                    premult;

	Imf::RgbaChannels		outputChannels;
	Imf::LineOrder			outputLineOrder;
	Imf::Compression		outputCompression;
};

typedef EXRFormatGlobals*		GPtr;


//-------------------------------------------------------------------------------
//	MaxPixelValue
//-------------------------------------------------------------------------------

inline int EXRFormatGlobals::MaxPixelValue () const
{
	if (bpc == 16)
		return (kExr_MaxPixelValue_16 < gExr_MaxPixelValue) ? kExr_MaxPixelValue_16 : gExr_MaxPixelValue;

	return (kExr_MaxPixelValue_8 < gExr_MaxPixelValue) ? kExr_MaxPixelValue_8 : gExr_MaxPixelValue;
}


