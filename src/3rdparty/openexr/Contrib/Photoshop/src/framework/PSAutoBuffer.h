// ===========================================================================
//	PSAutoBuffer.h				Part of OpenEXR
// ===========================================================================
//

#pragma once

#include "PIGeneral.h"


class PSAutoBuffer
{
	public:
							PSAutoBuffer		(int32			inSize,
												 BufferProcs*	inProcs);
												 
							~PSAutoBuffer		();
							
		Ptr					Lock				();
		
		
	protected:
	
		BufferProcs*		mProcs;
		BufferID			mBufferID;					

};
