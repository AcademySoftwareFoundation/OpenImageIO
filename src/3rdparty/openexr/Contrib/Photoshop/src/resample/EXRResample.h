// ===========================================================================
//	EXRResample.h				Part of OpenEXR
// ===========================================================================
//
//	Routines for converting EXR pixel data to integers of various bit depths.
//	Configuration parameters are passed in Globals struct
//
//	Channel 0 = red, 1 = green, 2 = blue, 3 = alpha
//

#pragma once

#include "half.h"
#include "EXRFormatGlobals.h"


//-------------------------------------------------------------------------------
//	Lookup table rebuilders
//-------------------------------------------------------------------------------

extern void					
ResetHalfToIntTable		(const GPtr		inGlobals);

extern void
ResetIntToHalfTable		(const GPtr		inGlobals);



//-------------------------------------------------------------------------------
//	HalfToInt
//-------------------------------------------------------------------------------

inline unsigned short		
HalfToInt
(
	const half&	inHalf,
	int			inChannel
)
{
	extern unsigned short halftoint [4] [1<<16]; 

	return halftoint[inChannel][inHalf.bits()];
}


//-------------------------------------------------------------------------------
//	IntToHalf
//-------------------------------------------------------------------------------

inline half		
IntToHalf
(
	unsigned short	inInt,
	int				inChannel
)
{
	extern unsigned short 	inttohalf [4] [1<<16]; 
	half					x;
	
	x.setBits (inttohalf[inChannel][inInt]);

	return x;
}

