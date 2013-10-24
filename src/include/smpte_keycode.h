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


#ifndef OPENIMAGEIO_SMPTE_KEYCODE_H
#define OPENIMAGEIO_SMPTE_KEYCODE_H

#include <string>

#include "version.h"

OIIO_NAMESPACE_ENTER
{

//-----------------------------------------------------------------------------
//
//	class SMPTE_KeyCode
// 	
// 	An SMPTE_KeyCode object uniquely identifies a motion picture film frame.
// 	The following fields specifiy film manufacturer, film type, film
// 	roll and the frame's position within the roll:
//
//	    filmMfcCode		film manufacturer code
//				range: 0 - 99
//
//	    filmType		film type code
// 				range: 0 - 99
//
//	    prefix		prefix to identify film roll
// 				range: 0 - 999999
//
//	    count		count, increments once every perfsPerCount
// 				perforations (see below)
// 				range: 0 - 9999
//
//	    perfOffset		offset of frame, in perforations from
// 				zero-frame reference mark
// 				range: 0 - 119
//
//	    perfsPerFrame	number of perforations per frame 
// 				range: 1 - 15
//
//				typical values:
//
//				    1 for 16mm film
//				    3, 4, or 8 for 35mm film
//				    5, 8 or 15 for 65mm film
//
//	    perfsPerCount	number of perforations per count 
// 				range: 20 - 120
//
//				typical values:
//
//				    20 for 16mm film
//				    64 for 35mm film
//				    80 or 120 for 65mm film
//
// 	For more information about the interpretation of those fields see
// 	the following standards and recommended practice publications:
//
// 	    SMPTE 254	MotionSMPTE_SMPTE_-Picture Film (35-mm) - Manufacturer-Printed
// 			Latent Image Identification Information
//
// 	    SMPTE 268M 	File Format for Digital Moving-Picture Exchange (DPX)
// 			(section 6.1)
//
// 	    SMPTE 270	Motion-Picture Film (65-mm) - Manufacturer- Printed
// 			Latent Image Identification Information
//
// 	    SMPTE 271	Motion-Picture Film (16-mm) - Manufacturer- Printed
// 			Latent Image Identification Information
//
//-----------------------------------------------------------------------------

   
class SMPTE_KeyCode
{
  public:

    //-------------------------------------
    // Constructors and assignment operator
    //-------------------------------------

    SMPTE_KeyCode (int filmMfcCode = 0,
	     int filmType = 0,
	     int prefix = 0,
	     int count = 0,
	     int perfOffset = 0,
	     int perfsPerFrame = 4,
	     int perfsPerCount = 64);

    SMPTE_KeyCode ( const char *filmMfcCode,
	     const char *filmType,
	     const char *prefix,
	     const char *count,
	     const char *perfOffset,
	     const char *format);

    SMPTE_KeyCode (const SMPTE_KeyCode &other);
    SMPTE_KeyCode & operator = (const SMPTE_KeyCode &other);


    //----------------------------
    // Access to individual fields
    //----------------------------

    int		filmMfcCode () const;
    void	filmMfcCode (char *str) const;
    void	setFilmMfcCode (int filmMfcCode);

    int		filmType () const;
    void	filmType (char *str) const;
    void	setFilmType (int filmType);

    int		prefix () const;
    void	prefix (char *str) const;
    void	setPrefix (int prefix);

    int		count () const;
    void	count (char *str) const;
    void	setCount (int count);

    int		perfOffset () const;
    void	perfOffset (char *str) const;
    void	setPerfOffset (int perfOffset);

    int		perfsPerFrame () const;
    void	setPerfsPerFrame (int perfsPerFrame);

    int		perfsPerCount () const;
    void	setPerfsPerCount (int perfsPerCount);

    void    format( char *str) const;
    void    setFormat( const std::string &format);

	void    toArray(int *dstArray);

  private:

    int		_filmMfcCode;
    int		_filmType;
    int		_prefix;
    int		_count;
    int		_perfOffset;
    int		_perfsPerFrame;
    int		_perfsPerCount;
};


}
OIIO_NAMESPACE_EXIT

#endif // OPENIMAGEIO_SMPTE_KEYCODE_H
