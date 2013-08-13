// ===========================================================================
//	EXRFormatPlugin.cp			Part of OpenEXR
// ===========================================================================

#if MSWindows
#pragma warning (disable: 161)
#endif

#include "EXRFormatPlugin.h"

#include "EXRResample.h"

#include "EXRImportDialog.h"
#include "EXRExportDialog.h"
#include "RefNumIO.h"

#include "PIFormat.h"
#include "PSAutoBuffer.h"

#include "ImfFrameBuffer.h" 
#include "ImfRgbaFile.h" 
#include "ImathBox.h" 
#include "ImfArray.h" 
#include "ImfIO.h"
#include "ImathFun.h"

#include "PITypes.h"
#include "ADMBasic.h"

#include <cassert>



//-------------------------------------------------------------------------------
//	ConfigureLimits
//-------------------------------------------------------------------------------

static void ConfigureLimits (const FormatRecord* inFormatRec)
{
	// set gExr_MaxPixelValue and gExr_MaxPixelDepth here

	if (inFormatRec != NULL)
	{
		if (inFormatRec->maxValue >= 0xFFFF)
		{
			// host supports 16 bit pixels
		
			gExr_MaxPixelDepth	=	16;
			gExr_MaxPixelValue	=	0x8000;
		}
		else
		{
			// host only supports 8 bit pixels.
		
			gExr_MaxPixelDepth	=	8;
			gExr_MaxPixelValue	=	0x00FF;
		}
	}
	
}

#pragma mark-

//-------------------------------------------------------------------------------
//	EXRFormatPlugin
//-------------------------------------------------------------------------------

EXRFormatPlugin::EXRFormatPlugin ()
{
	// nothing to do
}


//-------------------------------------------------------------------------------
//	~EXRFormatPlugin
//-------------------------------------------------------------------------------

EXRFormatPlugin::~EXRFormatPlugin ()
{
	// nothing to do
}

#pragma mark-

//-------------------------------------------------------------------------------
//	GlobalsSize
//-------------------------------------------------------------------------------

int EXRFormatPlugin::GlobalsSize ()
{
	return sizeof (EXRFormatGlobals);
}


//-------------------------------------------------------------------------------
//	InitGlobals
//-------------------------------------------------------------------------------

void EXRFormatPlugin::InitGlobals ()
{
	Globals()->Reset();
}

#pragma mark-

//-------------------------------------------------------------------------------
//	DoAbout
//-------------------------------------------------------------------------------

void EXRFormatPlugin::DoAbout (AboutRecord* inAboutRec)
{
    if (inAboutRec != NULL && inAboutRec->sSPBasic != NULL)
    {
        ADMBasicSuite6* basicSuite = NULL;
        
        inAboutRec->sSPBasic->AcquireSuite (kADMBasicSuite, kADMBasicSuiteVersion6, (void**) &basicSuite);

        if (basicSuite != NULL)
        {                      
            basicSuite->MessageAlert ("OpenEXR Format v1.1.1\n\n"
                                      "Format by Florian Kainz, Rod Bogart, Josh Pines, and Drew Hess\n"
                                      "Plug-in by Paul Schneider\n"
                                      "www.openexr.com");

            inAboutRec->sSPBasic->ReleaseSuite (kADMBasicSuite, kADMBasicSuiteVersion6);
            basicSuite = NULL;
        }
    }
}

#pragma mark-

//-------------------------------------------------------------------------------
//	DoReadStart
//-------------------------------------------------------------------------------

void EXRFormatPlugin::DoReadStart ()
{
	using namespace Imf;
	using namespace Imath;


	// construct input file from refnum
	
	assert( Globals()->inputStream == NULL );
	assert( Globals()->inputFile   == NULL );
	
	Globals()->inputStream 	= new RefNumIFStream (mFormatRec->dataFork, "EXR File");
	Globals()->inputFile 	= new RgbaInputFile (* (Globals()->inputStream));
	
	
	// get dimension info
	
	const Box2i& dw = Globals()->inputFile->dataWindow();
	
	int w = dw.max.x - dw.min.x + 1;
	int h = dw.max.y - dw.min.y + 1;
	int dx = dw.min.x;
	int dy = dw.min.y;
	

	// get resampling configuration	
	// pop up dialog, ask user for config
	// don't display dialog if running in a host other than Photoshop,
	// for partial After Effects compatibility

	if (mFormatRec->hostSig == '8BIM' || mFormatRec->hostSig == 'MIB8')
	{
		if (!DoImportPreviewDlog())
		{
			// user hit cancel.  clean up (some hosts like AE won't
			// call us with the ReadFinish selector in this case)
			// and return a user canceled error to the host.
		
			DoReadFinish();
		
			*mResult = userCanceledErr;

			return;
		}
	}
		
	
	// we need this here, because the table is initialized to 
	// 8-bit (preview) mode.

	ResetHalfToIntTable (Globals());


	// return image info to host
	// always do interleaved RGB or RGBA for now
	
	// if image is RGB, don't add an alpha channel
	// if image is single channel, add all four channels
	// so that we don't have to switch to grayscale mode
		
	mFormatRec->imageSize.v 	= h;
	mFormatRec->imageSize.h 	= w;
	mFormatRec->planes 			= (Globals()->inputFile->channels() == WRITE_RGB) ? 3 : 4;
	mFormatRec->depth 			= (Globals()->bpc == 8) ? 8 : 16;
	mFormatRec->imageMode		= (mFormatRec->depth > 8) ? plugInModeRGB48 : plugInModeRGBColor;
	mFormatRec->maxValue		= Globals()->MaxPixelValue();
}


//-------------------------------------------------------------------------------
//	DoReadContinue
//-------------------------------------------------------------------------------

void EXRFormatPlugin::DoReadContinue ()
{
	using namespace Imf;
	using namespace Imath;
	
	int				rowBytes;
	int				done, total;
	bool			haveAlpha;
	bool            premult;

	
	// sanity check

	if (Globals()->inputFile == NULL)
	{
		*mResult = formatCannotRead;
		return;
	}
	
	
	// get channel info
	
	haveAlpha = !(Globals()->inputFile->channels() == WRITE_RGB);
    premult   = Globals()->premult;
	

	// get dimension info
	
	const Box2i& dw = Globals()->inputFile->dataWindow();
	
	int w 	= dw.max.x - dw.min.x + 1;
	int h 	= dw.max.y - dw.min.y + 1;
	int dx 	= dw.min.x;
	int dy 	= dw.min.y;


	// prepare for progress reporting
	
	done 	= 0;
	total	= h;
	

	// get rowbytes, and add alignment padding

	rowBytes = (mFormatRec->imageSize.h * mFormatRec->planes * (mFormatRec->depth / 8));
	rowBytes = (rowBytes * mFormatRec->depth + 7) >> 3;


	// create a half buffer for reading into
	// buffer is big enough for one scanline
	
	Array2D<Rgba> p2 (1, w);
	

	// create an integer buffer for returning pixels to the host
	// buffer is big enough for one scanline
	
	PSAutoBuffer intBuffer (rowBytes, mFormatRec->bufferProcs);
	
	mFormatRec->data	 	= intBuffer.Lock();
	unsigned char* data8 	= (unsigned char*) mFormatRec->data;
	unsigned short* data16	= (unsigned short*) mFormatRec->data;


	// Set up to start returning chunks of data. 
	// use RGBA interleaved format

	mFormatRec->colBytes 		= mFormatRec->planes * (mFormatRec->depth / 8);
	mFormatRec->rowBytes 		= rowBytes;
	mFormatRec->planeBytes 		= mFormatRec->depth / 8;
	mFormatRec->loPlane			= 0;
	mFormatRec->hiPlane			= 3;
	mFormatRec->theRect.left	= 0;
	mFormatRec->theRect.right 	= mFormatRec->imageSize.h;

	
	// read one scanline at a time
	
	for (int scanline = dw.min.y; scanline <= dw.max.y && *mResult == noErr; ++scanline)
	{	
		// read scanline
		// need to offset into array so that scanline
		// starts at the front of the array
	
		Globals()->inputFile->setFrameBuffer (&p2[-scanline][-dx], 1, w);
		Globals()->inputFile->readPixels (scanline);
		
		
		// unmult scanline if necessary
		
		if (premult)
		{
		    for (int x = 0; x < w; ++x)
		    {
		        // we're going to throw away any alpha data > 1, so 
                // clamp it to that range before using it for unmulting
                
                float a = p2[0][x].a;
                a = Imath::clamp (a, 0.f, 1.f); 
		    
		        if (a != 0)
		        {
		            p2[0][x].r /= a;
		            p2[0][x].g /= a;
		            p2[0][x].b /= a;
		        }
		    }
		}
		
		
		// convert scanline
		
		int i = 0;
		
		if (mFormatRec->depth > 8)
		{
			for (int x = 0; x < w; ++x)
			{
				data16[i] = HalfToInt (p2[0][x].r, 0);		i++;
				data16[i] = HalfToInt (p2[0][x].g, 1);		i++;
				data16[i] = HalfToInt (p2[0][x].b, 2);		i++;
				
				if (haveAlpha)
				{
					data16[i] = HalfToInt (p2[0][x].a, 3); 		
					i++;
				}
			}
		}
		else
		{
			for (int x = 0; x < w; ++x)
			{
				data8[i] = HalfToInt (p2[0][x].r, 0);		i++;
				data8[i] = HalfToInt (p2[0][x].g, 1);		i++;
				data8[i] = HalfToInt (p2[0][x].b, 2);		i++;
				
				if (haveAlpha)
				{
					data8[i] = HalfToInt (p2[0][x].a, 3); 		
					i++;
				}
			}
		}
		
		
		// pass scanline back to host
		// offset data window to origin, because pshop doesn't have "data window" concept
		
		mFormatRec->theRect.top		= scanline - dw.min.y;
		mFormatRec->theRect.bottom	= mFormatRec->theRect.top + 1;
		*mResult 					= mFormatRec->advanceState();
		
		
		// report progress
		
		mFormatRec->progressProc (++done, total);
	}
	
	
	// we are done
			
	mFormatRec->data = NULL;

}


//-------------------------------------------------------------------------------
//	DoReadFinish
//-------------------------------------------------------------------------------

void EXRFormatPlugin::DoReadFinish ()
{
	// clean up Globals()

	delete Globals()->inputFile;
	Globals()->inputFile = NULL;
	
	delete Globals()->inputStream;
	Globals()->inputStream = NULL;
}

#pragma mark-

//-------------------------------------------------------------------------------
//	DoOptionsStart
//-------------------------------------------------------------------------------

void EXRFormatPlugin::DoOptionsStart ()
{
	// show options dialog

	if (DoExportSettingsDlog ())
	{
		// user configured options, so set revert info up so that it reflects
		// the new options.  Commotion, in particular, uses this so it doesn't
		// bring the options dialog up when saving each frame of a sequence.
	
		if (mFormatRec->revertInfo == NULL)
		{
			mFormatRec->revertInfo = mFormatRec->handleProcs->newProc (GlobalsSize());
		}
			
		if (mFormatRec->revertInfo != NULL)
		{
			char* ptr = mFormatRec->handleProcs->lockProc (mFormatRec->revertInfo, false);
			memcpy (ptr, Globals(), GlobalsSize());
			mFormatRec->handleProcs->unlockProc (mFormatRec->revertInfo);
		}	
	}
	else
	{
		// user canceled out of options dialog
	
		*mResult = userCanceledErr;
	}
}

#pragma mark-

//-------------------------------------------------------------------------------
//	DoEstimateStart
//-------------------------------------------------------------------------------

void EXRFormatPlugin::DoEstimateStart ()
{
	// provide an estimate as to how much disk space
	// we need to write the file.  If we don't set a 
	// non-zero size, Photoshop won't open the file!
	// Thanks to Chris Cox @ Adobe for this fix.

	int32 dataBytes;


	// minimum file size estimate is just the header

	mFormatRec->minDataBytes = 100;


	// estimate maximum file size if it were uncompressed
	// header plus 4 channels, 2 bytes per channel

	dataBytes = 100 +
			4 * 2 * (int32) mFormatRec->imageSize.h * mFormatRec->imageSize.v;

	mFormatRec->maxDataBytes = dataBytes;

		
	// tell the host not to call us with DoEstimateContinue
	
	mFormatRec->data = NULL;
}

#pragma mark-

//-------------------------------------------------------------------------------
//	DoWriteStart
//-------------------------------------------------------------------------------

void EXRFormatPlugin::DoWriteStart ()
{
	using namespace 	Imf;
	using namespace 	Imath;
	
	int					done, total;
	unsigned char* 		pix8;
	unsigned short*		pix16;	


	
	// set globals to reflect the pixels we're recieving, and
	// rebuild the LUT to convert integer pixels 
	// to floating point pixels
	
	Globals()->bpc = mFormatRec->depth;
	ResetIntToHalfTable (Globals());
			

	// construct output file from file spec
	
	Header				header	(mFormatRec->imageSize.h, 
								 mFormatRec->imageSize.v,
								 1,
								 Imath::V2f (0, 0),
								 1,
								 Imf::INCREASING_Y,
								 Globals()->outputCompression);

	RefNumOFStream		stream	(mFormatRec->dataFork, "EXR File");
	RgbaOutputFile		out		(stream, header, (mFormatRec->planes == 3) ? WRITE_RGB : WRITE_RGBA);	


	// tell the host what format we want to recieve pixels in
	// interleaved RGBA format is always popular
	// note that we don't align the rowbytes in this case
	
	mFormatRec->imageMode			= (mFormatRec->depth > 8) ? plugInModeRGB48 : plugInModeRGBColor;
	mFormatRec->loPlane				= 0;
	mFormatRec->hiPlane				= mFormatRec->planes - 1;
	mFormatRec->planeBytes			= mFormatRec->depth / 8;
	mFormatRec->colBytes			= mFormatRec->planeBytes * mFormatRec->planes;
	mFormatRec->rowBytes			= mFormatRec->colBytes * mFormatRec->imageSize.h;
	mFormatRec->theRect.left		= 0;
	mFormatRec->theRect.right		= mFormatRec->imageSize.h;
	
	
	// set up progress
	
	done 	=	0;
	total	=	mFormatRec->imageSize.v;
	

	// create buffers for one scanline
	
	PSAutoBuffer	intBuffer 	(mFormatRec->rowBytes, mFormatRec->bufferProcs);
	Array2D<Rgba>	p2			(1, mFormatRec->imageSize.h);
	
	
	// tell host where our buffer is
	
	mFormatRec->data = intBuffer.Lock();		
	
	
	// convert one scanline at a time
	
	for (int y = 0; y < mFormatRec->imageSize.v; ++y)
	{
		// get one scanline from host
		
		mFormatRec->theRect.top		 = y;
		mFormatRec->theRect.bottom	 = y+1;
	
		mFormatRec->advanceState();
		
		
		// convert scanline

		if (mFormatRec->depth > 8)
		{
			pix16 = (unsigned short*) (mFormatRec->data);
			
			for (int x = 0; x < mFormatRec->imageSize.h; ++x)
			{
				p2[0][x].r	=	IntToHalf	(*pix16++, 0);
				p2[0][x].g	=	IntToHalf	(*pix16++, 1);
				p2[0][x].b	=	IntToHalf	(*pix16++, 2);
				p2[0][x].a	=	(mFormatRec->planes > 3) ? IntToHalf (*pix16++, 3) : half(1.0);			
			}
		}
		else
		{
			pix8  = (unsigned char*)  (mFormatRec->data);
		
			for (int x = 0; x < mFormatRec->imageSize.h; ++x)
			{
				p2[0][x].r	=	IntToHalf	(*pix8++, 0);
				p2[0][x].g	=	IntToHalf	(*pix8++, 1);
				p2[0][x].b	=	IntToHalf	(*pix8++, 2);
				p2[0][x].a	=	(mFormatRec->planes > 3) ? IntToHalf (*pix8++, 3) : half(1.0);	
			}
		}
		
		
		// premult if necessary
			
		if (Globals()->premult)
		{
		    for (int x = 0; x < mFormatRec->imageSize.h; ++x)
		    {
		        half a = p2[0][x].a;
		        p2[0][x].r *= a;
		        p2[0][x].g *= a;
		        p2[0][x].b *= a;
		    }
		}
			
			
		// write scanline
		
		out.setFrameBuffer (&p2[-y][0], 1, mFormatRec->imageSize.h);
		out.writePixels (1);
		
		
		// report progress
		
		mFormatRec->progressProc (++done, total);
	}
	

	// we are done
	
	mFormatRec->data = NULL;
}

#pragma mark-

//-------------------------------------------------------------------------------
//	DoImportPreviewDlog
//-------------------------------------------------------------------------------

bool EXRFormatPlugin::DoImportPreviewDlog ()
{
    return EXRImportDialog (Globals(), mFormatRec->sSPBasic, mFormatRec->plugInRef);
}


//-------------------------------------------------------------------------------
//	DoExportSettingsDlog
//-------------------------------------------------------------------------------

bool EXRFormatPlugin::DoExportSettingsDlog ()
{
    return EXRExportDialog (Globals(), mFormatRec->sSPBasic, mFormatRec->plugInRef);
}

#pragma mark-

//-------------------------------------------------------------------------------
//	Main entry point
//-------------------------------------------------------------------------------

DLLExport MACPASCAL 
void PluginMain
(
	const short 		inSelector,
	FormatRecord*		inFormatRecord,
	long*				inData,
	short*				outResult
)
{

	// configure resampling based on host's capabilities
	
	if (inSelector != formatSelectorAbout)
	{
		ConfigureLimits (inFormatRecord);
	}
	

	// create and run the plug-in

	try
	{
		EXRFormatPlugin		plugin;
			
		plugin.Run (inSelector, inFormatRecord, inData, outResult);
	}
	
	
	// catch out-of-memory exception
	
	catch (const std::bad_alloc&)
	{
		*outResult = memFullErr;
	}
	
	
	// catch an exception that provides an error string
	
	catch (const std::exception& ex)
	{
		if (inSelector == formatSelectorAbout || ex.what() == NULL)
		{
			*outResult = formatCannotRead;
		}
		else
		{
			memcpy (& (*inFormatRecord->errorString)[1], ex.what(), strlen (ex.what()));
			(*inFormatRecord->errorString)[0] = strlen (ex.what());
			*outResult = errReportString;
		}
	}
	
	
	// catch any other exception (we don't want to throw back into the host)
	
	catch (...)
	{
		*outResult = formatCannotRead;
	}

}

