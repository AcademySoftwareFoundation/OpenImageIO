// -*- mode: C++; tab-width: 4 -*-
// vi: ts=4

/*
* Copyright (c) 2009, Patrick A. Palmer.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*   - Redistributions of source code must retain the above copyright notice,
*     this list of conditions and the following disclaimer.
*
*   - Redistributions in binary form must reproduce the above copyright
*     notice, this list of conditions and the following disclaimer in the
*     documentation and/or other materials provided with the distribution.
*
*   - Neither the name of Patrick A. Palmer nor the names of its
*     contributors may be used to endorse or promote products derived from
*     this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/


#include "DPXColorConverter.h"
#include <algorithm>

namespace dpx {
	template <typename DATA>
	static inline bool ConvertABGRToRGBA(const DATA *input, DATA *output, int pixels) {
		// copy the data that could be destroyed to an additional buffer in case input == output
		DATA tmp[2];
		for (int i = 0; i < pixels; i++) {
			memcpy(tmp, &input[i * 4], sizeof(DATA) * 2);
			output[i * 4 + 0] = input[i * 4 + 3];
			output[i * 4 + 1] = input[i * 4 + 2];
			output[i * 4 + 2] = tmp[1];
			output[i * 4 + 3] = tmp[0];
		}
		return true;
	}

	static inline const float *GetYCbCrtoRGBColorMatrix(const Characteristic space) {
		// YCbCr -> RGB matrices
		static const float Rec601[9] = {
			// multipliers for the corresponding signals
			//			Y'		Cb			Cr
			/* R' = */	1.f,	0.f,		1.402f,
			/* G' = */	1.f,	-0.344136f,	-0.714136f,
			/* B' = */	1.f,	-0.772f,	0.f
		},
		Rec709[9] = {
			// multipliers for the corresponding signals
			//			Y'		Cb			Cr
			/* R' = */	1.f,	0.f,		1.5748f,
			/* G' = */	1.f,	-0.187324f,	-0.468124f,
			/* B' = */	1.f,	1.8556f,	0.f
		};
		switch (space) {
			// FIXME: research those constants!
			//case kPrintingDensity:
			//case kUnspecifiedVideo:
			//case kSMPTE274M:
			case kITUR709:
				return Rec709;
			case kITUR601:
			case kITUR602:	// FIXME? what's up with these two?
				return Rec601;
			//case kUserDefined:
			//case kNTSCCompositeVideo:
			//case kPALCompositeVideo:
			default:
				// ???
				return NULL;
		}
	}

	template <typename DATA, int max>
	static inline void ConvertPixelYCbCrToRGB(const DATA CbYCr[3], DATA RGB[3], const float matrix[9]) {
		float tmp;
		for (int i = 0; i < 3; i++) {
			// dot product of matrix row and YCbCr pixel vector
			// chroma must be put in the [-0.5; 0.5] range
			tmp = matrix[i * 3 + 0] * CbYCr[1]									// Y
				+ matrix[i * 3 + 1] * ((float)CbYCr[0] - 0.5f * (float)max)		// Cb
				+ matrix[i * 3 + 2] * ((float)CbYCr[2] - 0.5f * (float)max);	// Cr
			// for some reason the R and B channels get swapped, put them back in the correct order
			RGB[2 - i] = std::max((DATA)0, std::min(static_cast<DATA>(tmp), (DATA)max));
		}
	}

	// 4:4:4
	template <typename DATA, int max>
	static bool ConvertCbYCrToRGB(const Characteristic space, const DATA *input, DATA *output, const int pixels) {
		const float *matrix = GetYCbCrtoRGBColorMatrix(space);
		if (matrix == NULL)
			return false;
		DATA RGB[3];
		for (int i = 0; i < pixels; i++) {
			ConvertPixelYCbCrToRGB<DATA, max>(&input[i * 3], RGB, matrix);
			memcpy(&output[i * 3], RGB, sizeof(DATA) * 3);
		}
		return true;
	}

	// 4:4:4:4
	template <typename DATA, int max>
	static bool ConvertCbYCrAToRGBA(const Characteristic space, const DATA *input, DATA *output, const int pixels) {
		const float *matrix = GetYCbCrtoRGBColorMatrix(space);
		if (matrix == NULL)
			return false;
		DATA RGBA[4];
		for (int i = 0; i < pixels; i++) {
			ConvertPixelYCbCrToRGB<DATA, max>(&input[i * 4], RGBA, matrix);
			RGBA[3] = input[i * 4 + 3];
			memcpy(&output[i * 4], RGBA, sizeof(DATA) * 4);
		}
		return true;
	}

	// 4:2:2
	template <typename DATA, int max>
	static bool ConvertCbYCrYToRGB(const Characteristic space, const DATA *input, DATA *output, const int pixels) {
		const float *matrix = GetYCbCrtoRGBColorMatrix(space);
		if (matrix == NULL)
			return false;
		DATA CbYCr[3];
		for (int i = 0; i < pixels; i++) {
			// upsample to 4:4:4
			// FIXME: proper interpolation
			CbYCr[0] = input[(i | 1) * 2];	// Cb
			CbYCr[1] = input[i * 2 + 1];	// Y
			CbYCr[2] = input[(i & ~1) * 2];	// Cr
			// convert to RGB; we can pass a pointer into output because input must be != output
			ConvertPixelYCbCrToRGB<DATA, max>(CbYCr, &output[i * 3], matrix);
		}
		return true;
	}

	// 4:2:2:4
	template <typename DATA, int max>
	static bool ConvertCbYACrYAToRGBA(const Characteristic space, const DATA *input, DATA *output, const int pixels) {
		const float *matrix = GetYCbCrtoRGBColorMatrix(space);
		if (matrix == NULL)
			return false;
		DATA CbYCr[3];
		for (int i = 0; i < pixels; i++) {
			// upsample to 4:4:4
			// FIXME: proper interpolation
			CbYCr[0] = input[(i | 1) * 3];	// Cb
			CbYCr[1] = input[i * 3 + 1];	// Y
			CbYCr[2] = input[(i & ~1) * 3];	// Cr
			// convert to RGBA; we can pass a pointer into output because input must be != output
			ConvertPixelYCbCrToRGB<DATA, max>(CbYCr, &output[i * 4], matrix);
			output[i * 4 + 3] = input[i * 3 + 2];	// A
		}
		return true;
	}

	static inline bool ConvertToRGBInternal(const Descriptor desc, const DataSize size, const Characteristic space,
		const void *input, const void *output, const int pixels) {
		switch (desc) {
			// redundant calls
			case kRGB:
			case kRGBA:
				return true;

			// needs swapping
			case kABGR:
				switch (size) {
					case kByte:
						return ConvertABGRToRGBA<U8>((const U8 *)input, (U8 *)output, pixels);
					case kWord:
						return ConvertABGRToRGBA<U16>((const U16 *)input, (U16 *)output, pixels);
					case kInt:
						return ConvertABGRToRGBA<U32>((const U32 *)input, (U32 *)output, pixels);
					case kFloat:
						return ConvertABGRToRGBA<R32>((const R32 *)input, (R32 *)output, pixels);
					case kDouble:
						return ConvertABGRToRGBA<R64>((const R64 *)input, (R64 *)output, pixels);
				}
				// shouldn't ever get here
				return false;
				

			// FIXME: can this be translated to RGB?
			//case kCompositeVideo:
			case kCbYCrY:
				switch (size) {
					case kByte:
						return ConvertCbYCrYToRGB<U8, 0xFF>(space, (const U8 *)input, (U8 *)output, pixels);
					case kWord:
						return ConvertCbYCrYToRGB<U16, 0xFFFF>(space, (const U16 *)input, (U16 *)output, pixels);
					case kInt:
						return ConvertCbYCrYToRGB<U32, 0xFFFFFFFF>(space, (const U32 *)input, (U32 *)output, pixels);
					case kFloat:
						return ConvertCbYCrYToRGB<R32, 1>(space, (const R32 *)input, (R32 *)output, pixels);
					case kDouble:
						return ConvertCbYCrYToRGB<R64, 1>(space, (const R64 *)input, (R64 *)output, pixels);
				}
				// shouldn't ever get here
				return false;

			case kCbYCr:
				switch (size) {
					case kByte:
						return ConvertCbYCrToRGB<U8, 0xFF>(space, (const U8 *)input, (U8 *)output, pixels);
					case kWord:
						return ConvertCbYCrToRGB<U16, 0xFFFF>(space, (const U16 *)input, (U16 *)output, pixels);
					case kInt:
						return ConvertCbYCrToRGB<U32, 0xFFFFFFFF>(space, (const U32 *)input, (U32 *)output, pixels);
					case kFloat:
						return ConvertCbYCrToRGB<R32, 1>(space, (const R32 *)input, (R32 *)output, pixels);
					case kDouble:
						return ConvertCbYCrToRGB<R64, 1>(space, (const R64 *)input, (R64 *)output, pixels);
				}
				// shouldn't ever get here
				return false;

			case kCbYACrYA:
				switch (size) {
					case kByte:
						return ConvertCbYACrYAToRGBA<U8, 0xFF>(space, (const U8 *)input, (U8 *)output, pixels);
					case kWord:
						return ConvertCbYACrYAToRGBA<U16, 0xFFFF>(space, (const U16 *)input, (U16 *)output, pixels);
					case kInt:
						return ConvertCbYACrYAToRGBA<U32, 0xFFFFFFFF>(space, (const U32 *)input, (U32 *)output, pixels);
					case kFloat:
						return ConvertCbYACrYAToRGBA<R32, 1>(space, (const R32 *)input, (R32 *)output, pixels);
					case kDouble:
						return ConvertCbYACrYAToRGBA<R64, 1>(space, (const R64 *)input, (R64 *)output, pixels);
				}
				// shouldn't ever get here
				return false;

			case kCbYCrA:
				switch (size) {
					case kByte:
						return ConvertCbYCrAToRGBA<U8, 0xFF>(space, (const U8 *)input, (U8 *)output, pixels);
					case kWord:
						return ConvertCbYCrAToRGBA<U16, 0xFFFF>(space, (const U16 *)input, (U16 *)output, pixels);
					case kInt:
						return ConvertCbYCrAToRGBA<U32, 0xFFFFFFFF>(space, (const U32 *)input, (U32 *)output, pixels);
					case kFloat:
						return ConvertCbYCrAToRGBA<R32, 1>(space, (const R32 *)input, (R32 *)output, pixels);
					case kDouble:
						return ConvertCbYCrAToRGBA<R64, 1>(space, (const R64 *)input, (R64 *)output, pixels);
				}
				// shouldn't ever get here
				return false;

			// all the rest is either irrelevant, invalid or unsupported
			/*case kUserDefinedDescriptor:
			case kRed:
			case kGreen:
			case kBlue:
			case kAlpha:
			case kLuma:
			case kColorDifference:
			case kDepth:
			case kUserDefined2Comp:
			case kUserDefined3Comp:
			case kUserDefined4Comp:
			case kUserDefined5Comp:
			case kUserDefined6Comp:
			case kUserDefined7Comp:
			case kUserDefined8Comp:
			case kUndefinedDescriptor:*/
			default:
				return false;
		}
	}

	static inline int QueryBufferSizeInternal(const Descriptor desc, const int pixels, const int bytes) {
		switch (desc) {
			//case kCompositeVideo:	// FIXME: can this be translated to RGB?
			case kCbYCrY:	// 4:2:2 -> RGB, requires allocation
				return pixels * 3 * bytes;
			case kCbYCr:	// 4:4:4 -> RGB, can get away with sviweling
			case kRGB:		// redundant
				return pixels * -3 * bytes;
				
			case kCbYACrYA:	// 4:2:2:4 -> RGB, requires allocation
				return pixels * 4 * bytes;
			case kCbYCrA:	// 4:4:4:4 -> RGBA, can get away with sviweling
			case kRGBA:		// redundant
			case kABGR:		// only needs swapping
				return pixels * -4 * bytes;
				
			// all the rest is either irrelevant, invalid or unsupported
			/*case kUserDefinedDescriptor:
			case kRed:
			case kGreen:
			case kBlue:
			case kAlpha:
			case kLuma:
			case kColorDifference:
			case kDepth:
			case kUserDefined2Comp:
			case kUserDefined3Comp:
			case kUserDefined4Comp:
			case kUserDefined5Comp:
			case kUserDefined6Comp:
			case kUserDefined7Comp:
			case kUserDefined8Comp:
			case kUndefinedDescriptor:*/
			default:
				return 0;
		}
	}

	int QueryBufferSize(const Header &header, const int element, const Block &block) {
		return QueryBufferSizeInternal(header.ImageDescriptor(element),
			(block.x2 - block.x1 + 1) * (block.y2 - block.y1 + 1),
			header.ComponentByteCount(element));
	}
	
	int QueryBufferSize(const Header &header, const int element) {
		return QueryBufferSizeInternal(header.ImageDescriptor(element),
			header.Width() * header.Height(),
			header.ComponentByteCount(element));
	}
	
	bool ConvertToRGB(const Header &header, const int element, const void *input, const void *output, const Block &block) {
		return ConvertToRGBInternal(header.ImageDescriptor(element),
			header.ComponentDataSize(element), header.Colorimetric(element),
			input, output, (block.x2 - block.x1 + 1) * (block.y2 - block.y1 + 1));
	}
	
	bool ConvertToRGB(const Header &header, const int element, const void *input, const void *output) {
		return ConvertToRGBInternal(header.ImageDescriptor(element),
			header.ComponentDataSize(element), header.Colorimetric(element),
			input, output, header.Width() * header.Height());
	}
}
