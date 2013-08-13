// ===========================================================================
//	PSFormatPlugin.h			Part of OpenEXR
// ===========================================================================

#pragma once

#include "PSFormatGlobals.h"

#include "PIAbout.h"


//-------------------------------------------------------------------------------
//	PSFormatPlugin
//-------------------------------------------------------------------------------
//
//	Base class for a Photoshop File Format plugin.
//

class PSFormatPlugin
{
	public:
	
			//-------------------------------------------------------------------
			//	Constructor / Destructor
			//-------------------------------------------------------------------
	
									PSFormatPlugin			();
		virtual						~PSFormatPlugin			();


			//-------------------------------------------------------------------
			//	Run - main function called from plug-ins main entry point
			//-------------------------------------------------------------------
	
				void				Run						(short				inSelector,
															 FormatRecord*		inFormatRecord,
															 long*				inData,
															 short*				outResult);
		
	
	protected:


			//-------------------------------------------------------------------
			//	Convenience routines for making globals as painless
			//	as possible (not very painless, though)
			//-------------------------------------------------------------------
				
				void				AllocateGlobals			(FormatRecord*		inFormatRecord,
															 long*				inData,
															 short*				outResult);



			//-------------------------------------------------------------------
			//	Override hooks - subclasses should override as many of these
			//	as they need to, and disregard the rest
			//-------------------------------------------------------------------
		
		virtual int					GlobalsSize				();
		virtual void				InitGlobals				();

		virtual void				DoAbout					(AboutRecord*       inAboutRec);
			
		virtual void				DoReadPrepare			();
		virtual void				DoReadStart				();
		virtual void				DoReadContinue			();
		virtual void				DoReadFinish			();
			
		virtual void				DoOptionsPrepare		();
		virtual void				DoOptionsStart			();
		virtual void				DoOptionsContinue		();
		virtual void				DoOptionsFinish			();
			
		virtual void				DoEstimatePrepare		();
		virtual void				DoEstimateStart			();
		virtual void				DoEstimateContinue		();
		virtual void				DoEstimateFinish		();
				
		virtual void				DoWritePrepare			();
		virtual void				DoWriteStart			();
		virtual void				DoWriteContinue			();
		virtual void				DoWriteFinish			();
			
		virtual void				DoFilterFile			();


			//-------------------------------------------------------------------
			//	Globals - valid upon entry into every override hook
			//	except DoAbout().  May actually be a pointer to your
			//	subclass of PSFormatGlobals.
			//-------------------------------------------------------------------

		PSFormatGlobals*			mGlobals;
		short*						mResult;
		FormatRecord*				mFormatRec;
};
