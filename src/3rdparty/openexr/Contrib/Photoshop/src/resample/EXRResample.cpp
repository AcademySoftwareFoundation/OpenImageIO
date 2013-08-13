// ===========================================================================
//	EXRResample.cp				Part of OpenEXR
// ===========================================================================
//
//	Routines for converting EXR pixel data to integers of various bit depths.
//	Configuration parameters are passed in Globals struct
//

#if MSWindows
#pragma warning (disable: 161)
#endif

#include "EXRResample.h"


//-------------------------------------------------------------------------------
//	big lookup tables
//-------------------------------------------------------------------------------

unsigned short halftoint [4] [1<<16]; 
unsigned short inttohalf [4] [1<<16]; 


//-------------------------------------------------------------------------------
//	clamp
//-------------------------------------------------------------------------------

inline int clampi (int v, int l, int h)
{
	return v < l ? l : v > h ? h : v;
}


//-------------------------------------------------------------------------------
//	ResetHalfToIntTable
//-------------------------------------------------------------------------------

void ResetHalfToIntTable (const GPtr inGlobals)
{

	float	multiplier;
	int 	i, c, v;
	half 	x;
	int		maxval;


	// get float - int scalar
	
	maxval = inGlobals->MaxPixelValue();
	

	// calculate exposure multiplier

	multiplier = pow (2.0, inGlobals->exposure);
		
	
		
	// build table
			
	for (i = 0; i < 1<<16; ++i)
	{
		x.setBits (i);
		
	    
	    //------------------------------------------
		//  rgb
	    //------------------------------------------
	    
		for (c = 0; c < 3; ++c)
		{	
			float f = x;	
			
			
			// apply exposure multiplier
			
			f *= multiplier;
			
			
			// apply gamma
			
			if (inGlobals->gamma != 0)
			{
			    f = pow ((double) f, 1.0 / inGlobals->gamma);
			}
			
			
			// convert to integer space
			
			v = f * maxval + .5;
			
			
			// clamp to legal range
			
			v = clampi (v, 0, maxval);
			
		
			// store value	
		
			halftoint[c][i] = v;
		}
		
		
		//------------------------------------------
		//  alpha - no correction applied
	    //------------------------------------------
		
		
		// convert to integer space
		
		v = x * maxval + .5;
		
		
		// clamp to legal range
		
		v = clampi (v, 0, maxval);		
				
		
		// store value
		
		halftoint[3][i] = v;
	}
}


//-------------------------------------------------------------------------------
//	ResetIntToHalfTable
//-------------------------------------------------------------------------------

void ResetIntToHalfTable (const GPtr inGlobals)
{
	float	multiplier;
	int 	i, c;
	half 	x;
	float   f;
	int		maxval;
	

	// get float - int scalar
	
	maxval = inGlobals->MaxPixelValue();
	

	// calculate exposure multiplier

	multiplier = pow (2.0, inGlobals->exposure);
	
		
	// build table
			
	for (i = 0; i < 1<<16; ++i)
	{
	
	
		//------------------------------------------
		//  rgb
	    //------------------------------------------
	
		for (c = 0; c < 3; ++c)
		{	
		
		    // get float
				
			f = (float) i;
			
			
			// convert to float range
			
			f /= maxval;
			
			
			// undo gamma
			
			if (inGlobals->gamma != 0)
			{
			    f = pow ((double) f, inGlobals->gamma);
			}
							
			
			// undo exposure
			
			f /= multiplier;
			
			
			// store value
			
			x = f;		
			inttohalf[c][i] = x.bits();
		}
		
		
		//------------------------------------------
		//  alpha - no correction applied
	    //------------------------------------------
		
		
		// get float
		
		f = (float) i;
		
		
		// convert to float range
		
		f = f / maxval;
		
		
		// store value
		
		x = f;
		inttohalf[3][i] = x.bits();
	}
}

