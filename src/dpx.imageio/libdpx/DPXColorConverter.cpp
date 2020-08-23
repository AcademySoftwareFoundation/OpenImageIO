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
	static inline bool SwapRGBABytes(const DATA *input, DATA *output, int pixels) {
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
	
	// ========================================================================
	// native formats -> RGB conversion
	// ========================================================================

	static inline const float *GetYCbCrToRGBColorMatrix(const Characteristic space) {
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
			case kITUR709:
			case kSMPTE274M:	// SMPTE 247M has the same chromaticities as Rec709
				return Rec709;
			case kITUR601:
			case kITUR602:
				return Rec601;
			//case kUserDefined:
			//case kNTSCCompositeVideo:
			//case kPALCompositeVideo:
			default:
				// ???
				return NULL;
		}
	}

	template <typename DATA, unsigned int max>
	static inline void ConvertPixelYCbCrToRGB(const DATA CbYCr[3], DATA RGB[3], const float matrix[9]) {
		float tmp;
		for (int i = 0; i < 3; i++) {
			// dot product of matrix row and YCbCr pixel vector
			// chroma must be put in the [-0.5; 0.5] range
			tmp = matrix[i * 3 + 0] * CbYCr[1]									// Y
				+ matrix[i * 3 + 1] * ((float)CbYCr[0] - 0.5f * (float)max)		// Cb
				+ matrix[i * 3 + 2] * ((float)CbYCr[2] - 0.5f * (float)max);	// Cr
			// for some reason the R and B channels get swapped, put them back in the correct order
			// prevent overflow
			RGB[2 - i] = std::max((DATA)0, static_cast<DATA>(std::min(tmp, (float)max)));
		}
	}

	// 4:4:4
	template <typename DATA, unsigned int max>
	static bool ConvertCbYCrToRGB(const Characteristic space, const DATA *input, DATA *output, const int pixels) {
		const float *matrix = GetYCbCrToRGBColorMatrix(space);
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
	template <typename DATA, unsigned int max>
	static bool ConvertCbYCrAToRGBA(const Characteristic space, const DATA *input, DATA *output, const int pixels) {
		const float *matrix = GetYCbCrToRGBColorMatrix(space);
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
	template <typename DATA, unsigned int max>
	static bool ConvertCbYCrYToRGB(const Characteristic space, const DATA *input, DATA *output, const int pixels) {
		const float *matrix = GetYCbCrToRGBColorMatrix(space);
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
	template <typename DATA, unsigned int max>
	static bool ConvertCbYACrYAToRGBA(const Characteristic space, const DATA *input, DATA *output, const int pixels) {
		const float *matrix = GetYCbCrToRGBColorMatrix(space);
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
		const void *input, void *output, const int pixels) {
		switch (desc) {
			// redundant calls
			case kRGB:
			case kRGBA:
				return true;

			// needs swapping
			case kABGR:
				switch (size) {
					case kByte:
						return SwapRGBABytes<U8>((const U8 *)input, (U8 *)output, pixels);
					case kWord:
						return SwapRGBABytes<U16>((const U16 *)input, (U16 *)output, pixels);
					case kInt:
						return SwapRGBABytes<U32>((const U32 *)input, (U32 *)output, pixels);
					case kFloat:
						return SwapRGBABytes<R32>((const R32 *)input, (R32 *)output, pixels);
					case kDouble:
						return SwapRGBABytes<R64>((const R64 *)input, (R64 *)output, pixels);
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

	static inline int QueryRGBBufferSizeInternal(const Descriptor desc, const int pixels, const int bytes) {
		switch (desc) {
			//case kCompositeVideo:	// FIXME: can this be translated to RGB?
			case kCbYCrY:	// 4:2:2 -> RGB, requires allocation
				return pixels * 3 * bytes;
				
			case kCbYCr:	// 4:4:4 -> RGB, can get away with swiveling
			case kRGB:		// redundant
				return pixels * -3 * bytes;
				
			case kCbYACrYA:	// 4:2:2:4 -> RGBA, requires allocation
				return pixels * 4 * bytes;
				
			case kCbYCrA:	// 4:4:4:4 -> RGBA, can get away with swiveling
			case kRGBA:		// redundant
			case kABGR:		// only needs swapping
				return pixels * -4 * bytes;
				
            // Added by lg, does this work?
            case kRed:
            case kGreen:
            case kBlue:
            case kAlpha:
            case kLuma:
            case kDepth:
                return pixels * 1 * bytes;
			// all the rest is either irrelevant, invalid or unsupported
			/*case kUserDefinedDescriptor:
			case kColorDifference:
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

	int QueryRGBBufferSize(const Header &header, const int element, const Block &block) {
		return QueryRGBBufferSizeInternal(header.ImageDescriptor(element),
			(block.x2 - block.x1 + 1) * (block.y2 - block.y1 + 1),
			header.ComponentByteCount(element));
	}

	int QueryRGBBufferSize(const Header &header, const int element) {
		return QueryRGBBufferSizeInternal(header.ImageDescriptor(element),
			header.Width() * header.Height(),
			header.ComponentByteCount(element));
	}

	bool ConvertToRGB(const Header &header, const int element, const void *input, void *output, const Block &block) {
		return ConvertToRGBInternal(header.ImageDescriptor(element),
			header.ComponentDataSize(element), header.Colorimetric(element),
			input, output, (block.x2 - block.x1 + 1) * (block.y2 - block.y1 + 1));
	}

	bool ConvertToRGB(const Header &header, const int element, const void *input, void *output) {
		return ConvertToRGBInternal(header.ImageDescriptor(element),
			header.ComponentDataSize(element), header.Colorimetric(element),
			input, output, header.Width() * header.Height());
	}

	// ========================================================================
	// RGB -> native formats conversion
	// ========================================================================

	static inline const float *GetRGBToYCbCrColorMatrix(const Characteristic space) {
		// RGB -> YCbCr matrices
		static const float Rec601[9] = {
			// multipliers for the corresponding signals
			//			R'		G'			B'
			/* Cb = */	-0.168736f,	-0.331264f,	0.5f,
			/* Y' = */	0.299f,		0.587f,		0.114f,
			/* Cr = */	0.5f,		-0.418688f,	-0.081312f
		},
		Rec709[9] = {
			// multipliers for the corresponding signals
			//			R'			G'			B'
			/* Cb = */	-0.114572f,	-0.385428f,	0.5f,
			/* Y' = */	0.2126f,	0.7152f,	0.0722f,
			/* Cr = */	0.5f,		-0.454153f,	-0.045847f
		};
		switch (space) {
			// FIXME: research those constants!
			//case kPrintingDensity:
			//case kUnspecifiedVideo:
			case kITUR709:
			case kSMPTE274M:	// SMPTE 247M has the same chromaticities as Rec709
				return Rec709;
			case kITUR601:
			case kITUR602:
				return Rec601;
			//case kUserDefined:
			//case kNTSCCompositeVideo:
			//case kPALCompositeVideo:
			default:
				// ???
				return NULL;
		}
	}

	template <typename DATA, unsigned int max>
	static inline void ConvertPixelRGBToYCbCr(const DATA RGB[3], DATA CbYCr[3], const float matrix[9]) {
		float tmp;
		for (int i = 0; i < 3; i++) {
			// dot product of matrix row and RGB pixel vector
			tmp = matrix[i * 3 + 0] * RGB[0]
				+ matrix[i * 3 + 1] * RGB[1]
				+ matrix[i * 3 + 2] * RGB[2];
			// chroma (indices 0 and 2) must be put in the [0; 1] range
			if (i != 1)
				tmp += 0.5f * (float)max;
			// prevent overflow
			CbYCr[i] = std::max((DATA)0, static_cast<DATA>(std::min(tmp, (float)max)));
		}
	}

	// 4:4:4
	template <typename DATA, unsigned int max>
	static bool ConvertRGBToCbYCr(const Characteristic space, const DATA *input, DATA *output, const int pixels) {
		const float *matrix = GetRGBToYCbCrColorMatrix(space);
		if (matrix == NULL)
			return false;
		DATA CbYCr[3];
		for (int i = 0; i < pixels; i++) {
			ConvertPixelRGBToYCbCr<DATA, max>(&input[i * 3], CbYCr, matrix);
			memcpy(&output[i * 3], CbYCr, sizeof(DATA) * 3);
		}
		return true;
	}

	// 4:4:4:4
	template <typename DATA, unsigned int max>
	static bool ConvertRGBAToCbYCrA(const Characteristic space, const DATA *input, DATA *output, const int pixels) {
		const float *matrix = GetRGBToYCbCrColorMatrix(space);
		if (matrix == NULL)
			return false;
		DATA CbYCrA[4];
		for (int i = 0; i < pixels; i++) {
			ConvertPixelRGBToYCbCr<DATA, max>(&input[i * 4], CbYCrA, matrix);
			CbYCrA[3] = input[i * 4 + 3];
			memcpy(&output[i * 4], CbYCrA, sizeof(DATA) * 4);
		}
		return true;
	}

	// 4:2:2
	template <typename DATA, unsigned int max>
	static bool ConvertRGBToCbYCrY(const Characteristic space, const DATA *input, DATA *output, const int pixels) {
		const float *matrix = GetRGBToYCbCrColorMatrix(space);
		if (matrix == NULL)
			return false;
		DATA CbYCr[3];
		for (int i = 0; i < pixels; i++) {
			// convert to YCbCr
			ConvertPixelRGBToYCbCr<DATA, max>(&input[i * 3], CbYCr, matrix);
			// downsample to 4:2:2
			// FIXME: proper downsampling
			output[i * 2 + 0] = (i & 1) == 0 ? CbYCr[0] : CbYCr[2];
			output[i * 2 + 1] = CbYCr[1];
		}
		return true;
	}

	// 4:2:2:4
	template <typename DATA, unsigned int max>
	static bool ConvertRGBAToCbYACrYA(const Characteristic space, const DATA *input, DATA *output, const int pixels) {
		const float *matrix = GetRGBToYCbCrColorMatrix(space);
		if (matrix == NULL)
			return false;
		DATA CbYCr[3];
		for (int i = 0; i < pixels; i++) {
			// convert to YCbCr
			ConvertPixelRGBToYCbCr<DATA, max>(&input[i * 4], CbYCr, matrix);
			// downsample to 4:2:2
			// FIXME: proper downsampling
			output[i * 3 + 0] = (i & 1) == 0 ? CbYCr[0] : CbYCr[2];
			output[i * 3 + 1] = CbYCr[1];
			output[i * 3 + 3] = input[i * 4 + 3];
		}
		return true;
	}

	static inline bool ConvertToNativeInternal(const Descriptor desc, const DataSize size, const Characteristic space,
		const void *input, void *output, const int pixels) {
		switch (desc) {
			// redundant calls
			case kRGB:
			case kRGBA:
				return true;

			// needs swapping
			case kABGR:
				switch (size) {
					case kByte:
						return SwapRGBABytes<U8>((const U8 *)input, (U8 *)output, pixels);
					case kWord:
						return SwapRGBABytes<U16>((const U16 *)input, (U16 *)output, pixels);
					case kInt:
						return SwapRGBABytes<U32>((const U32 *)input, (U32 *)output, pixels);
					case kFloat:
						return SwapRGBABytes<R32>((const R32 *)input, (R32 *)output, pixels);
					case kDouble:
						return SwapRGBABytes<R64>((const R64 *)input, (R64 *)output, pixels);
				}
				// shouldn't ever get here
				return false;

			case kCbYCrY:
				switch (size) {
					case kByte:
						return ConvertRGBToCbYCrY<U8, 0xFF>(space, (const U8 *)input, (U8 *)output, pixels);
					case kWord:
						return ConvertRGBToCbYCrY<U16, 0xFFFF>(space, (const U16 *)input, (U16 *)output, pixels);
					case kInt:
						return ConvertRGBToCbYCrY<U32, 0xFFFFFFFF>(space, (const U32 *)input, (U32 *)output, pixels);
					case kFloat:
						return ConvertRGBToCbYCrY<R32, 1>(space, (const R32 *)input, (R32 *)output, pixels);
					case kDouble:
						return ConvertRGBToCbYCrY<R64, 1>(space, (const R64 *)input, (R64 *)output, pixels);
				}
				// shouldn't ever get here
				return false;

			case kCbYCr:
				switch (size) {
					case kByte:
						return ConvertRGBToCbYCr<U8, 0xFF>(space, (const U8 *)input, (U8 *)output, pixels);
					case kWord:
						return ConvertRGBToCbYCr<U16, 0xFFFF>(space, (const U16 *)input, (U16 *)output, pixels);
					case kInt:
						return ConvertRGBToCbYCr<U32, 0xFFFFFFFF>(space, (const U32 *)input, (U32 *)output, pixels);
					case kFloat:
						return ConvertRGBToCbYCr<R32, 1>(space, (const R32 *)input, (R32 *)output, pixels);
					case kDouble:
						return ConvertRGBToCbYCr<R64, 1>(space, (const R64 *)input, (R64 *)output, pixels);
				}
				// shouldn't ever get here
				return false;

			case kCbYACrYA:
				switch (size) {
					case kByte:
						return ConvertRGBAToCbYACrYA<U8, 0xFF>(space, (const U8 *)input, (U8 *)output, pixels);
					case kWord:
						return ConvertRGBAToCbYACrYA<U16, 0xFFFF>(space, (const U16 *)input, (U16 *)output, pixels);
					case kInt:
						return ConvertRGBAToCbYACrYA<U32, 0xFFFFFFFF>(space, (const U32 *)input, (U32 *)output, pixels);
					case kFloat:
						return ConvertRGBAToCbYACrYA<R32, 1>(space, (const R32 *)input, (R32 *)output, pixels);
					case kDouble:
						return ConvertRGBAToCbYACrYA<R64, 1>(space, (const R64 *)input, (R64 *)output, pixels);
				}
				// shouldn't ever get here
				return false;

			case kCbYCrA:
				switch (size) {
					case kByte:
						return ConvertRGBAToCbYCrA<U8, 0xFF>(space, (const U8 *)input, (U8 *)output, pixels);
					case kWord:
						return ConvertRGBAToCbYCrA<U16, 0xFFFF>(space, (const U16 *)input, (U16 *)output, pixels);
					case kInt:
						return ConvertRGBAToCbYCrA<U32, 0xFFFFFFFF>(space, (const U32 *)input, (U32 *)output, pixels);
					case kFloat:
						return ConvertRGBAToCbYCrA<R32, 1>(space, (const R32 *)input, (R32 *)output, pixels);
					case kDouble:
						return ConvertRGBAToCbYCrA<R64, 1>(space, (const R64 *)input, (R64 *)output, pixels);
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
			case kCompositeVideo:
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

	static inline int QueryNativeBufferSizeInternal(const Descriptor desc, const int pixels, const DataSize compSize) {
		int bytes = compSize == kByte ? 1 : compSize == kWord ? 2 : compSize == kDouble ? 8 : 4;
		switch (desc) {			
			case kCbYCrY:	// RGB -> 4:2:2, requires allocation
				return pixels * 2 * bytes;
			
			case kCbYCr:	// RGB -> 4:4:4, can get away with swiveling
			case kRGB:		// redundant
				return pixels * -3 * bytes;
			
			case kCbYACrYA:	// RGBA -> 4:2:2:4, requires allocation
				return pixels * 4 * bytes;
			
			case kCbYCrA:	// RGBA -> 4:4:4:4, can get away with swiveling
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
			case kCompositeVideo:
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
	
	int QueryNativeBufferSize(const Descriptor desc, const DataSize compSize, const Block &block) {
		return QueryNativeBufferSizeInternal(desc, (block.x2 - block.x1 + 1) * (block.y2 - block.y1 + 1), compSize);
	}
	
	int QueryNativeBufferSize(const Descriptor desc, const DataSize compSize, const int width, const int height) {
		return QueryNativeBufferSizeInternal(desc, width * height, compSize);
	}
	
	bool ConvertToNative(const Descriptor desc, const DataSize compSize, const Characteristic cmetr, const void *input, void *output, const Block &block) {
		return ConvertToNativeInternal(desc, compSize, cmetr, input, output, (block.x2 - block.x1 + 1) * (block.y2 - block.y1 + 1));
	}
	
	bool ConvertToNative(const Descriptor desc, const DataSize compSize, const Characteristic cmetr, const int width, const int height, const void *input, void *output) {
		return ConvertToNativeInternal(desc, compSize, cmetr, input, output, width * height);
	}
}
