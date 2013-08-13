// ===========================================================================
//	RefNumIO.cpp								Part of OpenEXR
// ===========================================================================

#include "RefNumIO.h"

#include <IexBaseExc.h>
#include <PITypes.h>		// for Macintosh and MSWindows defines


// ===========================================================================
//	Macintosh IO Abstraction 
//
//  use 64-bit HFS+ APIs if the system supports them,
//	fall back to 32-bit classic File Manager APIs otherwise
// ===========================================================================

#pragma mark ===== Macintosh =====

#if Macintosh

#include <Gestalt.h>
#include <Files.h>


//-------------------------------------------------------------------------------
// HaveHFSPlusAPIs
//-------------------------------------------------------------------------------

static bool HaveHFSPlusAPIs ()
{
	static bool sCheckedForHFSPlusAPIs 	= 	false;
	static bool sHaveHFSPlusAPIs		=	false;

	if (!sCheckedForHFSPlusAPIs)
	{
		long 	response 	=	0;
		OSErr	err			=	noErr;
	
		err = Gestalt (gestaltFSAttr, &response);
		
		if (err == noErr && (response & (1 << gestaltHasHFSPlusAPIs)))
		{
			sHaveHFSPlusAPIs = true;	
		}
	
		sCheckedForHFSPlusAPIs = true;
	}

	return sHaveHFSPlusAPIs;
}


//-------------------------------------------------------------------------------
// Read
//-------------------------------------------------------------------------------

static bool Read (short refNum, int n, void* c)
{
	OSErr err = noErr;
	
	if (HaveHFSPlusAPIs())
	{
		ByteCount 	actual;
		
		err = FSReadFork (refNum, fsFromMark, 0, n, c, &actual);
	}
	else
	{
		long count = n;
		
		err = FSRead (refNum, &count, c);
	}
	
	return (err == noErr);
}


//-------------------------------------------------------------------------------
// Write
//-------------------------------------------------------------------------------

static bool Write (short refNum, int n, const void* c)
{
	OSErr err = noErr;
	
	if (HaveHFSPlusAPIs())
	{
		ByteCount 	actual;
		
		err = FSWriteFork (refNum, fsFromMark, 0, n, c, &actual);
	}
	else
	{
		long count = n;
		
		err = FSWrite (refNum, &count, c);
	}
	
	return (err == noErr);
}


//-------------------------------------------------------------------------------
// Tell
//-------------------------------------------------------------------------------

static bool Tell (short refNum, Imf::Int64& pos)
{
	OSErr err = noErr;
	
	if (HaveHFSPlusAPIs())
	{
		SInt64 p;
		
		err = FSGetForkPosition (refNum, &p);
		pos = p;
	}
	else
	{
		long p;
		
		err = GetFPos (refNum, &p);
		pos = p;
	}
	
	return (err == noErr);
}


//-------------------------------------------------------------------------------
// Seek
//-------------------------------------------------------------------------------

static bool Seek (short refNum, const Imf::Int64& pos)
{
	OSErr err = noErr;
	
	if (HaveHFSPlusAPIs())
	{
		err = FSSetForkPosition (refNum, fsFromStart, pos);
	}
	else
	{
		err = SetFPos (refNum, fsFromStart, pos);
	}
	
	return (err == noErr);
}


//-------------------------------------------------------------------------------
// GetSize
//-------------------------------------------------------------------------------

static bool GetSize (short refNum, Imf::Int64& size)
{
	OSErr err = noErr;
	
	if (HaveHFSPlusAPIs())
	{
		SInt64 fileSize;
		
		err  = FSGetForkSize (refNum, &fileSize);
		size = fileSize;
	}
	else
	{
		long fileSize;
		
		err  = GetEOF (refNum, &fileSize);
		size = fileSize;
	}
	
	return (err == noErr);
}

#endif

#pragma mark-
#pragma mark ===== Windows =====

// ===========================================================================
//	Windows IO Abstraction 
// ===========================================================================

#if MSWindows

//-------------------------------------------------------------------------------
// Read
//-------------------------------------------------------------------------------

static bool Read (short refNum, int n, void* c)
{
	DWORD nRead;
 
    return ReadFile ((HANDLE) refNum, c, n, &nRead, NULL);
}


//-------------------------------------------------------------------------------
// Write
//-------------------------------------------------------------------------------

static bool Write (short refNum, int n, const void* c)
{
	DWORD nRead;
 
    return WriteFile ((HANDLE) refNum, c, n, &nRead, NULL);
}


//-------------------------------------------------------------------------------
// Tell
//-------------------------------------------------------------------------------

static bool Tell (short refNum, Imf::Int64& pos)
{
	LARGE_INTEGER li;

    li.QuadPart = 0;

    li.LowPart = SetFilePointer ((HANDLE) refNum, 0, &li.HighPart, FILE_CURRENT);

    if (li.HighPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
    {
        return false;
    }

    pos = li.QuadPart;
    return true;
}


//-------------------------------------------------------------------------------
// Seek
//-------------------------------------------------------------------------------

static bool Seek (short refNum, const Imf::Int64& pos)
{
	LARGE_INTEGER li;

    li.QuadPart = pos;

    SetFilePointer ((HANDLE) refNum, li.LowPart, &li.HighPart, FILE_BEGIN);

    return !(li.HighPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR);
}


//-------------------------------------------------------------------------------
// GetSize
//-------------------------------------------------------------------------------

static bool GetSize (short refNum, Imf::Int64& size)
{
    LARGE_INTEGER li;
    DWORD         hi;

    li.QuadPart = 0;
    li.LowPart  = GetFileSize ((HANDLE) refNum, &hi);
    li.HighPart = hi;
    size        = li.QuadPart;

    return !(li.LowPart == INVALID_FILE_SIZE && GetLastError() != NO_ERROR);
}

#endif

#pragma mark-

//-------------------------------------------------------------------------------
// IStream Constructor
//-------------------------------------------------------------------------------

RefNumIFStream::RefNumIFStream 
(
	short 				refNum,
	const char 			fileName[]
) : 
	IStream 			(fileName),
	_refNum 			(refNum)
{ 

}


//-------------------------------------------------------------------------------
// IStream Destructor
//-------------------------------------------------------------------------------

RefNumIFStream::~RefNumIFStream ()
{

}


//-------------------------------------------------------------------------------
// read
//-------------------------------------------------------------------------------

bool	
RefNumIFStream::read (char c[/*n*/], int n)
{
	if (!Read (_refNum, n, c))
	{
		throw Iex::InputExc ("Unable to read file.");
	}
	
	Imf::Int64 fileSize;
	
	if (!GetSize (_refNum, fileSize))
	{
		throw Iex::InputExc ("Couldn't get file size.");	
	}
	
	return !(fileSize == tellg());
}


//-------------------------------------------------------------------------------
// tellg
//-------------------------------------------------------------------------------

Imf::Int64	
RefNumIFStream::tellg ()
{
	Imf::Int64 	fpos 	= 	0;

	if (!Tell (_refNum, fpos))
	{
		throw Iex::InputExc ("Error finding file positon.");
	}
	
	return fpos;
}


//-------------------------------------------------------------------------------
// seekg
//-------------------------------------------------------------------------------

void	
RefNumIFStream::seekg (Imf::Int64 pos)
{
	if (!Seek (_refNum, pos))
	{
		throw Iex::InputExc ("Error setting file positon.");
	}
}


//-------------------------------------------------------------------------------
// clear
//-------------------------------------------------------------------------------

void	
RefNumIFStream::clear ()
{
	// nothing to do
}

#pragma mark-

//-------------------------------------------------------------------------------
// OStream Constructor
//-------------------------------------------------------------------------------

RefNumOFStream::RefNumOFStream 
(
	short 				refNum, 
	const char 			fileName[]
) : 
	OStream 			(fileName),
	_refNum 			(refNum)
{ 

}


//-------------------------------------------------------------------------------
// OStream Destructor
//-------------------------------------------------------------------------------

RefNumOFStream::~RefNumOFStream ()
{

}


//-------------------------------------------------------------------------------
// write
//-------------------------------------------------------------------------------

void	
RefNumOFStream::write (const char c[/*n*/], int n)
{
	if (!Write (_refNum, n, c))
	{
		throw Iex::IoExc ("Unable to write file.");
	}	
}


//-------------------------------------------------------------------------------
// tellp
//-------------------------------------------------------------------------------

Imf::Int64	
RefNumOFStream::tellp ()
{
	Imf::Int64 	fpos 	= 	0;

	if (!Tell (_refNum, fpos))
	{
		throw Iex::InputExc ("Error finding file positon.");
	}
	
	return fpos;
}


//-------------------------------------------------------------------------------
// seekp
//-------------------------------------------------------------------------------

void	
RefNumOFStream::seekp (Imf::Int64 pos)
{
	if (!Seek (_refNum, pos))
	{
		throw Iex::InputExc ("Error setting file positon.");
	}
}
