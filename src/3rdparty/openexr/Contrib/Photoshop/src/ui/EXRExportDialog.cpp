// ===========================================================================
//	EXRExportDialog.cpp           			Part of OpenEXR
// ===========================================================================

#if MSWindows
#pragma warning (disable: 161)
#endif

#include "EXRExportDialog.h"

#include "ADMDialog.h"
#include "ADMItem.h"
#include "ADMList.h"
#include "ADMEntry.h"


// ---------------------------------------------------------------------------
//	Resource ID's
// ---------------------------------------------------------------------------

enum
{
    kItem_OK                =	1,
    kItem_Cancel,           
    kItem_Defaults,          
    kItem_Sep1,      

    kItem_ColorGroup,    	
    kItem_ExposureLabel,		
    kItem_Exposure,          
    kItem_GammaLabel,
    kItem_Gamma,             
    
    kItem_AlphaGroup,		
    kItem_Premult,           

    kItem_CompressionGroup,	
    kItem_CompressionNone,   
    kItem_CompressionRLE,    
    kItem_CompressionZip,    
    kItem_CompressionZips,   
    kItem_CompressionPiz,   

    kItem_Sep2,              
    kItem_Text1,             
    kItem_Text2             
};

// ---------------------------------------------------------------------------
//	Globals - ADM makes it hard to avoid them
// ---------------------------------------------------------------------------

static ADMDialogSuite5*     sDlogSuite              =	NULL;
static ADMItemSuite5*       sItemSuite              =	NULL;
static ADMListSuite3*       sListSuite              =   NULL;
static ADMEntrySuite4*      sEntrySuite             =   NULL;


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
//	BuildDialog
// ---------------------------------------------------------------------------

static void BuildDialog (ADMDialogRef dialog)
{

	ADMItemRef		item;
	ASRect			rect;


	// set the dialog to the correct size

	sDlogSuite->Size (dialog, 474, 295);


	// OK button

	ASSetRect (&rect, 388, 270, 468, 290);
	item = sItemSuite->Create (dialog, kItem_OK, kADMTextPushButtonType, &rect, NULL, NULL, 0);
	sItemSuite->SetText (item, "OK");	


	// cancel button

	ASSetRect (&rect, 296, 270, 376, 290);
	item = sItemSuite->Create (dialog, kItem_Cancel, kADMTextPushButtonType, &rect, NULL, NULL, 0);
	sItemSuite->SetText (item, "Cancel");


	// defaults button

	ASSetRect (&rect, 8, 270, 88, 290);
	item = sItemSuite->Create (dialog, kItem_Defaults, kADMTextPushButtonType, &rect, NULL, NULL, 0);
	sItemSuite->SetText (item, "Defaults");


	// separator

	ASSetRect (&rect, 5, 263, 469, 265);
	item = sItemSuite->Create (dialog, kItem_Sep1, 	kADMFrameType, &rect, NULL, NULL, 0);
	sItemSuite->SetItemStyle (item, kADMEtchedFrameStyle);
	

    // exposure label

    ASSetRect (&rect, 22, 140, 115, 160);
    item = sItemSuite->Create (dialog, kItem_ExposureLabel, kADMTextStaticType, &rect, NULL, NULL, 0);
    sItemSuite->SetText (item, "Exposure:");
    sItemSuite->SetJustify (item, kADMRightJustify);


    // exposure control

	ASSetRect (&rect, 120, 140, 220, 160);
	item = sItemSuite->Create (dialog, kItem_Exposure, kADMSpinEditType, &rect, NULL, NULL, 0);
	

    // gamma label

    ASSetRect (&rect, 22, 165, 115, 185);
    item = sItemSuite->Create (dialog, kItem_GammaLabel, kADMTextStaticType, &rect, NULL, NULL, 0);
    sItemSuite->SetText (item, "Gamma:");
    sItemSuite->SetJustify (item, kADMRightJustify);


	// gamma control

	ASSetRect (&rect, 120, 165, 220, 185);
	item = sItemSuite->Create (dialog, kItem_Gamma, kADMSpinEditType, &rect, NULL, NULL, 0);


	// color group

	ASSetRect (&rect, 12, 113, 268, 200);
	item = sItemSuite->Create (dialog, kItem_ColorGroup, kADMFrameType, &rect, NULL, NULL, 0);
	sItemSuite->SetItemStyle (item, kADMEtchedFrameStyle);
	sItemSuite->SetText (item, "Color Settings:");


	// premult checkbox

	ASSetRect (&rect, 65, 225, 250, 245);
	item = sItemSuite->Create (dialog, kItem_Premult, kADMTextCheckBoxType, &rect, NULL, NULL, 0);
	sItemSuite->SetText (item, "Premultiply");


	// alpha group
	
	ASSetRect (&rect, 12, 205, 268, 255);
	item = sItemSuite->Create (dialog, kItem_AlphaGroup, kADMFrameType, &rect, NULL, NULL, 0);
	sItemSuite->SetItemStyle (item, kADMEtchedFrameStyle);
	sItemSuite->SetText (item, "Alpha:");


	// compression choices

	ASSetRect (&rect, 300, 134, 444, 154);
	item = sItemSuite->Create (dialog, kItem_CompressionNone, kADMTextRadioButtonType, &rect, NULL, NULL, 0);
	sItemSuite->SetText (item, "None");

	ASSetRect (&rect, 300, 154, 444, 174);
	item = sItemSuite->Create (dialog, kItem_CompressionRLE, kADMTextRadioButtonType, &rect, NULL, NULL, 0);
	sItemSuite->SetText (item, "RLE");

	ASSetRect (&rect, 300, 174, 444, 194);
	item = sItemSuite->Create (dialog, kItem_CompressionZips, kADMTextRadioButtonType, &rect, NULL, NULL, 0);
	sItemSuite->SetText (item, "Zip");

	ASSetRect (&rect, 300, 194, 444, 214);
	item = sItemSuite->Create (dialog, kItem_CompressionZip, kADMTextRadioButtonType, &rect, NULL, NULL, 0);
	sItemSuite->SetText (item, "Zip (multi-scanline)");

	ASSetRect (&rect, 300, 214, 444, 234);
	item = sItemSuite->Create (dialog, kItem_CompressionPiz, kADMTextRadioButtonType, &rect, NULL, NULL, 0);
	sItemSuite->SetText (item, "Piz");


	// compression group

	ASSetRect (&rect, 288, 113, 464, 255);
	item = sItemSuite->Create (dialog, kItem_CompressionGroup, kADMFrameType, &rect, NULL, NULL, 0);
	sItemSuite->SetItemStyle (item, kADMEtchedFrameStyle);
	sItemSuite->SetText (item, "Compression:");		


	// separator

	ASSetRect (&rect, 5, 106, 469, 108);
	item = sItemSuite->Create (dialog, kItem_Sep2, kADMFrameType, &rect, NULL, NULL, 0);
	sItemSuite->SetItemStyle (item, kADMEtchedFrameStyle);
	

	// some text

	ASSetRect (&rect, 24, 8, 450, 44);
	item = sItemSuite->Create (dialog, kItem_Text1, kADMTextStaticType, &rect, NULL, NULL, 0);
	sItemSuite->SetText (item, "The inverse of these settings will be applied to the image.");


	// some more text
	
	ASSetRect (&rect, 24, 48, 450, 100);
	item = sItemSuite->Create (dialog, kItem_Text2, kADMTextStaticType, &rect, NULL, NULL, 0);
	sItemSuite->SetText (item, "If you use the same settings as when you loaded the image, "
						       "it will be returned to its original colorspace.");


	// if on Windows, swap the OK and Cancel button positions

#if MSWindows
	item = sDlogSuite->GetItem (dialog, kItem_OK);
	sItemSuite->Move (item, 296, 270);

	item = sDlogSuite->GetItem (dialog, kItem_Cancel);
	sItemSuite->Move (item, 388, 270);
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
	ADMListRef      list    =   NULL;
	ADMEntryRef     entry   =   NULL;
	
	
	// apply control values to globals
	
	item = sDlogSuite->GetItem (dialog, kItem_Exposure);
	globals->exposure = sItemSuite->GetFloatValue (item);

	item = sDlogSuite->GetItem (dialog, kItem_Gamma);
	globals->gamma = sItemSuite->GetFloatValue (item);

    item = sDlogSuite->GetItem (dialog, kItem_Premult);
    globals->premult = sItemSuite->GetIntValue (item);

    item = sDlogSuite->GetItem (dialog, kItem_CompressionNone);
	if (sItemSuite->GetBooleanValue (item)) globals->outputCompression = Imf::NO_COMPRESSION;

	item = sDlogSuite->GetItem (dialog, kItem_CompressionRLE);
	if (sItemSuite->GetBooleanValue (item)) globals->outputCompression = Imf::RLE_COMPRESSION;

	item = sDlogSuite->GetItem (dialog, kItem_CompressionZip);
	if (sItemSuite->GetBooleanValue (item)) globals->outputCompression = Imf::ZIP_COMPRESSION;

	item = sDlogSuite->GetItem (dialog, kItem_CompressionZips);
	if (sItemSuite->GetBooleanValue (item)) globals->outputCompression = Imf::ZIPS_COMPRESSION;

	item = sDlogSuite->GetItem (dialog, kItem_CompressionPiz);
	if (sItemSuite->GetBooleanValue (item)) globals->outputCompression = Imf::PIZ_COMPRESSION;	

    
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
	ADMListRef      list    =   NULL;
	ADMEntryRef     entry   =   NULL;
	
	
	// set control values
	
	item = sDlogSuite->GetItem (dialog, kItem_Exposure);
	sItemSuite->SetFloatValue (item, 0.0f);

	item = sDlogSuite->GetItem (dialog, kItem_Gamma);
	sItemSuite->SetFloatValue (item, 2.2f);
	
	item = sDlogSuite->GetItem (dialog, kItem_Premult);
    sItemSuite->SetIntValue (item, true);

	item  = sDlogSuite->GetItem (dialog, kItem_CompressionNone);
	sItemSuite->SetBooleanValue (item, false);

	item  = sDlogSuite->GetItem (dialog, kItem_CompressionRLE);
	sItemSuite->SetBooleanValue (item, false);

	item  = sDlogSuite->GetItem (dialog, kItem_CompressionZip);
	sItemSuite->SetBooleanValue (item, false);

	item  = sDlogSuite->GetItem (dialog, kItem_CompressionZips);
	sItemSuite->SetBooleanValue (item, false);

	item  = sDlogSuite->GetItem (dialog, kItem_CompressionPiz);
	sItemSuite->SetBooleanValue (item, true);
}


// ---------------------------------------------------------------------------
//	DoDialogInit - ADM callback
// ---------------------------------------------------------------------------

static ASErr ASAPI DoDialogInit (ADMDialogRef dialog)
{
	const char*		compressionNames[] = { "None", "RLE", "Zip - 1 Scanline", "Zip - 16 Scanlines", "Piz" };

	GPtr            globals	= 	(GPtr) sDlogSuite->GetUserData (dialog);
	ADMItemRef		item	=	NULL;
	ADMListRef      list    =   NULL;
	ADMEntryRef     entry   =   NULL;
	int				c		=	(int) globals->outputCompression;
	
	
    // create UI elements

    BuildDialog (dialog);


	// set dialog title
	
	sDlogSuite->SetText (dialog, "EXR Export Settings");
	
	
	// set control values
	
	item = sDlogSuite->GetItem (dialog, kItem_Exposure);
	sItemSuite->SetUnits (item, kADMNoUnits);
	sItemSuite->SetFloatValue (item, globals->exposure);

	item = sDlogSuite->GetItem (dialog, kItem_Gamma);
	sItemSuite->SetUnits (item, kADMNoUnits);
	sItemSuite->SetFloatValue (item, globals->gamma);
	
	item = sDlogSuite->GetItem (dialog, kItem_Premult);
    sItemSuite->SetIntValue (item, globals->premult);

    item = sDlogSuite->GetItem (dialog, kItem_CompressionNone);
	sItemSuite->SetBooleanValue (item, globals->outputCompression == Imf::NO_COMPRESSION);

	item = sDlogSuite->GetItem (dialog, kItem_CompressionRLE);
	sItemSuite->SetBooleanValue (item, globals->outputCompression == Imf::RLE_COMPRESSION);
	
	item = sDlogSuite->GetItem (dialog, kItem_CompressionZip);
	sItemSuite->SetBooleanValue (item, globals->outputCompression == Imf::ZIP_COMPRESSION);

	item = sDlogSuite->GetItem (dialog, kItem_CompressionZips);
	sItemSuite->SetBooleanValue (item, globals->outputCompression == Imf::ZIPS_COMPRESSION);

	item = sDlogSuite->GetItem (dialog, kItem_CompressionPiz);
	sItemSuite->SetBooleanValue (item, globals->outputCompression == Imf::PIZ_COMPRESSION);
		

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
    

// ---------------------------------------------------------------------------
//	EXRExportDialog - show the Export Settings dialog
// ---------------------------------------------------------------------------

bool EXRExportDialog (GPtr ioGlobals, SPBasicSuite* inSPBasic, void* inPluginRef)
{
    int item = kItem_Cancel;


    // get suites

	inSPBasic->AcquireSuite (kADMDialogSuite, kADMDialogSuiteVersion5, (void**) &sDlogSuite);
    inSPBasic->AcquireSuite (kADMItemSuite,   kADMItemSuiteVersion5,   (void**) &sItemSuite);
    inSPBasic->AcquireSuite (kADMListSuite,   kADMListSuiteVersion3,   (void**) &sListSuite);
    inSPBasic->AcquireSuite (kADMEntrySuite,  kADMEntrySuiteVersion4,  (void**) &sEntrySuite);


    // show dialog
    
    if (sDlogSuite != NULL && sItemSuite != NULL && sListSuite != NULL)
    {
        item = sDlogSuite->Modal ((SPPluginRef) inPluginRef, 
		                          "EXR Export Settings", 
		                          0, 
		                          kADMModalDialogStyle, 
		                          DoDialogInit, 
		                          ioGlobals, 
		                          0);    
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
		
	if (sListSuite != NULL)
	{
	    inSPBasic->ReleaseSuite (kADMListSuite, kADMListSuiteVersion3);
		sListSuite = NULL;
	}	
	
	if (sEntrySuite != NULL)
	{
	    inSPBasic->ReleaseSuite (kADMEntrySuite, kADMEntrySuiteVersion4);
		sEntrySuite = NULL;
	}	


    // return true if user hit OK, false if user hit Cancel

    return (item == kItem_OK);
}

