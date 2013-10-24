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
//	class SMPTE_TimeCode
// 	
//-----------------------------------------------------------------------------

#include "smpte_timecode.h"
#include "typedesc.h"
#include "dassert.h"
#include <stdio.h>
#include <sstream>
#include <iomanip>

OIIO_NAMESPACE_ENTER
{

   
SMPTE_TimeCode::SMPTE_TimeCode ()
{
    _time = 0;
    _user = 0;
}


SMPTE_TimeCode::SMPTE_TimeCode
    (int hours,
     int minutes,
     int seconds,
     int frame,
     bool dropFrame,
     bool colorFrame,
     bool fieldPhase,
     bool bgf0,
     bool bgf1,
     bool bgf2,
     int binaryGroup1,
     int binaryGroup2,
     int binaryGroup3,
     int binaryGroup4,
     int binaryGroup5,
     int binaryGroup6,
     int binaryGroup7,
     int binaryGroup8)
{
    setHours (hours);
    setMinutes (minutes);
    setSeconds (seconds);
    setFrame (frame);
    setDropFrame (dropFrame);
    setColorFrame (colorFrame);
    setFieldPhase (fieldPhase);
    setBgf0 (bgf0);
    setBgf1 (bgf1);
    setBgf2 (bgf2);
    setBinaryGroup (1, binaryGroup1);
    setBinaryGroup (2, binaryGroup2);
    setBinaryGroup (3, binaryGroup3);
    setBinaryGroup (4, binaryGroup4);
    setBinaryGroup (5, binaryGroup5);
    setBinaryGroup (6, binaryGroup6);
    setBinaryGroup (7, binaryGroup7);
    setBinaryGroup (8, binaryGroup8);
}


SMPTE_TimeCode::SMPTE_TimeCode
    (unsigned int timeAndFlags,
     unsigned int userData,
     Packing packing)
{
    setTimeAndFlags (timeAndFlags, packing);
    setUserData (userData);
}


SMPTE_TimeCode::SMPTE_TimeCode
    (std::string value)
{
    fromString(value);
}


SMPTE_TimeCode::SMPTE_TimeCode (const SMPTE_TimeCode &other)
{
    _time = other._time;
    _user = other._user;
}


SMPTE_TimeCode &
SMPTE_TimeCode::operator = (const SMPTE_TimeCode &other)
{
    _time = other._time;
    _user = other._user;
    return *this;
}

    
bool
SMPTE_TimeCode::operator == (const SMPTE_TimeCode & c) const
{
    return (_time == c._time && _user == c._user);
}


bool
SMPTE_TimeCode::operator != (const SMPTE_TimeCode & c) const
{
    return (_time != c._time || _user != c._user);
}
    
    

namespace {

unsigned int
bitField (unsigned int value, int minBit, int maxBit)
{
    int shift = minBit;
    unsigned int mask = (~(~0U << (maxBit - minBit + 1)) << minBit);
    return (value & mask) >> shift;
}


void
setBitField (unsigned int &value, int minBit, int maxBit, unsigned int field)
{
    int shift = minBit;
    unsigned int mask = (~(~0U << (maxBit - minBit + 1)) << minBit);
    value = ((value & ~mask) | ((field << shift) & mask));
}


int
bcdToBinary (unsigned int bcd)
{
    return int ((bcd & 0x0f) + 10 * ((bcd >> 4) & 0x0f));
}


unsigned int
binaryToBcd (int binary)
{
    int units = binary % 10;
    int tens = (binary / 10) % 10;
    return (unsigned int) (units | (tens << 4));
}


} // namespace


int
SMPTE_TimeCode::hours () const
{
    return bcdToBinary (bitField (_time, 24, 29));
}


void
SMPTE_TimeCode::setHours (int value)
{
    if (value < 0 || value > 23)
        ASSERT_MSG(0, "setHours value '%d' is out of range.", value);

    setBitField (_time, 24, 29, binaryToBcd (value));
}


int
SMPTE_TimeCode::minutes () const
{
    return bcdToBinary (bitField (_time, 16, 22));
}


void
SMPTE_TimeCode::setMinutes (int value)
{
    if (value < 0 || value > 59)
        ASSERT_MSG(0, "setMinutes value '%d' is out of range.", value);

    setBitField (_time, 16, 22, binaryToBcd (value));
}


int
SMPTE_TimeCode::seconds () const
{
    return bcdToBinary (bitField (_time, 8, 14));
}


void
SMPTE_TimeCode::setSeconds (int value)
{
    if (value < 0 || value > 59)
        ASSERT_MSG(0, "setSeconds value '%d' is out of range.", value);

    setBitField (_time, 8, 14, binaryToBcd (value));
}


int
SMPTE_TimeCode::frame () const
{
    return bcdToBinary (bitField (_time, 0, 5));
}


void
SMPTE_TimeCode::setFrame (int value)
{
    if (value < 0 || value > 59)
        ASSERT_MSG(0, "setFrame value '%d' is out of range.", value);

    setBitField (_time, 0, 5, binaryToBcd (value));
}


bool
SMPTE_TimeCode::dropFrame () const
{
    return !!bitField (_time, 6, 6);
}


void
SMPTE_TimeCode::setDropFrame (bool value)
{
    setBitField (_time, 6, 6, (unsigned int) !!value);
}


bool
SMPTE_TimeCode::colorFrame () const
{
    return !!bitField (_time, 7, 7);
}


void
SMPTE_TimeCode::setColorFrame (bool value)
{
    setBitField (_time, 7, 7, (unsigned int) !!value);
}


bool
SMPTE_TimeCode::fieldPhase () const
{
    return !!bitField (_time, 15, 15);
}


void
SMPTE_TimeCode::setFieldPhase (bool value)
{
    setBitField (_time, 15, 15, (unsigned int) !!value);
}


bool
SMPTE_TimeCode::bgf0 () const
{
    return !!bitField (_time, 23, 23);
}


void
SMPTE_TimeCode::setBgf0 (bool value)
{
    setBitField (_time, 23, 23, (unsigned int) !!value);
}


bool
SMPTE_TimeCode::bgf1 () const
{
    return!!bitField (_time, 30, 30);
}


void
SMPTE_TimeCode::setBgf1 (bool value)
{
    setBitField (_time, 30, 30, (unsigned int) !!value);
}


bool
SMPTE_TimeCode::bgf2 () const
{
    return !!bitField (_time, 31, 31);
}


void
SMPTE_TimeCode::setBgf2 (bool value)
{
    setBitField (_time, 31, 31, (unsigned int) !!value);
}


int
SMPTE_TimeCode::binaryGroup (int group) const
{
    if (group < 1 || group > 8)
        ASSERT_MSG(0, "binaryGroup number '%d' is out of range.", group);

    int minBit = 4 * (group - 1);
    int maxBit = minBit + 3;
    return int (bitField (_user, minBit, maxBit));
}


void
SMPTE_TimeCode::setBinaryGroup (int group, int value)
{
    if (group < 1 || group > 8)
        ASSERT_MSG(0, "binaryGroup number '%d' is out of range.", group);

    int minBit = 4 * (group - 1);
    int maxBit = minBit + 3;
    setBitField (_user, minBit, maxBit, (unsigned int) value);
}


unsigned int
SMPTE_TimeCode::timeAndFlags (Packing packing) const
{
    if (packing == TV50_PACKING)
    {
	unsigned int t = _time;

	t &= ~((1 << 6) | (1 << 15) | (1 << 23) | (1 << 30) | (1 << 31));

	t |= ((unsigned int) bgf0() << 15);
	t |= ((unsigned int) bgf2() << 23);
	t |= ((unsigned int) bgf1() << 30);
	t |= ((unsigned int) fieldPhase() << 31);

	return t;
    }
    if (packing == FILM24_PACKING)
    {
	return _time & ~((1 << 6) | (1 << 7));
    }
    else // packing == TV60_PACKING
    {
	return _time;
    }
}


void
SMPTE_TimeCode::setTimeAndFlags (unsigned int value, Packing packing)
{
    if (packing == TV50_PACKING)
    {
	_time = value &
		 ~((1 << 6) | (1 << 15) | (1 << 23) | (1 << 30) | (1 << 31));

	if (value & (1 << 15))
	    setBgf0 (true);

	if (value & (1 << 23))
	    setBgf2 (true);

	if (value & (1 << 30))
	    setBgf1 (true);

	if (value & (1 << 31))
	    setFieldPhase (true);
    }
    else if (packing == FILM24_PACKING)
    {
	_time = value & ~((1 << 6) | (1 << 7));
    }
    else // packing == TV60_PACKING
    {
	_time = value;
    }
}


unsigned int
SMPTE_TimeCode::userData () const
{
    return _user;
}


void
SMPTE_TimeCode::setUserData (unsigned int value)
{
    _user = value;
}


std::string
SMPTE_TimeCode::toString() const
{
    int values[] = {hours(), minutes(), seconds(), frame()};
    std::stringstream ss;
    for (int i=0; i<4; i++)
    {
        std::ostringstream padded;
        padded << std::setw(2) << std::setfill('0') << values[i];
        ss << padded.str();
        if (i != 3)
        {
            if (i == 2)
            {
                dropFrame() ? ss << ';' : ss << ':';
            }
            else
            {
                ss << ':';
            }
		}
    }
    return ss.str();
}


void
SMPTE_TimeCode::fromString(const std::string value)
{
    int _hours, _minutes, _seconds, _frame;
    bool drop;
    
    if ( sscanf( value.c_str(), "%02d:%02d:%02d:%02d", &_hours, &_minutes, &_seconds, &_frame) == 4)
    {
        drop = false;
    }
    else if (sscanf( value.c_str(), "%02d:%02d:%02d;%02d", &_hours, &_minutes, &_seconds, &_frame) == 4)
    {
        drop = true;
    }
    else
    {
        ASSERT_MSG(0, "Unrecognised timecode string '%s'.", value.c_str());
    }

    setHours(_hours);
    setMinutes(_minutes);
    setSeconds(_seconds);
    setFrame(_frame);
    setDropFrame(drop);
}

}
OIIO_NAMESPACE_EXIT
