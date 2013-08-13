// ===========================================================================
//	EXRFormatPlugin.h			Part of OpenEXR
// ===========================================================================

#pragma once

#include "PSFormatPlugin.h"

#include "EXRFormatGlobals.h"

#include "PIDefines.h"
#include "PITypes.h"


//-------------------------------------------------------------------------------
//	EXRFormatPlugin
//-------------------------------------------------------------------------------

class EXRFormatPlugin : public PSFormatPlugin
{
	public:
	
						EXRFormatPlugin			();
		virtual			~EXRFormatPlugin		();
	
	
	protected:

		// access to our Globals struct
	
 		inline EXRFormatGlobals* Globals		()
 		{
 			return (EXRFormatGlobals*) mGlobals;
 		}

		
		// PSFormatPlugin Overrides
				
		virtual int		GlobalsSize				();
		virtual void	InitGlobals				();

		virtual void	DoAbout					(AboutRecord* inAboutRec);
			
		virtual void	DoReadStart				();
		virtual void	DoReadContinue			();
		virtual void	DoReadFinish			();
			
		virtual void	DoOptionsStart			();
		virtual void	DoEstimateStart			();
 		virtual void	DoWriteStart			();


		// UI methods

 				bool 	DoImportPreviewDlog 	();
 				bool 	DoExportSettingsDlog 	();

};


//-------------------------------------------------------------------------------
//	Main entry point
//-------------------------------------------------------------------------------

DLLExport MACPASCAL 
void PluginMain 			(const short 		inSelector,
			  	             FormatRecord*		inFormatRecord,
				             long*				inData,
				             short*				outResult);
