///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004, Industrial Light & Magic, a division of Lucas
// Digital Ltd. LLC
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Industrial Light & Magic nor the names of
// its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission. 
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////


//-----------------------------------------------------------------------------
//
//	class SMPTE_KeyCode
//
//-----------------------------------------------------------------------------

#include "smpte_keycode.h"
#include "dassert.h"
#include <sstream>
#include <string.h>
#include <iomanip>

OIIO_NAMESPACE_ENTER
{
   
SMPTE_KeyCode::SMPTE_KeyCode (int filmMfcCode,
		  int filmType,
		  int prefix,
		  int count,
		  int perfOffset,
		  int perfsPerFrame,
		  int perfsPerCount)
{
    setFilmMfcCode (filmMfcCode);
    setFilmType (filmType);
    setPrefix (prefix);
    setCount (count);
    setPerfOffset (perfOffset);
    setPerfsPerFrame (perfsPerFrame);
    setPerfsPerCount (perfsPerCount);
}


SMPTE_KeyCode::SMPTE_KeyCode (const char *filmMfcCode,
		   const char *filmType,
		   const char *prefix,
		   const char *count,
		   const char *perfOffset,
		   const char *format)
{
	int tmp = 0;
	std::stringstream ss;

	// Manufacturer code
	ss << std::string(filmMfcCode, 2);
	ss >> tmp;
	setFilmMfcCode(tmp);
	ss.clear(); ss.str(""); tmp = 0;

	// Film type
	ss << std::string(filmType, 2);
	ss >> tmp;
	setFilmType(tmp);
	ss.clear(); ss.str(""); tmp = 0;

	// Prefix
	ss << std::string(prefix, 6);
	ss >> tmp;
	setPrefix(tmp);
	ss.clear(); ss.str(""); tmp = 0;

	// Count
	ss << std::string(count, 4);
	ss >> tmp;
	setCount(tmp);
	ss.clear(); ss.str(""); tmp = 0;

	// Perforation Offset
	ss << std::string(perfOffset, 2);
	ss >> tmp;
	setPerfOffset(tmp);
	ss.clear(); ss.str(""); tmp = 0;

	// Format
	std::string fmt(format, 32);
	setFormat(fmt);
}



SMPTE_KeyCode::SMPTE_KeyCode (const SMPTE_KeyCode &other)
{
    _filmMfcCode = other._filmMfcCode;
    _filmType = other._filmType;
    _prefix = other._prefix;
    _count = other._count;
    _perfOffset = other._perfOffset;
    _perfsPerFrame = other._perfsPerFrame;
    _perfsPerCount = other._perfsPerCount;
}


SMPTE_KeyCode &
SMPTE_KeyCode::operator = (const SMPTE_KeyCode &other)
{
    _filmMfcCode = other._filmMfcCode;
    _filmType = other._filmType;
    _prefix = other._prefix;
    _count = other._count;
    _perfOffset = other._perfOffset;
    _perfsPerFrame = other._perfsPerFrame;
    _perfsPerCount = other._perfsPerCount;

    return *this;
}


int		
SMPTE_KeyCode::filmMfcCode () const
{
    return _filmMfcCode;
}


void		
SMPTE_KeyCode::filmMfcCode (char *str) const
{
    std::stringstream ss;
	ss << std::setfill('0');
	ss << std::setw(2) << _filmMfcCode;
	memcpy(str, ss.str().c_str(), 2);
}


void	
SMPTE_KeyCode::setFilmMfcCode (int filmMfcCode)
{
    if (filmMfcCode < 0 || filmMfcCode > 99)
        ASSERT_MSG(0, "setFilmMfcCode value '%d' is out of range.", filmMfcCode);
        
    _filmMfcCode = filmMfcCode;
}

int		
SMPTE_KeyCode::filmType () const
{
    return _filmType;
}


void		
SMPTE_KeyCode::filmType (char *str) const
{
    std::stringstream ss;
	ss << std::setfill('0');
	ss << std::setw(2) << _filmType;
	memcpy(str, ss.str().c_str(), 2);
}


void	
SMPTE_KeyCode::setFilmType (int filmType)
{
    if (filmType < 0 || filmType > 99)
        ASSERT_MSG(0, "setFilmType value '%d' is out of range.", filmType);

    _filmType = filmType;
}

int		
SMPTE_KeyCode::prefix () const
{
    return _prefix;
}


void		
SMPTE_KeyCode::prefix (char *str) const
{
    std::stringstream ss;
	ss << std::setfill('0');
	ss << std::setw(6) << _prefix;
	memcpy(str, ss.str().c_str(), 6);
}


void	
SMPTE_KeyCode::setPrefix (int prefix)
{
    if (prefix < 0 || prefix > 999999)
        ASSERT_MSG(0, "setPrefix value '%d' is out of range.", prefix);

    _prefix = prefix;
}


int		
SMPTE_KeyCode::count () const
{
    return _count;
}


void		
SMPTE_KeyCode::count (char *str) const
{
    std::stringstream ss;
	ss << std::setfill('0');
	ss << std::setw(4) << _count;
	memcpy(str, ss.str().c_str(), 4);
}


void	
SMPTE_KeyCode::setCount (int count)
{
    if (count < 0 || count > 9999)
        ASSERT_MSG(0, "setCount value '%d' is out of range.", count);

    _count = count;
}


int		
SMPTE_KeyCode::perfOffset () const
{
    return _perfOffset;
}


void		
SMPTE_KeyCode::perfOffset (char *str) const
{
    std::stringstream ss;
	ss << std::setfill('0');
	ss << std::setw(2) << _perfOffset;
	memcpy(str, ss.str().c_str(), 2);
}


void	
SMPTE_KeyCode::setPerfOffset (int perfOffset)
{
    if (perfOffset < 0 || perfOffset > 119)
        ASSERT_MSG(0, "setPerfOffset value '%d' is out of range.", perfOffset);

    _perfOffset = perfOffset;
}


int	
SMPTE_KeyCode::perfsPerFrame () const
{
    return _perfsPerFrame;
}


void
SMPTE_KeyCode::setPerfsPerFrame (int perfsPerFrame)
{
    if (perfsPerFrame < 1 || perfsPerFrame > 15)
        ASSERT_MSG(0, "setPerfsPerFrame value '%d' is out of range.", perfsPerFrame);

    _perfsPerFrame = perfsPerFrame;
}


int	
SMPTE_KeyCode::perfsPerCount () const
{
    return _perfsPerCount;
}


void
SMPTE_KeyCode::setPerfsPerCount (int perfsPerCount)
{
    if (perfsPerCount < 20 || perfsPerCount > 120)
        ASSERT_MSG(0, "setPerfsPerCount value '%d' is out of range.", perfsPerCount);

    _perfsPerCount = perfsPerCount;
}


void
SMPTE_KeyCode::format(char *str) const
{
	// This method is not perfectly reversible.
	// Many formats utilise 4 perf per frame / 64 perfs per count
	// This method will provide a best guess / most generic response
	if (_perfsPerFrame == 15 && _perfsPerCount == 120)
	{
		strcpy(str, "8kimax");
	}
	else if (_perfsPerFrame == 8 && _perfsPerCount == 64)
	{
		strcpy(str, "VistaVision");
	}
	else if (_perfsPerFrame == 4 && _perfsPerCount == 64)
	{
		strcpy(str, "Full Aperture");
	}
	else if (_perfsPerFrame == 3 && _perfsPerCount == 64)
	{
		strcpy(str, "3perf");
	}
	else
	{
		strcpy(str,"Unknown");
	}
}


void
SMPTE_KeyCode::setFormat( const std::string &format )
{
	// Default to 4 perf - Standard 35mm film	
    setPerfsPerFrame( 4 );
	setPerfsPerCount( 64 );

	// These values do not seem to be documented anywhere, and are
	// usually set at the discretion of the film scanner.

    if ( format == "8kimax" )
    {
        setPerfsPerFrame( 15 );
		setPerfsPerCount( 120 );
    }
    else if ( format.substr(0,4) == "2kvv" || format.substr(0,4) == "4kvv" )
    {
        setPerfsPerFrame( 8 );
    }
    else if ( format == "VistaVision" )
    {
        setPerfsPerFrame( 8 );
    }
    else if ( format.substr(0,4) == "2k35" || format.substr(0,4) == "4k35")
    {
        setPerfsPerFrame( 4 );
    }
    else if ( format == "Full Aperture" )
    {
        setPerfsPerFrame( 4 );
    }
    else if ( format == "Academy" )
    {
        setPerfsPerFrame( 4 );
    }
    else if ( format.substr(0,7) == "2k3perf" || format.substr(0,7) == "4k3perf" )
    {
        setPerfsPerFrame( 3 );
    }
    else if ( format == "3perf" )
    {
        setPerfsPerFrame( 3 );
    }

}


void
SMPTE_KeyCode::toArray(int *dstArray)
{
	dstArray[0] = _filmMfcCode;
	dstArray[1] = _filmType;
	dstArray[2] = _prefix;
	dstArray[3] = _count;
	dstArray[4] = _perfOffset;
	dstArray[5] = _perfsPerFrame;
	dstArray[6] = _perfsPerCount;
}


}
OIIO_NAMESPACE_EXIT
