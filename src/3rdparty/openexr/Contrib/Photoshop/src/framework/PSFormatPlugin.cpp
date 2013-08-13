// ===========================================================================
//	PSFormatPlugin.cp 			Part of OpenEXR
// ===========================================================================

#if MSWindows
#pragma warning (disable: 161)
#endif

#include "PSFormatPlugin.h"

#include "PIFormat.h"

#include <string.h>



//-------------------------------------------------------------------------------
//	PSFormatPlugin
//-------------------------------------------------------------------------------

PSFormatPlugin::PSFormatPlugin ()
{
	mGlobals	=	NULL;
	mFormatRec	=	NULL;
	mResult		=	NULL;
}


//-------------------------------------------------------------------------------
//	~PSFormatPlugin
//-------------------------------------------------------------------------------

PSFormatPlugin::~PSFormatPlugin ()
{

}

#pragma mark-

//-------------------------------------------------------------------------------
//	Run
//-------------------------------------------------------------------------------

void PSFormatPlugin::Run 
(
	short 			inSelector,
 	FormatRecord* 	inFormatRecord,
 	long* 			inData,
 	short* 			outResult
)
{
	// plug-in's main routine
	// does the work of setting up the globals, and then
	// calls the appropriate override hook

	if (inSelector == formatSelectorAbout)
	{
		// format record isn't valid, so can't set up globals
		// just show about box
	
		DoAbout ((AboutRecord*) inFormatRecord);
	}
	else
	{
		// set up globals
	
		mResult 	= 	outResult;
		mFormatRec 	=	inFormatRecord;
		
		AllocateGlobals (inFormatRecord, inData, outResult);
	
		if (mGlobals == NULL)
		{
			*outResult = memFullErr;
			return;
		}
	
		
		// handle selector through override hooks
	
		switch (inSelector)
		{
			case formatSelectorFilterFile:			DoFilterFile();			break;

			case formatSelectorReadPrepare:			DoReadPrepare();		break;
			case formatSelectorReadStart:			DoReadStart();			break;
			case formatSelectorReadContinue:		DoReadContinue();		break;
			case formatSelectorReadFinish:			DoReadFinish();			break;

			case formatSelectorOptionsPrepare:		DoOptionsPrepare();		break;
			case formatSelectorOptionsStart:		DoOptionsStart();		break;
			case formatSelectorOptionsContinue:		DoOptionsContinue();	break;
			case formatSelectorOptionsFinish:		DoOptionsFinish();		break;

			case formatSelectorEstimatePrepare:		DoEstimatePrepare();	break;
			case formatSelectorEstimateStart:		DoEstimateStart();		break;
			case formatSelectorEstimateContinue:	DoEstimateContinue();	break;
			case formatSelectorEstimateFinish:		DoEstimateFinish();		break;

			case formatSelectorWritePrepare:		DoWritePrepare();		break;
			case formatSelectorWriteStart:			DoWriteStart();			break;
			case formatSelectorWriteContinue:		DoWriteContinue();		break;
			case formatSelectorWriteFinish:			DoWriteFinish();		break;

			default: *mResult = formatBadParameters;						break;
		}
		
		
		// unlock the handle containing our globals
		
		if ((Handle)*inData != NULL)
			inFormatRecord->handleProcs->unlockProc ((Handle)*inData);
	}
}

#pragma mark-

//-------------------------------------------------------------------------------
//	AllocateGlobals
//-------------------------------------------------------------------------------
//	
void PSFormatPlugin::AllocateGlobals
(
	FormatRecord* 	inFormatRecord,
 	long* 			inData,
 	short*			outResult
)
{
	// make sure globals are ready to go
	// allocate them if necessary, set pointer correctly if not needed
	// based heavily on AllocateGlobals() in PIUtilities.c, but modified
	// to allow subclasses to easily extend the Globals struct
	
	mGlobals = NULL;
	
	if (!*inData)
	{ 
		// Data is empty, so initialize our globals
		
		// Create a chunk of memory to put our globals.
		// Have to call HostNewHandle directly, since gStuff (in
		// the PINewHandle macro) hasn't been defined yet
		// use override hook to get size of globals struct
		
		Handle h = inFormatRecord->handleProcs->newProc (GlobalsSize());
		
		if (h != NULL)
		{ 
			// We created a valid handle. Use it.
		
			// lock the handle and move it high
			// (we'll unlock it after we're done):
			
			mGlobals = (PSFormatGlobals*) inFormatRecord->handleProcs->lockProc (h, TRUE);
			
			if (mGlobals != NULL)
			{ 
				// was able to create global pointer.
			
				// if we have revert info, copy it into the globals
				// otherwise, just init them
				
				if (inFormatRecord->revertInfo != NULL)
				{
					char* ptr = inFormatRecord->handleProcs->lockProc (inFormatRecord->revertInfo, false);
					memcpy (mGlobals, ptr, GlobalsSize());
					inFormatRecord->handleProcs->unlockProc (inFormatRecord->revertInfo);
				}
				else				
				{
					InitGlobals ();
				}
				
				
				// store the handle in the passed in long *data:
				
				*inData = (long)h;
				h = NULL; // clear the handle, just in case
			
			}
			else
			{ 
				// There was an error creating the pointer.  Back out
			  	// of all of this.

				inFormatRecord->handleProcs->disposeProc (h);
				h = NULL; // just in case
			}
		}		
	}
	else
	{
		// we've already got a valid structure pointed to by *data
		// lock it, cast the returned pointer to a global pointer
		// and point globals at it:

		mGlobals = (PSFormatGlobals*) inFormatRecord->handleProcs->lockProc ((Handle)*inData, TRUE);
	}	
}


//-------------------------------------------------------------------------------
//	GlobalsSize
//-------------------------------------------------------------------------------

int PSFormatPlugin::GlobalsSize ()
{
	// override if your subclass adds fields to the Globals struct
	// the first two fields must always be:
	//	short*					result;
	//  FormatRecord*			formatParamBlock;

	return sizeof (PSFormatGlobals);
}


//-------------------------------------------------------------------------------
//	InitGlobals
//-------------------------------------------------------------------------------

void PSFormatPlugin::InitGlobals ()
{
	// override hook - PSFormatGlobals are set up in AllocateGlobals()
}

#pragma mark-

//-------------------------------------------------------------------------------
//	DoAbout
//-------------------------------------------------------------------------------

void PSFormatPlugin::DoAbout (AboutRecord* inAboutRec)
{
	// override hook
}

#pragma mark-

//-------------------------------------------------------------------------------
//	DoReadPrepare
//-------------------------------------------------------------------------------

void PSFormatPlugin::DoReadPrepare ()
{
	// override hook
}


//-------------------------------------------------------------------------------
//	DoReadStart
//-------------------------------------------------------------------------------

void PSFormatPlugin::DoReadStart ()
{
	// override hook
}


//-------------------------------------------------------------------------------
//	DoReadContinue
//-------------------------------------------------------------------------------

void PSFormatPlugin::DoReadContinue ()
{
	// override hook
}


//-------------------------------------------------------------------------------
//	DoReadFinish
//-------------------------------------------------------------------------------

void PSFormatPlugin::DoReadFinish ()
{
	// override hook
}

#pragma mark-

//-------------------------------------------------------------------------------
//	DoOptionsPrepare
//-------------------------------------------------------------------------------

void PSFormatPlugin::DoOptionsPrepare ()
{
	// override hook
}


//-------------------------------------------------------------------------------
//	DoOptionsStart
//-------------------------------------------------------------------------------

void PSFormatPlugin::DoOptionsStart ()
{
	// override hook
}


//-------------------------------------------------------------------------------
//	DoOptionsContinue
//-------------------------------------------------------------------------------

void PSFormatPlugin::DoOptionsContinue ()
{
	// override hook
}


//-------------------------------------------------------------------------------
//	DoOptionsFinish
//-------------------------------------------------------------------------------

void PSFormatPlugin::DoOptionsFinish ()
{
	// override hook
}

#pragma mark-

//-------------------------------------------------------------------------------
//	DoEstimatePrepare
//-------------------------------------------------------------------------------

void PSFormatPlugin::DoEstimatePrepare ()
{
	// override hook
}


//-------------------------------------------------------------------------------
//	DoEstimateStart
//-------------------------------------------------------------------------------

void PSFormatPlugin::DoEstimateStart ()
{
	// override hook
}


//-------------------------------------------------------------------------------
//	DoEstimateContinue
//-------------------------------------------------------------------------------

void PSFormatPlugin::DoEstimateContinue ()
{
	// override hook
}


//-------------------------------------------------------------------------------
//	DoEstimateFinish
//-------------------------------------------------------------------------------

void PSFormatPlugin::DoEstimateFinish ()
{
	// override hook
}

#pragma mark-

//-------------------------------------------------------------------------------
//	DoWritePrepare
//-------------------------------------------------------------------------------

void PSFormatPlugin::DoWritePrepare ()
{
	// override hook
}


//-------------------------------------------------------------------------------
//	DoWriteStart
//-------------------------------------------------------------------------------

void PSFormatPlugin::DoWriteStart ()
{
	// override hook
}


//-------------------------------------------------------------------------------
//	DoWriteContinue
//-------------------------------------------------------------------------------

void PSFormatPlugin::DoWriteContinue ()
{
	// override hook
}


//-------------------------------------------------------------------------------
//	DoWriteFinish
//-------------------------------------------------------------------------------

void PSFormatPlugin::DoWriteFinish ()
{
	// override hook
}

#pragma mark-

//-------------------------------------------------------------------------------
//	DoFilterFile
//-------------------------------------------------------------------------------

void PSFormatPlugin::DoFilterFile ()
{
	// override hook
}



