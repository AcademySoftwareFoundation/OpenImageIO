// ===========================================================================
//	EXRImportDialog.cpp           			Part of OpenEXR
// ===========================================================================

#if MSWindows
#pragma warning (disable: 161)
#endif

#include "EXRImportDialog.h"

#include "EXRResample.h"

#include "ADMDialog.h"
#include "ADMItem.h"
#include "ADMDrawer.h"
#include "ADMImage.h"
#include "ADMTracker.h"
#include "ADMResource.h"

#include <ImfArray.h>
#include <ImfRgbaFile.h>
#include <ImathFun.h>

#include "PITypes.h"

#if Macintosh
#include <Appearance.h>
using std::min;
using std::max;
#endif


using Imf::Array2D;
using Imf::Rgba;


// ---------------------------------------------------------------------------
//	Resource IDs
// ---------------------------------------------------------------------------

enum
{
    kItem_OK                =	1,
    kItem_Cancel,
    kItem_Defaults,
    kItem_Sep1,
    kItem_ExposureLabel,
    kItem_Exposure,
    kItem_GammaLabel,
    kItem_Gamma,   
    kItem_Unmult,
    kItem_Sep2,
    kItem_Preview
};

// ---------------------------------------------------------------------------
//	Globals
// ---------------------------------------------------------------------------

static ADMDialogSuite5*     sDlogSuite              =	NULL;
static ADMItemSuite5*       sItemSuite              =	NULL;
static ADMDrawerSuite3*     sDrawSuite              =   NULL;
static ADMImageSuite2*      sImageSuite             =   NULL;
static ADMTrackerSuite1*    sTrackSuite             =   NULL;

static Array2D< Rgba >		sEXRBuffer;
static ADMImageRef          sPreviewImage           =   NULL;


// ---------------------------------------------------------------------------
//	round
// ---------------------------------------------------------------------------

#if MSWindows

inline double round (double d)
{
	return (double) (int) (d + .5);
}

#endif

// ---------------------------------------------------------------------------
//	ASSetRect
// ---------------------------------------------------------------------------

inline void ASSetRect (ASRect* rect, short l, short t, short r, short b)
{
    rect->left   = l;
    rect->top    = t;
    rect->right  = r;
    rect->bottom = b;
}


// ---------------------------------------------------------------------------
//	CenterRectInRect
// ---------------------------------------------------------------------------
//
//	Return a rect that has the aspect ratio of rect A, and is centered
//	in rect B.
//

static void CenterRectInRect
(
	const ASRect&	inRectToCenter,
	const ASRect&	inBoundsRect,
    ASRect&			outRect
)
{
	int				cw, ch;
	int				bw, bh;
	int				ow, oh;
	double			dx, dy;
	
	cw = inRectToCenter.right - inRectToCenter.left;
	ch = inRectToCenter.bottom - inRectToCenter.top;
	bw = inBoundsRect.right - inBoundsRect.left;
	bh = inBoundsRect.bottom - inBoundsRect.top;
	
	dx = double (cw) / double (bw);
	dy = double (ch) / double (bh);

	if (dy > dx)
	{
		ow = cw / dy;
		oh = ch / dy;
	}
	else
	{
		ow = cw / dx;
		oh = ch / dx;
	}

	if (cw < bw)
	{
		ow = cw;
	}
	
	if (ch < bh)
	{
		oh = ch;
	}
        	
	outRect.left 	= inBoundsRect.left + ((bw - ow) / 2);
	outRect.top 	= inBoundsRect.top + ((bh - oh) / 2);
	outRect.right	= outRect.left + ow;
	outRect.bottom	= outRect.top + oh;
}

#pragma mark-

// ---------------------------------------------------------------------------
//	AllocatePreview
// ---------------------------------------------------------------------------

static void ReadAllocatePreview (GPtr globals, ADMItemRef previewWidget)
{
	int 				w, h, dx, dy;
	int 				pw, ph;
	ASRect				r1, r2, r3;
	float				xSkip, ySkip;
	Array2D< Rgba >		scanline;
	float 				x1, x2, y1, y2;
	int 				x1i, x2i, y1i, y2i;
	ASRect              previewRect;
    int                 previewWidth, previewHeight;

#if Macintosh
    unsigned int        step = 0;
    unsigned int        lastUpdate = 0;
#endif

#if MSWindows
	SetCursor (LoadCursor (NULL, IDC_WAIT));
#endif
    
    // get dimensions of preview widget
    
    sItemSuite->GetBoundsRect (previewWidget, &previewRect);
    previewWidth  = previewRect.right - previewRect.left;
    previewHeight = previewRect.bottom - previewRect.top;

    
	// get dimensions of image on disk	
	
	const Imath::Box2i& dw = globals->inputFile->dataWindow();
	
	w 	= dw.max.x - dw.min.x + 1;
	h 	= dw.max.y - dw.min.y + 1;
	dx 	= dw.min.x;
	dy 	= dw.min.y;
	
	
	// get dimensions preview should be
	// we want to preserve the aspect ratio of the real image
		
	ASSetRect (&r1, 0, 0, w, h);
	ASSetRect (&r2, 0, 0, previewWidth, previewHeight);
	CenterRectInRect (r1, r2, r3);
	
    pw = min ((int) (r3.right - r3.left), previewWidth);
    ph = min ((int) (r3.bottom - r3.top), previewHeight);
	
	
	// get skip amounts for downsampling resolution
	
	xSkip = ((float)w / (float)pw);
	ySkip = ((float)h / (float)ph);

	
	// allocate EXR buffers	

	scanline.resizeErase (1, w);
	sEXRBuffer.resizeErase (ph, pw);
		
	
	// read and downsample one scanline at a time
	
	for (y1 = dw.min.y, y2 = 0; y2 < ph; y1 += ySkip, y2 += 1)
	{
		y1i = round (y1);
		y2i = round (y2);
	
	
		// read scanline
	
		globals->inputFile->setFrameBuffer (&scanline[-y1i][-dx], 1, w);
		globals->inputFile->readPixels (y1i);
		
		
		// downsample scanline into preview buffer
		
		for (x1 = dw.min.x, x2 = 0; x2 < pw; x1 += xSkip, x2 += 1)
		{
			x1i = round (x1);
			x2i = round (x2);
		
			sEXRBuffer[y2i][x2i] = scanline[0][x1i];
		}	
		
		
		// give a little feedback
		
#if Macintosh 
		unsigned int now = TickCount();
		if (now - lastUpdate > 20)
		{
		    SetAnimatedThemeCursor (kThemeWatchCursor, step++);
		    lastUpdate = now;
		}		
#endif
				
	}
	
	
	// allocate 8-bit buffer for drawing to screen
	
	sPreviewImage = sImageSuite->Create (pw, ph, 0);	
}


// ---------------------------------------------------------------------------
//	FreePreview
// ---------------------------------------------------------------------------

static void FreePreview ()
{
    if (sPreviewImage != NULL)
    {
        sImageSuite->Destroy (sPreviewImage);
        sPreviewImage = NULL;
    }
    
    sEXRBuffer.resizeErase (0, 0);
}


// ---------------------------------------------------------------------------
//	ResamplePreview
// ---------------------------------------------------------------------------

static void ResamplePreview (GPtr globals)
{
	// downsample the 16-bit EXR data into the 8-bit buffer

	if (sPreviewImage != NULL)
	{	
		ASBytePtr		baseAddr;
		int				w, h, rowBytes;

	
		// get preview image info
		
		baseAddr = sImageSuite->BeginBaseAddressAccess (sPreviewImage);
		w        = sImageSuite->GetWidth (sPreviewImage);
		h        = sImageSuite->GetHeight (sPreviewImage);
		rowBytes = sImageSuite->GetByteWidth (sPreviewImage);
		
		
		// globals changed, so rebuild lookup table

		int bpc = globals->bpc;
		globals->bpc = 8;
		ResetHalfToIntTable (globals);		
		globals->bpc = bpc;
				
		
		// downsample one scanline at a time
		
		for (int y = 0; y < h; ++y)
		{
			// downsample scanline
		
			for (int x = 0; x < w; ++x)
			{
			    // get half pixel
			    
				Rgba	    bigPixel	=	sEXRBuffer[y][x];
				
				
				// unmult
				
				if (globals->premult && bigPixel.a != 0)
				{
					// we're going to throw away any alpha data > 1, so 
                    // clamp it to that range before using it for unmulting

                    float a = Imath::clamp ((float) bigPixel.a, 0.f, 1.f);
				
				    bigPixel.r /= a;
				    bigPixel.g /= a;
				    bigPixel.b /= a;
				}
				
				
				// convert
				
				char	    r	   		=	HalfToInt (bigPixel.r, 0);
				char	    g	   		=	HalfToInt (bigPixel.g, 1);
				char	    b			=	HalfToInt (bigPixel.b, 2);
				
				
				// write to preview buffer
				
				ASBytePtr	pix			=	&baseAddr[ (y * rowBytes) + (x * 4) ];
				
#if MSWindows
				// ADM pixel data is little endian

				*pix	=	b;	pix++;
				*pix	=	g;	pix++;
				*pix	=	r;	pix++;
				pix++;

#else
				// ADM pixel data is big endian

				pix++;
				*pix	=	r;	pix++;
				*pix	=	g;	pix++;
				*pix	=	b;	pix++;
#endif
			}
		}
	
	
		// clean up
	
	    sImageSuite->EndBaseAddressAccess (sPreviewImage);
	}	
}


// ---------------------------------------------------------------------------
//	DrawPreview - ADM callback
// ---------------------------------------------------------------------------

static void ASAPI DrawPreview (ADMItemRef item, ADMDrawerRef drawer)
{
    ASRect  rect;
    
    sDrawSuite->GetBoundsRect (drawer, &rect);
    sDrawSuite->SetADMColor (drawer, kADMBlackColor);
    sDrawSuite->FillRect (drawer, &rect);
    
    if (sPreviewImage != NULL)
    {
        sDrawSuite->DrawADMImageCentered (drawer, sPreviewImage, &rect);
    }
    else
    {
        sDrawSuite->SetADMColor (drawer, kADMWhiteColor);
        sDrawSuite->DrawTextCentered (drawer, "Click for Preview", &rect);
    }
}


// ---------------------------------------------------------------------------
//	TrackPreview - ADM callback
// ---------------------------------------------------------------------------

static ASBoolean ASAPI TrackPreview (ADMItemRef inItem, ADMTrackerRef inTracker)
{
    // we need to return true so that the notifier proc will be called

    sTrackSuite->Abort(inTracker);

    return true;
}


// ---------------------------------------------------------------------------
//	ClickPreview - ADM callback
// ---------------------------------------------------------------------------

static void ASAPI ClickPreview (ADMItemRef inItem, ADMNotifierRef inNotifier)
{
    if (sPreviewImage == NULL)
    {
    	ADMDialogRef	dialog	=	(ADMDialogRef) sItemSuite->GetUserData (inItem);
    	GPtr            globals	= 	(GPtr) sDlogSuite->GetUserData (dialog);
    	ADMItemRef		item	=	NULL;
    	
    	
    	// update the preview
    	
    	item = sDlogSuite->GetItem (dialog, kItem_Preview);
    	ReadAllocatePreview (globals, item);
    	ResamplePreview (globals);
    	sItemSuite->Invalidate (item);
	}
}

#pragma mark-

// ---------------------------------------------------------------------------
//	BuildDialog
// ---------------------------------------------------------------------------

static void BuildDialog (ADMDialogRef dialog)
{
	ADMItemRef		item;
	ASRect			rect;


	// set the dialog to the correct size

	sDlogSuite->Size (dialog, 474, 285);


	// OK button

	ASSetRect (&rect, 388, 260, 468, 280);
	item = sItemSuite->Create (dialog, kItem_OK, kADMTextPushButtonType, &rect, NULL, NULL, 0);
	sItemSuite->SetText (item, "OK");	


	// cancel button

	ASSetRect (&rect, 296, 260, 376, 280);
	item = sItemSuite->Create (dialog, kItem_Cancel, kADMTextPushButtonType, &rect, NULL, NULL, 0);
	sItemSuite->SetText (item, "Cancel");


	// defaults button

	ASSetRect (&rect, 8, 260, 88, 280);
	item = sItemSuite->Create (dialog, kItem_Defaults, kADMTextPushButtonType, &rect, NULL, NULL, 0);
	sItemSuite->SetText (item, "Defaults");


	// separator

	ASSetRect (&rect, 5, 253, 469, 255);
	item = sItemSuite->Create (dialog, kItem_Sep1, kADMFrameType, &rect, NULL, NULL, 0);
	sItemSuite->SetItemStyle (item, kADMEtchedFrameStyle);
	

    // exposure label

    ASSetRect (&rect, 15, 230, 75, 250);
    item = sItemSuite->Create (dialog, kItem_ExposureLabel, kADMTextStaticType, &rect, NULL, NULL, 0);
    sItemSuite->SetText (item, "Exposure:");
    sItemSuite->SetJustify (item, kADMRightJustify);


    // exposure control

	ASSetRect (&rect, 80, 230, 160, 250);
	item = sItemSuite->Create (dialog, kItem_Exposure, kADMSpinEditType, &rect, NULL, NULL, 0);


    // gamma label

    ASSetRect (&rect, 165, 230, 225, 250);
    item = sItemSuite->Create (dialog, kItem_GammaLabel, kADMTextStaticType, &rect, NULL, NULL, 0);
    sItemSuite->SetText (item, "Gamma:");
    sItemSuite->SetJustify (item, kADMRightJustify);


	// gamma control

	ASSetRect (&rect, 230, 230, 290, 250);
	item = sItemSuite->Create (dialog, kItem_Gamma, kADMSpinEditType, &rect, NULL, NULL, 0);


	// unmult checkbox

	ASSetRect (&rect, 320, 230, 450, 250);
	item = sItemSuite->Create (dialog, kItem_Unmult, kADMTextCheckBoxType, &rect, NULL, NULL, 0);
	sItemSuite->SetText (item, "Un-Premultiply");


	// separator

	ASSetRect (&rect, 5, 224, 469, 226);
	item = sItemSuite->Create (dialog, kItem_Sep2, kADMFrameType, &rect, NULL, NULL, 0);
	sItemSuite->SetItemStyle (item, kADMEtchedFrameStyle);
	
	
	// preview
		
	ASSetRect (&rect, 5, 5, 469, 212);
	item = sItemSuite->Create (dialog, kItem_Preview, kADMUserType, &rect, NULL, NULL, 0);


	// if on Windows, swap the OK and Cancel button positions

#if MSWindows
	item = sDlogSuite->GetItem (dialog, kItem_OK);
	sItemSuite->Move (item, 296, 260);

	item = sDlogSuite->GetItem (dialog, kItem_Cancel);
	sItemSuite->Move (item, 388, 260);
#endif
}

// ---------------------------------------------------------------------------
//	DoDialogOK - ADM callback
// ---------------------------------------------------------------------------

static void ASAPI DoDialogOK (ADMItemRef inItem, ADMNotifierRef inNotifier)
{
	ADMDialogRef	dialog	=	(ADMDialogRef) sItemSuite->GetUserData (inItem);
	GPtr            globals	= 	(GPtr) sDlogSuite->GetUserData (dialog);
	ADMItemRef		item	=	NULL;
	
	
	// apply control values to globals
	
	item = sDlogSuite->GetItem (dialog, kItem_Exposure);
	globals->exposure = sItemSuite->GetFloatValue (item);

	item = sDlogSuite->GetItem (dialog, kItem_Gamma);
	globals->gamma = sItemSuite->GetFloatValue (item);
	
	item = sDlogSuite->GetItem (dialog, kItem_Unmult);
	globals->premult = sItemSuite->GetIntValue (item);
	
	
	// call default handler
	
	sItemSuite->DefaultNotify (inItem, inNotifier);
}


// ---------------------------------------------------------------------------
//	DoDialogDefaults - ADM callback
// ---------------------------------------------------------------------------

static void ASAPI DoDialogDefaults (ADMItemRef inItem, ADMNotifierRef inNotifier)
{
	ADMDialogRef	dialog	=	(ADMDialogRef) sItemSuite->GetUserData (inItem);
	GPtr            globals	= 	(GPtr) sDlogSuite->GetUserData (dialog);
	ADMItemRef		item	=	NULL;
	
	
	// reset the globals
	
	globals->DefaultIOSettings();
	
	
	// update control values
	
	item = sDlogSuite->GetItem (dialog, kItem_Exposure);
	sItemSuite->SetFloatValue (item, globals->exposure);

	item = sDlogSuite->GetItem (dialog, kItem_Gamma);
	sItemSuite->SetFloatValue (item, globals->gamma);
	
	item = sDlogSuite->GetItem (dialog, kItem_Unmult);
	sItemSuite->SetIntValue (item, true);
	
	
	// update the preview
	
	ResamplePreview (globals);
	item = sDlogSuite->GetItem (dialog, kItem_Preview);
	sItemSuite->Invalidate (item);
}


// ---------------------------------------------------------------------------
//	DoDialogControl - ADM callback
// ---------------------------------------------------------------------------

static void ASAPI DoDialogControl (ADMItemRef inItem, ADMNotifierRef inNotifier)
{
	ADMDialogRef	dialog	=	(ADMDialogRef) sItemSuite->GetUserData (inItem);
	GPtr            globals	= 	(GPtr) sDlogSuite->GetUserData (dialog);
	ADMItemRef		item	=	NULL;
	
	
	// call default handler first
	
	sItemSuite->DefaultNotify (inItem, inNotifier);
	

	// apply control values to globals
	
	item = sDlogSuite->GetItem (dialog, kItem_Exposure);
	globals->exposure = sItemSuite->GetFloatValue (item);

	item = sDlogSuite->GetItem (dialog, kItem_Gamma);
	globals->gamma = sItemSuite->GetFloatValue (item);
	
	item = sDlogSuite->GetItem (dialog, kItem_Unmult);
	globals->premult = sItemSuite->GetIntValue (item);
	
	
	// update the preview with the new globals values
	
	ResamplePreview (globals);
	item = sDlogSuite->GetItem (dialog, kItem_Preview);
	sItemSuite->Invalidate (item);
	
}


// ---------------------------------------------------------------------------
//	DoDialogInit - ADM callback
// ---------------------------------------------------------------------------

static ASErr ASAPI DoDialogInit (ADMDialogRef dialog)
{
	GPtr            globals	= 	(GPtr) sDlogSuite->GetUserData (dialog);
	ADMItemRef		item	=	NULL;
	

	// create dialog

    BuildDialog (dialog);


	// set dialog title
	
	sDlogSuite->SetText (dialog, "EXR Import Settings");
	
	
	// set control values
	
	item = sDlogSuite->GetItem (dialog, kItem_Exposure);
	sItemSuite->SetUnits (item, kADMNoUnits);
	sItemSuite->SetFloatValue (item, globals->exposure);
	sItemSuite->SetUserData (item, dialog);
	sItemSuite->SetNotifyProc (item, DoDialogControl);

	item = sDlogSuite->GetItem (dialog, kItem_Gamma);
	sItemSuite->SetUnits (item, kADMNoUnits);
	sItemSuite->SetFloatValue (item, globals->gamma);
	sItemSuite->SetUserData (item, dialog);
	sItemSuite->SetNotifyProc (item, DoDialogControl);
		
	item = sDlogSuite->GetItem (dialog, kItem_Unmult);
	sItemSuite->SetIntValue (item, globals->premult);
	sItemSuite->SetUserData (item, dialog);
	sItemSuite->SetNotifyProc (item, DoDialogControl);
	
	
	// set up the preview widget
	
	item = sDlogSuite->GetItem (dialog, kItem_Preview);
	sItemSuite->SetDrawProc (item, DrawPreview);
	sItemSuite->SetUserData (item, dialog);
	sItemSuite->SetNotifyProc (item, ClickPreview);
	sItemSuite->SetMask (item, kADMButtonUpMask);
	sItemSuite->SetTrackProc (item, TrackPreview);
	
	
	// set "OK" callback
	
	item = sDlogSuite->GetItem (dialog, kItem_OK);
	sItemSuite->SetUserData (item, dialog);
	sItemSuite->SetNotifyProc (item, DoDialogOK);
	
	
	// set "Defaults" callback
	
	item = sDlogSuite->GetItem (dialog, kItem_Defaults);
	sItemSuite->SetUserData (item, dialog);
	sItemSuite->SetNotifyProc (item, DoDialogDefaults);
	
	
	return kSPNoError;
}
    
#pragma mark-

// ---------------------------------------------------------------------------
//	EXRImportDialog - show the Import Settings dialog
// ---------------------------------------------------------------------------

bool EXRImportDialog (GPtr ioGlobals, SPBasicSuite* inSPBasic, void* inPluginRef)
{
    int item = kItem_Cancel;


    // get suites

	inSPBasic->AcquireSuite (kADMDialogSuite,  kADMDialogSuiteVersion5,  (void**) &sDlogSuite);
    inSPBasic->AcquireSuite (kADMItemSuite,    kADMItemSuiteVersion5,    (void**) &sItemSuite);
    inSPBasic->AcquireSuite (kADMDrawerSuite,  kADMDrawerSuiteVersion3,  (void**) &sDrawSuite);
    inSPBasic->AcquireSuite (kADMImageSuite,   kADMImageSuiteVersion2,   (void**) &sImageSuite);
    inSPBasic->AcquireSuite (kADMTrackerSuite, kADMTrackerSuiteVersion1, (void**) &sTrackSuite);


    // show dialog
    
    if (sDlogSuite != NULL && sItemSuite != NULL && sDrawSuite != NULL && sImageSuite != NULL)
    {
        item = sDlogSuite->Modal ((SPPluginRef) inPluginRef, 
		                          "EXR Import Settings", 
		                          0, 
		                          kADMModalDialogStyle, 
		                          DoDialogInit, 
		                          ioGlobals, 
		                          0);    
		                          
        FreePreview();
    }
    
    
    // release suites
    
	if (sDlogSuite != NULL)
	{
	    inSPBasic->ReleaseSuite (kADMDialogSuite, kADMDialogSuiteVersion5);
		sDlogSuite = NULL;
	}
	
	if (sItemSuite != NULL)
	{	
		inSPBasic->ReleaseSuite (kADMItemSuite, kADMItemSuiteVersion5);
		sItemSuite = NULL;
	}
	
	if (sDrawSuite != NULL)
	{
	    inSPBasic->ReleaseSuite (kADMDrawerSuite, kADMDrawerSuiteVersion3);
		sDrawSuite = NULL;
	}	
	
	if (sImageSuite != NULL)
	{
	    inSPBasic->ReleaseSuite (kADMImageSuite, kADMImageSuiteVersion2);
		sImageSuite = NULL;
	}	
	
	if (sTrackSuite != NULL)
	{
	    inSPBasic->ReleaseSuite (kADMTrackerSuite, kADMTrackerSuiteVersion1);
		sTrackSuite = NULL;
	}	


    // return true if user hit OK, false if user hit Cancel

    return (item == kItem_OK);
}


