// ===========================================================================
//	PSFormatGlobals.h			Part of OpenEXR
// ===========================================================================

#pragma once

//-------------------------------------------------------------------------------
//	Forward Declarations
//-------------------------------------------------------------------------------

struct FormatRecord;


//-------------------------------------------------------------------------------
//	Globals struct
//-------------------------------------------------------------------------------
//
//	you may subclass this struct for your own globals, but don't add a vtable!
//	(no virtual methods, please)  If you do add fields, you will have to override
//	GlobalsSize() and InitGlobals() to account for your additional fields.
//

struct PSFormatGlobals
{

};


