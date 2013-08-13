// ===========================================================================
//	PSAutoBuffer.cp				Part of OpenEXR
// ===========================================================================
//

#include "PSAutoBuffer.h"

#include <new>

PSAutoBuffer::PSAutoBuffer
(
	int32			inSize,
	BufferProcs*	inProcs
)
{
	OSErr	err;

	mBufferID	= 0;
	mProcs 		= inProcs;
	
	err = mProcs->allocateProc (inSize, &mBufferID);
	
	if (err != noErr)
	{
		throw std::bad_alloc();
	}
}
					 
PSAutoBuffer::~PSAutoBuffer ()
{
	if (mBufferID != 0)
	{
		mProcs->freeProc (mBufferID);
		mBufferID = 0;
	}
}

Ptr PSAutoBuffer::Lock ()
{
	return mProcs->lockProc (mBufferID, false);
}

