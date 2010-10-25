/* 
PTEX SOFTWARE
Copyright 2009 Disney Enterprises, Inc.  All rights reserved

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

  * The names "Disney", "Walt Disney Pictures", "Walt Disney Animation
    Studios" or the names of its contributors may NOT be used to
    endorse or promote products derived from this software without
    specific prior written permission from Walt Disney Pictures.

Disclaimer: THIS SOFTWARE IS PROVIDED BY WALT DISNEY PICTURES AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE, NONINFRINGEMENT AND TITLE ARE DISCLAIMED.
IN NO EVENT SHALL WALT DISNEY PICTURES, THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND BASED ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
*/

#include <math.h>
#include "PtexHalf.h"

uint16_t PtexHalf::f2hTable[512];
uint32_t PtexHalf::h2fTable[65536];

/** Table initializations. */
static bool PtexHalfInit()
{
    union { int i; float f; } u;

    for (int h = 0; h < 65536; h++) {
	int s = (h & 0x8000)<<16;
	int m = h & 0x03ff;
	int e = h&0x7c00;

	if (unsigned(e-1) < ((31<<10)-1)) {
	    // normal case
	    u.i = s|(((e+0x1c000)|m)<<13);
	}
	else if (e == 0) {
	    // denormalized
	    if (!(h&0x8000)) u.f = float(5.9604644775390625e-08*m);
	    else u.f = float(-5.9604644775390625e-08*m);
	}
	else {
	    // inf/nan, preserve low bits of m for nan code
	    u.i = s|0x7f800000|(m<<13);
	}
	PtexHalf::h2fTable[h] = u.i;
    }

    for (int i = 0; i < 512; i++) {
	int f = i << 23;
	int e = (f & 0x7f800000) - 0x38000000;
	// normalized iff (0 < e < 31)
	if (unsigned(e-1) < ((31<<23)-1)) {
	    int s = ((f>>16) & 0x8000);
	    int m = f & 0x7fe000;
	    // add bit 12 to round
	    PtexHalf::f2hTable[i] = (s|((e|m)>>13))+((f>>12)&1);
	}
    }

    return 1;
}

static bool PtexHalfInitialized = PtexHalfInit();


/** Handle exceptional cases for half-to-float conversion */
uint16_t PtexHalf::fromFloat_except(uint32_t i)
{
    uint32_t s = ((i>>16) & 0x8000);
    int32_t e = ((i>>13) & 0x3fc00) - 0x1c000;

    if (e <= 0) {
	// denormalized
	union { uint32_t i; float f; } u;
	u.i = i;
	return s | int(fabs(u.f)*1.6777216e7 + .5);
    }

    if (e == 0x23c00)
	// inf/nan, preserve msb bits of m for nan code
	return s|0x7c00|((i&0x7fffff)>>13);
    else
	// overflow - convert to inf
	return s|0x7c00;
}
