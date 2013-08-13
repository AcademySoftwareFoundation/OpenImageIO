// ===========================================================================
//	ExrFormatGlobals.cp			Part of OpenEXR
// ===========================================================================
//
//	Structure in which the EXRFormat plug-in stores its state
//

#if MSWindows
#pragma warning (disable: 161)
#endif

#include "EXRFormatGlobals.h"


//-------------------------------------------------------------------------------
//	Globals
//-------------------------------------------------------------------------------

int		gExr_MaxPixelValue		=	0xFF;
int		gExr_MaxPixelDepth		=	8;



//-------------------------------------------------------------------------------
//	Reset
//-------------------------------------------------------------------------------

void EXRFormatGlobals::Reset ()
{
	inputFile		=	NULL;
	inputStream		=	NULL;
	
	DefaultIOSettings();
}


//-------------------------------------------------------------------------------
//	DefaultIOSettings
//-------------------------------------------------------------------------------

void EXRFormatGlobals::DefaultIOSettings ()
{
    exposure            =   0;
    gamma               =   2.2;
    bpc                 =   (gExr_MaxPixelDepth == 8) ? 8 : 16;
	premult             =   true;
	
	outputChannels		=	Imf::WRITE_RGBA;
	outputLineOrder		=	Imf::DECREASING_Y;
	outputCompression	=	Imf::PIZ_COMPRESSION;
}

