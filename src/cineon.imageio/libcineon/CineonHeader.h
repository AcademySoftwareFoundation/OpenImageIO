// -*- mode: C++; tab-width: 4 -*-
// vi: ts=4

/*! \file CineonHeader.h */

/*
 * Copyright (c) 2010, Patrick A. Palmer and Leszek Godlewski.
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


// Cineon graphic file format v4.5


#ifndef _CINEON_CINEONHEADER_H
#define _CINEON_CINEONHEADER_H 1

#include <cstring>
#include <limits>
#include <OpenImageIO/strutil.h>

#if defined(_MSC_VER) && _MSC_VER < 1600
   typedef __int32 int32_t;
   typedef unsigned __int32 uint32_t;
   typedef __int64 int64_t;
   typedef unsigned __int64 uint64_t;
#else
# include <cstdint>
#endif

#include "CineonStream.h"



/*!
 * \def SPEC_VERSION
 * \brief Cineon format version
 */
#define SPEC_VERSION		"V4.5"

/*!
 * \def MAX_ELEMENTS
 * \brief Maximum number of image elements
 */
#define MAX_ELEMENTS		8

/*!
 * \def MAX_COMPONENTS
 * \brief Maximum number of components per image element
 */
#define MAX_COMPONENTS		8


/*!
 * \def MAGIC_COOKIE
 * \brief HEX value of 0x802A5FD7
 */
#define MAGIC_COOKIE		0x802A5FD7




namespace cineon
{


	// DPX data types

	/*!
	 * \typedef unsigned char U8
	 * \brief Unsigned 8 bit integer
	 */
	typedef unsigned char	U8;

	/*!
	 * \typedef unsigned short U16
	 * \brief Unsigned 16 bit integer
	 */
	typedef unsigned short	U16;

	/*!
	 * \typedef unsigned int U32
	 * \brief Unsigned 32 bit integer
	 */
	typedef unsigned int	U32;

	/*!
	 * \typedef signed char U32
	 * \brief Signed 32 bit integer
	 */
	typedef signed int		S32;

	/*!
	 * \typedef unsigned long long U64
	 * \brief Unsigned 64 bit integer
	 */
	typedef uint64_t		U64;

	/*!
	 * \typedef float R32
	 * \brief 32 bit floating point number
	 */
	typedef float			R32;

	/*!
	 * \typedef float R64
	 * \brief 64 bit floating point number
	 */
	typedef double			R64;

	/*!
	 * \typedef char ASCII
	 * \brief ASCII character
	 */
	typedef char			ASCII;


	/*!
	 * \enum DataSize
	 * \brief Component Data Storage Data Type
	 */
	enum DataSize
	{
		kByte,											//!< 8-bit size component
		kWord,											//!<
		kInt,											//!<
		kLongLong										//!< 64-bit integer
	};


	/*!
	 * \enum Orientation
	 * \brief Image Orientation Code
	 */
	enum Orientation
	{
		kLeftToRightTopToBottom = 0,					//!< Oriented left to right, top to bottom
		kRightToLeftTopToBottom = 1,					//!< Oriented right to left, top to bottom
		kLeftToRightBottomToTop = 2,					//!< Oriented left to right, bottom to top
		kRightToLeftBottomToTop = 3,					//!< Oriented right to left, bottom to top
		kTopToBottomLeftToRight = 4,					//!< Oriented top to bottom, left to right
		kTopToBottomRightToLeft = 5,					//!< Oriented top to bottom, right to left
		kBottomToTopLeftToRight = 6,					//!< Oriented bottom to top, left to right
		kBottomToTopRightToLeft = 7,					//!< Oriented bottom to top, right to left
		kUndefinedOrientation = 0xff					//!< Undefined orientation
	};


	/*!
	 * \enum Descriptor
	 * \brief Image element Descriptor (second byte)
	 */
	enum Descriptor
	{
		kGrayscale = 0,									//!< Grayscale
		kPrintingDensityRed = 1,						//!< Red
		kPrintingDensityGreen = 2,						//!< Green
		kPrintingDensityBlue = 3,						//!< Blue
		kRec709Red = 4,									//!< Red
		kRec709Green = 5,								//!< Green
		kRec709Blue = 6,								//!< Blue
		kUndefinedDescriptor = 0xff						//!< Undefined descriptor
	};


	/*!
	 * \enum Interleave
	 * \brief Component interleaving method
	 */
	enum Interleave
	{
		kPixel = 0,										//!< Pixel interleave (rgbrgbrgb...)
		kLine = 1,										//!< Line interleave (rrr.ggg.bbb.rrr.ggg.bbb.)
		kChannel = 2									//!< Channel interleave (rrr..ggg..bbb..)
	};


	/*!
	 * \enum Packing
	 * \brief Component data packing method
	 */
	enum Packing
	{
		kPacked = 0,									//!< Use all bits (tight packing)
		kByteLeft = 1,									//!< Byte (8-bit) boundary, left justified
		kByteRight = 2,									//!< Byte (8-bit) boundary, right justified
		kWordLeft = 3,									//!< Word (16-bit) boundary, left justified
		kWordRight = 4,									//!< Word (16-bit) boundary, right justified
		kLongWordLeft = 5,								//!< Longword (32-bit) boundary, left justified
		kLongWordRight = 6,								//!< Longword (32-bit) boundary, right justified
		kPackAsManyAsPossible = 0x80					//!< Bitflag - if present, pack as many fields as possible per cell, only one otherwise
	};


	/*!
	 * \struct ImageElement
	 * \brief Data Structure for Image Element
	 */
	struct ImageElement
	{
		U8					designator[2];				//!< Channel descriptor \see Descriptor
		U8					bitDepth;					//!< Bits per pixel
		U8					unused1;					//!< Unused
		U32					pixelsPerLine;				//!< Pixels per line
		U32					linesPerElement;			//!< Lines per element
		R32					lowData;					//!< Reference low data code value
		R32					lowQuantity;				//!< Reference low quantity represented
		R32					highData;					//!< Reference high data code value
		R32					highQuantity;				//!< Reference high quantity represented

		/*!
		 * \brief Constructor
		 */
							ImageElement();
	};




	/*!
	 * \struct GenericHeader
	 * \brief Generic File and Image Header Information
	 */
	struct GenericHeader
	{
		/*!
		 * \name File Information Members
		 */
		 //@{
		U32					magicNumber;				//!< Indicates start of DPX image file and is used to determine byte order.
		U32					imageOffset;				//!< Offset to image data (in bytes)
		U32					genericSize;				//!< Generic Header length (in bytes)
		U32					industrySize;				//!< Industry Header length (in bytes)
		U32					userSize;					//!< User defined header length (in bytes)
		U32					fileSize;					//!< Total file size (in bytes)
		ASCII				version[8];					//!< Version number of header format
		ASCII				fileName[100];				//!< File name
		ASCII				creationDate[12];			//!< Create date /see DateTimeFormat
		ASCII				creationTime[12];			//!< Create time /see DateTimeFormat
		ASCII				reserved1[36];				//!< Reserved
		/* end of group */
		//@}


		/*!
		 * \name Image Information Members
		 */
		 //@{
		U8					imageOrientation;			//!< Image orientation \see Orientation
		U8					numberOfElements;			//!< Number of elements (1-8)
		U8					unused1[2];					//!< Unused (word alignment)
		ImageElement		chan[MAX_ELEMENTS];			//!< Image element data structures
		R32					whitePoint[2];				//!< White point (x, y pair)
		R32					redPrimary[2];				//!< Red primary chromaticity (x, y pair)
		R32					greenPrimary[2];			//!< Green primary chromaticity (x, y pair)
		R32					bluePrimary[2];				//!< Blue primary chromaticity (x, y pair)
		ASCII				labelText[200];				//!< Label text
		ASCII				reserved2[28];				//!< Reserved
		U8					interleave;					//!< Data interleave \see Interleave
		U8					packing;					//!< Packing \see Packing
		U8					dataSign;					//!< Data sign (0 = unsigned, 1 = signed)
		U8					imageSense;					//!< Image sense (0 = positive image, 1 = negative image)
		U32					endOfLinePadding;			//!< End-of-Line Padding
		U32					endOfImagePadding;			//!< End-of-Image Padding
		ASCII				reserved3[20];
		/* end of group */
		//@}


		/*!
		 * \name Image Origination Members
		 */
		 //@{
		S32					xOffset;					//!< X offset
		S32					yOffset;					//!< Y offset
		ASCII				sourceImageFileName[100];	//!< Source image file name
		ASCII				sourceDate[12];				//!< Source date /see DateTimeFormat
		ASCII				sourceTime[12];				//!< Source time /see DateTimeFormat
		ASCII				inputDevice[64];			//!< Input device name
		ASCII				inputDeviceModelNumber[32];	//!< Input device model number
		ASCII				inputDeviceSerialNumber[32];	//!< Input device serial number
		R32					xDevicePitch;				//!< X device pitch (samples/mm)
		R32					yDevicePitch;				//!< Y device pitch (samples/mm)
		R32					gamma;						//!< Gamma
		ASCII				reserved4[40];				//!< Reserved
		/* end of group */
		//@}

		/*!
		 * \brief Constructor
		 */
							GenericHeader();

		/*!
		 * \brief Reset class to initial state
		 */
		void				Reset();



		/*!
		 * \name File Information Methods
		 */
		 //@{

		/*!
		 * \brief Get magic number, used for byte ordering identification
		 * \return magic number
		 */
		inline U32			MagicNumber() const;

		/*!
		 * \brief Get the offset in bytes to the start of the first image element
		 * \return offset
		 */
		inline U32			ImageOffset() const;

		/*!
		 * \brief Set the offset in bytes to the start of the first image element
		 * \param offset offset in bytes
		 */
		inline void			SetImageOffset(const U32 offset);

		/*!
		 * \brief Get the size of the generic section within the header
		 * \return generic header size in bytes
		 */
		inline U32			GenericSize() const;

		/*!
		 * \brief Get the size of the industry section within the header
		 * \return industry header size in bytes
		 */
		inline U32			IndustrySize() const;

		/*!
		 * \brief Get the size of the user data
		 * \return user data size in bytes
		 */
		inline U32			UserSize() const;

		/*!
		 * \brief Set the size of the user data
		 * \param size user data size in bytes
		 */
		inline void 		SetUserSize(const U32 size);

		/*!
		 * \brief Get the size of the entire file
		 * \return file size in bytes
		 */
		inline U32			FileSize() const;

		/*!
		 * \brief Set the size of the entire file
		 * \param fs file size in bytes
		 */
		inline void 		SetFileSize(const U32 fs);

		/*!
		 * \brief Get current version string of header
		 * \param v buffer to place string, needs to be at least 8+1 bytes long
		 */
		inline void			Version(char *v) const;

		/*!
		 * \brief Set the version string
		 * \param v version string
		 */
		inline void 		SetVersion(const char *v);


		/*!
		 * \brief Get the file name
		 * \param fn buffer to store filename (100+1 chars)
		 */
		inline void			FileName(char *fn) const;

		/*!
		 * \brief Set the file name
		 * \param fn buffer with filename
		 */
		inline void 		SetFileName(const char *fn);

		/*!
		 * \brief Get the creation time/date
		 * \param ct buffer to store creation time/date (24+1 chars)
		 */
		inline void			CreationDate(char *ct) const;

		/*!
		 * \brief Set the creation time/date
		 * \param ct buffer with creation time/date
		 */
		inline void 		SetCreationDate(const char *ct);

		/*!
		 * \brief Get the creation time/date
		 * \param ct buffer to store creation time/date (24+1 chars)
		 */
		inline void			CreationTime(char *ct) const;

		/*!
		 * \brief Set the creation time/date
		 * \param ct buffer with creation time/date
		 */
		inline void 		SetCreationTime(const char *ct);

		/*!
		 * \brief Set the creation time/date
		 * \param secs number of seconds since January 1, 1970 00:00
		 */
		void				SetCreationTimeDate(const long secs);

		/* end of group */
		//@}


		/*!
		 * \name Image Information Methods
		 */
		 //@{

		/*!
		 * \brief Get the image orientation
		 * \return orientation enum
		 */
		inline Orientation	ImageOrientation() const;

		/*!
		 * \brief Set the image orientation
		 * \param orient orientation
		 */
		inline void			SetImageOrientation(const Orientation orient);

		/*!
		 * \brief Get the number of elements
		 * \return element count
		 */
		inline U8			NumberOfElements() const;

		/*!
		 * \brief Set the number of elements
		 * \param num element count
		 */
		inline void			SetNumberOfElements(const U8 num);

		/*!
		 * \brief Get the first byte of the channel designator - metric info
		 * \param i element index (0-7)
		 * \return 0 = universal metric, 1-254 = vendor-specific
		 */
		inline U8			Metric(const int i) const;

		/*!
		 * \brief Set the first byte of the channel designator - metric info
		 * \param i element index (0-7)
		 * \param ppl metric
		 */
		inline void			SetMetric(const int i, const U8 m);

		/*!
		 * \brief Get the second byte of the channel designator
		 * \param i element index (0-7)
		 * \return channel descriptor
		 */
		inline Descriptor	ImageDescriptor(const int i) const;

		/*!
		 * \brief Set the second byte of the channel designator
		 * \param i element index (0-7)
		 * \param d channel descriptor
		 */
		inline void			SetImageDescriptor(const int i, const Descriptor d);

		/*!
		 * \brief Get the bits per pixel
		 * \param i element index (0-7)
		 * \return bit count
		 */
		inline U8			BitDepth(const int i) const;

		/*!
		 * \brief Set the bits per pixel
		 * \param i element index (0-7)
		 * \param bpp bit count
		 */
		inline void			SetBitDepth(const int i, const U8 bpp);

		/*!
		 * \brief Get the pixels per line
		 * \param i element index (0-7)
		 * \return pixel count
		 */
		inline U32			PixelsPerLine(const int i) const;

		/*!
		 * \brief Set the pixels per line
		 * \param i element index (0-7)
		 * \param ppl pixel count
		 */
		inline void			SetPixelsPerLine(const int i, const U32 ppl);

		/*!
		 * \brief Get the lines per element
		 * \param i element index (0-7)
		 * \return lines count
		 */
		inline U32			LinesPerElement(const int i) const;

		/*!
		 * \brief Set the lines per element
		 * \param i element index (0-7)
		 * \param lpe lines count
		 */
		inline void			SetLinesPerElement(const int i, const U32 lpe);

		/*!
		 * \brief Get the minimum data value
		 * \param i element index (0-7)
		 * \return minimum value
		 */
		inline R32			LowData(const int i) const;

		/*!
		 * \brief Set the minimum data value
		 * \param i element index (0-7)
		 * \param data minimum value
		 */
		inline void			SetLowData(const int i, const R32 data);

		/*!
		 * \brief Get the quantity of minimum data value
		 * \param i element index (0-7)
		 * \return quantity
		 */
		inline R32			LowQuantity(const int i) const;

		/*!
		 * \brief Set the quantity of minimum data value
		 * \param i element index (0-7)
		 * \param quant quantity
		 */
		inline void			SetLowQuantity(const int i, const R32 quant);

		/*!
		 * \brief Get the maximum data value
		 * \param i element index (0-7)
		 * \return maximum value
		 */
		inline R32			HighData(const int i) const;

		/*!
		 * \brief Set the maximum data value
		 * \param i element index (0-7)
		 * \param data maximum value
		 */
		inline void			SetHighData(const int i, const R32 data);

		/*!
		 * \brief Get the quantity of maximum data value
		 * \param i element index (0-7)
		 * \return quantity
		 */
		inline R32			HighQuantity(const int i) const;

		/*!
		 * \brief Set the quantity of maximum data value
		 * \param i element index (0-7)
		 * \param quant quantity
		 */
		inline void			SetHighQuantity(const int i, const R32 quant);

		/*!
		 * \brief Get the white point primary x, y pair
		 * \param xy buffer to store the x, y pair (2 32-bit floats)
		 */
		inline void			WhitePoint(R32 xy[2]) const;

		/*!
		 * \brief Set the white point primary x, y pair
		 * \param xy the x, y pair (2 32-bit floats)
		 */
		inline void			SetWhitePoint(const R32 xy[2]);

		/*!
		 * \brief Get the red primary x, y pair
		 * \param xy buffer to store the x, y pair (2 32-bit floats)
		 */
		inline void			RedPrimary(R32 xy[2]) const;

		/*!
		 * \brief Set the red primary x, y pair
		 * \param xy the x, y pair (2 32-bit floats)
		 */
		inline void			SetRedPrimary(const R32 xy[2]);

		/*!
		 * \brief Get the green primary x, y pair
		 * \param xy buffer to store the x, y pair (2 32-bit floats)
		 */
		inline void			GreenPrimary(R32 xy[2]) const;

		/*!
		 * \brief Set the green primary x, y pair
		 * \param xy the x, y pair (2 32-bit floats)
		 */
		inline void			SetGreenPrimary(const R32 xy[2]);

		/*!
		 * \brief Get the blue primary x, y pair
		 * \param xy buffer to store the x, y pair (2 32-bit floats)
		 */
		inline void			BluePrimary(R32 xy[2]) const;

		/*!
		 * \brief Set the blue primary x, y pair
		 * \param xy the x, y pair (2 32-bit floats)
		 */
		inline void			SetBluePrimary(const R32 xy[2]);

		/*!
		 * \brief Get the label text
		 * \param ct buffer to store label text (200 chars)
		 */
		inline void			LabelText(char *ct) const;

		/*!
		 * \brief Set the label text
		 * \param ct buffer with label text
		 */
		inline void 		SetLabelText(const char *ct);

		/*!
		 * \brief Get the data interleave mode
		 * \return interleave method
		 */
		inline Interleave	ImageInterleave() const;

		/*!
		 * \brief Set the data intearleave mode
		 * \param inter intearleave method
		 */
		inline void			SetImageInterleave(const Interleave inter);

		/*!
		 * \brief Get the data packing mode
		 * \return packing method
		 */
		inline Packing		ImagePacking() const;

		/*!
		 * \brief Set the data packing mode
		 * \param pack packing method
		 */
		inline void			SetImagePacking(const Packing pack);

		/*!
		 * \brief Get the data sign (0 = unsigned, 1 = signed)
		 * \return data sign
		 */
		inline U8			DataSign() const;

		/*!
		 * \brief Set the data sign (0 = unsigned, 1 = signed)
		 * \param sign data sign
		 */
		inline void			SetDataSign(const U8 sign);

		/*!
		 * \brief Get the image sense (0 = positive, 1 = negative)
		 * \return image sense
		 */
		inline U8			ImageSense() const;

		/*!
		 * \brief Set the image sense (0 = positive, 1 = negative)
		 * \param sense image sense
		 */
		inline void			SetImageSense(const U8 sense);

		/*!
		 * \brief Get the number of bytes padding the end of each line
		 * \param i element index (0-7)
		 * \return count
		 */
		inline U32			EndOfLinePadding() const;

		/*!
		 * \brief Set the number of bytes padding the end of each line
		 * \param i element index (0-7)
		 * \param eolp count
		 */
		inline void			SetEndOfLinePadding(const U32 eolp);

		/*!
		 * \brief Get the number of bytes padding the end of the image element
		 * \param i element index (0-7)
		 * \return count
		 */
		inline U32			EndOfImagePadding() const;

		/*!
		 * \brief Set the number of bytes padding the end of the image element
		 * \param i element index (0-7)
		 * \param eoip count
		 */
		inline void			SetEndOfImagePadding(const U32 eoip);

		/* end of group */
		//@}

		/*!
		 * \name Image Origination Methods
		 */
		 //@{

		/*!
		 * \brief Get the line offset (in pixels) from the first pixel in original image
		 * \return offset count
		 */
		inline S32			XOffset() const;

		/*!
		 * \brief Set the line offset (in pixels) from the first pixel in original image
		 * \param offset offset count
		 */
		inline void			SetXOffset(const S32 offset);

		/*!
		 * \brief Get the frame offset (in lines) from the first line in original image
		 * \return offset count
		 */
		inline S32			YOffset() const;

		/*!
		 * \brief Set the frame offset (in lines) from the first line in original image
		 * \param offset offset count
		 */
		inline void			SetYOffset(const S32 offset);

		/*!
		 * \brief Get the source image file name that this image was extracted
		 * \param fn buffer to write source file name (100+1)
		 */
		inline void			SourceImageFileName(char *fn) const;

		/*!
		 * \brief Set the source image file name that this image was extracted
		 * \param fn buffer with source file name
		 */
		inline void			SetSourceImageFileName(const char *fn);

		/*!
		 * \brief Get the source image time and date that this image was extracted
		 * \param td buffer to write time/date string (24+1)
		 */
		inline void			SourceDate(char *td) const;

		/*!
		 * \brief Set the source image time and date that this image was extracted
		 * \param td buffer with time/date string
		 */
		inline void			SetSourceDate(const char *td);

		/*!
		 * \brief Get the source image time and date that this image was extracted
		 * \param td buffer to write time/date string (24+1)
		 */
		inline void			SourceTime(char *td) const;

		/*!
		 * \brief Set the source image time and date that this image was extracted
		 * \param td buffer with time/date string
		 */
		inline void			SetSourceTime(const char *td);

		/*!
		 * \brief Set the source image time and date that this image was extracted
		 * \param secs number of seconds since January 1, 1970 00:00
		 */
		void				SetSourceTimeDate(const long secs);

		/*!
		 * \brief Get the input device name
		 * \param dev buffer to write device (64+1)
		 */
		inline void			InputDevice(char *dev) const;

		/*!
		 * \brief Set the input device name
		 * \param dev buffer with device name
		 */
		inline void 		SetInputDevice(const char *dev);

		/*!
		 * \brief Get the input device model number
		 * \param sn buffer to write device model number (32+1)
		 */
		inline void			InputDeviceModelNumber(char *sn) const;

		/*!
		 * \brief Set the input device model number
		 * \param sn buffer with device model number
		 */
		inline void			SetInputDeviceModelNumber(const char *sn);

		/*!
		 * \brief Get the input device serial number
		 * \param sn buffer to write device serial number (32+1)
		 */
		inline void			InputDeviceSerialNumber(char *sn) const;

		/*!
		 * \brief Set the input device serial number
		 * \param sn buffer with device serial number
		 */
		inline void			SetInputDeviceSerialNumber(const char *sn);

		/*!
		 * \brief Get the horizontal pitch of the device
		 * \return pitch in samples/mm
		 */
		inline R32			XDevicePitch() const;

		/*!
		 * \brief Set the horizontal pitch of the device
		 * \param size pitch in samples/mm
		 */
		inline void			SetXDevicePitch(const R32 size);

		/*!
		 * \brief Get the veritcal pitch of the device
		 * \return pitch in samples/mm
		 */
		inline R32			YDevicePitch() const;

		/*!
		 * \brief Set the vertical pitch of the device
		 * \param size pitch in samples/mm
		 */
		inline void			SetYDevicePitch(const R32 size);

		/*!
		 * \brief Get the gamma correction exponent
		 * \return gamma exponent
		 */
		inline R32			Gamma() const;

		/*!
		 * \brief Set the gamma correction exponent
		 * \param gamma gamma exponent
		 */
		inline void			SetGamma(const R32 gamma);

		/* end of group */
		//@}

		/*!
		 * \brief Number of Active Elements in the Image
		 * \return element count
		 */
		int	ImageElementCount() const;

		/*!
		 * \brief Set member numberOfElements based on channel structure
		 */
		void CalculateNumberOfElements();

		/*!
		 * \brief DataSize required for individual image element components
		 * \return datasize of element
		 */
		DataSize ComponentDataSize(const int element) const;

		/*!
		 * \brief Byte count of data element components
		 * \return byte count
		 */
		int ComponentByteCount(const int element) const;

		/*
		 * \brief Byte size for each DataSize
		 * \return byte count
		 */
		static int DataSizeByteCount(const DataSize ds);

	};


	/*!
	 * \struct IndustryHeader
	 * \brief Motion Picture and Television Industry Specific Information
	 */
	struct IndustryHeader
	{

		/*!
		 * \name Motion Picture Industry Specific Members
		 */
		 //@{
		U8					filmManufacturingIdCode;	//!< Film edge code manufacturing ID code
		U8					filmType;					//!< Film edge code type
		U8					perfsOffset;				//!< Film edge code offset in perfs
		U8					unused1;					//!< Unused (word alignment)
		U32					prefix;						//!< Film edge code prefix
		U32					count;						//!< Film edge code count
		ASCII				format[32];					//!< Format string, e.g. Academy
		U32					framePosition;				//!< Frame position in sequence
		R32					frameRate;					//!< Frame rate of original (frame / sec)
		ASCII				frameId[32];				//!< Frame identification, e.g. keyframe
		ASCII				slateInfo[200];				//!< Slate information
		ASCII				reserved1[740];				//!< Reserved
		/* end of group */
		//@}

		/*!
		 * \brief Constructor
		 */
							IndustryHeader();

		/*!
		 * \brief Reset class to initial state
		 */
		void				Reset();


		// set/get functions for the data methods

		/*!
		 * \name Motion Picture Industry Specific Methods
		 */
		 //@{

		/*!
		 * \brief Get the film edge code information that is machine readable
		 * \param edge buffer to write film edge code information (16+1 chars)
		 */
		void				FilmEdgeCode(char *edge) const;

		/*!
		 * \brief Set the film edge code information that is machine readable
		 * \param edge buffer with film edge code information
		 */
		void				SetFilmEdgeCode(const char *edge);

		/*!
		 * \brief Get the format (e.g., Academy)
		 * \param fmt buffer to write format information (32+1 chars)
		 */
		inline void			Format(char *fmt) const;

		/*!
		 * \brief Set the format (e.g., Academy)
		 * \param fmt buffer with format information
		 */
		inline void			SetFormat(const char *fmt);

		/*!
		 * \brief Get the frame position in sequence
		 * \return position
		 */
		inline U32			FramePosition() const;

		/*!
		 * \brief Set the frame position in sequence
		 * \param pos position
		 */
		inline void			SetFramePosition(const U32 pos);

		/*!
		 * \brief Get the frame rate (frames / second)
		 * \return rate
		 */
		inline R32			FrameRate() const;

		/*!
		 * \brief Set the frame rate (frames / second)
		 * \param rate rate
		 */
		inline void			SetFrameRate(const R32 rate);

		/*!
		 * \brief Get the user-defined frame identification
		 * \param id buffer to write frame identification (32+1 chars)
		 */
		inline void			FrameId(char *id) const;

		/*!
		 * \brief Set the user-defined frame identification
		 * \param id buffer with frame identification
		 */
		inline void			SetFrameId(const char *id);

		/*!
		 * \brief Get the production information from the camera slate
		 * \param slate buffer to write slate information (200+1 chars)
		 */
		inline void			SlateInfo(char *slate) const;

		/*!
		 * \brief Set the production information from the camera slate
		 * \param slate buffer with slate information
		 */
		inline void			SetSlateInfo(const char *slate);

		/* end of group */
		//@}

	protected:
			U32 TCFromString(const char *str) const;
	};



	/*!
	 * \brief Complete DPX Header
	 */
	struct Header : public GenericHeader, public IndustryHeader
	{
							Header();

		/*!
		 * \brief Set the header data to a known start state
		 */
		void				Reset();

		/*!
		 * \brief Set the Input Stream object to read header from
		 */
		bool				Read(InStream *);

		/*!
		 * \brief Set the Output Stream object to write header to
		 */
		bool				Write(OutStream *);

		// write the offset within the header
		bool				WriteOffsetData(OutStream *);

		/*!
		 * \brief Validate the header
		 */
		bool				Validate();

		/*!
		 * \brief Does header require endian byte swap
		 * \return swap required true/false
		 */
		inline bool			RequiresByteSwap() const;

		/*!
		 * \brief Check magic cookie
		 * \return valid true/false
		 */
		static bool			ValidMagicCookie(const U32 magic);

		/*!
		 * \brief Returns the size of the header
		 * \return 2048 as defined by the standard
		 */
		const U32			Size() const;

		/*!
		 * \brief Calculate all of the offset members in the header
		 */
		void				CalculateOffsets();

		/*!
		 * \brief Set whether reader/writer should swap component ordering
		 * \param swap allow swapping true/false
		 */
		void				SetDatumSwap(const bool swap);

		// system check, used only during platform port
		bool				Check();

		/*!
		 * \brief Height of the image (maximum of all elements' heights) adjusted for orientation
		 * \return height
		 */
		U32					Height() const;

		/*!
		 * \brief Width of the image (maximum of all elements' widths) adjusted for orientation
		 * \param element image element
		 * \return width
		 */
		U32					Width() const;


	protected:
		bool DetermineByteSwap(const U32 magic) const;
	};





	inline bool Header::RequiresByteSwap() const
	{
		return this->DetermineByteSwap(this->magicNumber);
	}

	inline const U32 Header::Size() const
	{
		return 2048;
	}



	inline U32 GenericHeader::MagicNumber() const
	{
		return this->magicNumber;
	}

	inline U32 GenericHeader::ImageOffset() const
	{
		return this->imageOffset;
	}

	inline void GenericHeader::SetImageOffset(const U32 offset)
	{
		this->imageOffset = offset;
	}

	inline void GenericHeader::Version(char *v) const
	{
		OIIO::Strutil::safe_strcpy(v, this->version, 8);
	}

	inline void GenericHeader::SetVersion(const char * v)
	{
		OIIO::Strutil::safe_strcpy(this->version, v, 8);
	}

	inline U32 GenericHeader::FileSize() const
	{
		return this->fileSize;
	}

	inline void GenericHeader::SetFileSize(const U32 fs)
	{
		this->fileSize = fs;
	}

	inline U32 GenericHeader::GenericSize() const
	{
		return this->genericSize;
	}

	inline U32 GenericHeader::IndustrySize() const
	{
		return this->industrySize;
	}

	inline U32 GenericHeader::UserSize() const
	{
		return this->userSize;
	}

	inline void GenericHeader::SetUserSize(const U32 size)
	{
		this->userSize = size;
	}

	inline void GenericHeader::FileName(char *fn) const
	{
		OIIO::Strutil::safe_strcpy(fn, this->fileName, 100);
	}

	inline void GenericHeader::SetFileName(const char *fn)
	{
		OIIO::Strutil::safe_strcpy(this->fileName, fn, 100);
	}

	inline void GenericHeader::CreationDate(char *ct) const
	{
		OIIO::Strutil::safe_strcpy(ct, this->creationDate, 12);
	}

	inline void GenericHeader::SetCreationDate(const char *ct)
	{
		OIIO::Strutil::safe_strcpy(this->creationDate, ct, 12);
	}

	inline void GenericHeader::CreationTime(char *ct) const
	{
		OIIO::Strutil::safe_strcpy(ct, this->creationTime, 12);
	}

	inline void GenericHeader::SetCreationTime(const char *ct)
	{
		OIIO::Strutil::safe_strcpy(this->creationTime, ct, 12);
	}

	inline Orientation GenericHeader::ImageOrientation() const
	{
		return Orientation(this->imageOrientation);
	}

	inline void GenericHeader::SetImageOrientation(const Orientation orient)
	{
		this->imageOrientation = orient;
	}

	inline U8 GenericHeader::NumberOfElements() const
	{
		return this->numberOfElements;
	}

	inline void GenericHeader::SetNumberOfElements(const U8 num)
	{
		this->numberOfElements = num;
	}

	inline U32 GenericHeader::PixelsPerLine(const int i) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return 0xffffffff;
		return this->chan[i].pixelsPerLine;
	}

	inline void GenericHeader::SetPixelsPerLine(const int i, const U32 ppl)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		this->chan[i].pixelsPerLine = ppl;
	}

	inline U32 GenericHeader::LinesPerElement(const int i) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return 0xffffffff;
		return this->chan[i].linesPerElement;
	}

	inline void GenericHeader::SetLinesPerElement(const int i, const U32 lpe)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		this->chan[i].linesPerElement = lpe;
	}

	inline U8 GenericHeader::DataSign() const
	{
		return this->dataSign;
	}

	inline void GenericHeader::SetDataSign(const U8 sign)
	{
		this->dataSign = sign;
	}

	inline R32 GenericHeader::LowData(const int i) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return std::numeric_limits<R32>::infinity();
		return this->chan[i].lowData;
	}

	inline void GenericHeader::SetLowData(const int i, const R32 data)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		this->chan[i].lowData = data;
	}

	inline R32 GenericHeader::LowQuantity(const int i) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return std::numeric_limits<R32>::max();
		return this->chan[i].lowQuantity;
	}

	inline void GenericHeader::SetLowQuantity(const int i, const R32 quant)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		this->chan[i].lowQuantity = quant;
	}

	inline R32 GenericHeader::HighData(const int i) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return std::numeric_limits<R32>::max();
		return this->chan[i].highData;
	}

	inline void GenericHeader::SetHighData(const int i, const R32 data)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		this->chan[i].highData = data;
	}

	inline R32 GenericHeader::HighQuantity(const int i) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return std::numeric_limits<R32>::max();
		return this->chan[i].highQuantity;
	}

	inline void GenericHeader::SetHighQuantity(const int i, const R32 quant)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		this->chan[i].highQuantity = quant;
	}

	inline U8 GenericHeader::Metric(const int i) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return 0xff;
		return this->chan[i].designator[0];
	}

	inline void GenericHeader::SetMetric(const int i, const U8 m)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		this->chan[i].designator[0] = m;
	}

	inline Descriptor GenericHeader::ImageDescriptor(const int i) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return Descriptor(0xff);
		return Descriptor(this->chan[i].designator[1]);
	}

	inline void GenericHeader::SetImageDescriptor(const int i, const Descriptor desc)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		this->chan[i].designator[1] = (U8)desc;
	}

	inline U8 GenericHeader::BitDepth(const int i) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return 0xff;
		return this->chan[i].bitDepth;
	}

	inline void GenericHeader::SetBitDepth(const int i, const U8 depth)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		this->chan[i].bitDepth = depth;
	}

	inline Interleave GenericHeader::ImageInterleave() const
	{
		return Interleave(this->interleave);
	}

	inline void GenericHeader::SetImageInterleave(const Interleave inter)
	{
		this->interleave = (U8)inter;
	}

	inline Packing GenericHeader::ImagePacking() const
	{
		return Packing(this->packing);
	}

	inline void GenericHeader::SetImagePacking(const Packing pack)
	{
		this->packing = (U8)pack;
	}

	inline U32 GenericHeader::EndOfLinePadding() const
	{
		if (this->endOfLinePadding == 0xffffffff)
				return 0;
		return this->endOfLinePadding;
	}

	inline void GenericHeader::SetEndOfLinePadding(const U32 eolp)
	{
		this->endOfLinePadding = eolp;
	}

	inline U32 GenericHeader::EndOfImagePadding() const
	{
		if (this->endOfImagePadding == 0xffffffff)
			return 0;
		return this->endOfImagePadding;
	}

	inline void GenericHeader::SetEndOfImagePadding(const U32 eoip)
	{
		this->endOfImagePadding = eoip;
	}

	inline void GenericHeader::LabelText(char *desc) const
	{
		OIIO::Strutil::safe_strcpy(desc, this->labelText, 200);
	}

	inline void GenericHeader::SetLabelText(const char *desc)
	{
		OIIO::Strutil::safe_strcpy(this->labelText, desc, 200);
	}


	inline S32 GenericHeader::XOffset() const
	{
		return this->xOffset;
	}

	inline void GenericHeader::SetXOffset(const S32 offset)
	{
		this->xOffset = offset;
	}

	inline S32 GenericHeader::YOffset() const
	{
		return this->yOffset;
	}

	inline void GenericHeader::SetYOffset(const S32 offset)
	{
		this->yOffset = offset;
	}

	inline void	GenericHeader::WhitePoint(R32 xy[2]) const
	{
		memcpy(xy, this->whitePoint, sizeof(xy[0]) * 2);
	}

	inline void	GenericHeader::SetWhitePoint(const R32 xy[2])
	{
		memcpy(this->whitePoint, xy, sizeof(this->whitePoint[0]) * 2);
	}

	inline void	GenericHeader::RedPrimary(R32 xy[2]) const
	{
		memcpy(xy, this->redPrimary, sizeof(xy[0]) * 2);
	}

	inline void	GenericHeader::SetRedPrimary(const R32 xy[2])
	{
		memcpy(this->redPrimary, xy, sizeof(this->redPrimary[0]) * 2);
	}

	inline void	GenericHeader::GreenPrimary(R32 xy[2]) const
	{
		memcpy(xy, this->greenPrimary, sizeof(xy[0]) * 2);
	}

	inline void	GenericHeader::SetGreenPrimary(const R32 xy[2])
	{
		memcpy(this->greenPrimary, xy, sizeof(this->greenPrimary[0]) * 2);
	}

	inline void	GenericHeader::BluePrimary(R32 xy[2]) const
	{
		memcpy(xy, this->bluePrimary, sizeof(xy[0]) * 2);
	}

	inline void	GenericHeader::SetBluePrimary(const R32 xy[2])
	{
		memcpy(this->bluePrimary, xy, sizeof(this->bluePrimary[0]) * 2);
	}

	inline void GenericHeader::SourceImageFileName(char *fn) const
	{
		OIIO::Strutil::safe_strcpy(fn, this->sourceImageFileName, 100);
	}

	inline void GenericHeader::SetSourceImageFileName(const char *fn)
	{
		OIIO::Strutil::safe_strcpy(this->sourceImageFileName, fn, 100);
	}

	inline void GenericHeader::SourceDate(char *td) const
	{
		OIIO::Strutil::safe_strcpy(td, this->sourceDate, 12);
	}

	inline void GenericHeader::SetSourceDate(const char *td)
	{
		OIIO::Strutil::safe_strcpy(this->sourceDate, td, 12);
	}

	inline void GenericHeader::SourceTime(char *td) const
	{
		OIIO::Strutil::safe_strcpy(td, this->sourceTime, 12);
	}

	inline void GenericHeader::SetSourceTime(const char *td)
	{
		OIIO::Strutil::safe_strcpy(this->sourceTime, td, 12);
	}

	inline void GenericHeader::InputDevice(char *dev) const
	{
		OIIO::Strutil::safe_strcpy(dev, this->inputDevice, 32);
	}

	inline void  GenericHeader::SetInputDevice(const char *dev)
	{
		OIIO::Strutil::safe_strcpy(this->inputDevice, dev, 32);
	}

	inline void GenericHeader::InputDeviceModelNumber(char *sn) const
	{
		OIIO::Strutil::safe_strcpy(sn, this->inputDeviceModelNumber, 32);
	}

	inline void GenericHeader::SetInputDeviceModelNumber(const char *sn)
	{
		OIIO::Strutil::safe_strcpy(this->inputDeviceModelNumber, sn, 32);
	}

	inline void GenericHeader::InputDeviceSerialNumber(char *sn) const
	{
		OIIO::Strutil::safe_strcpy(sn, this->inputDeviceSerialNumber, 32);
	}

	inline void GenericHeader::SetInputDeviceSerialNumber(const char *sn)
	{
		OIIO::Strutil::safe_strcpy(this->inputDeviceSerialNumber, sn, 32);
	}

	inline R32 GenericHeader::XDevicePitch() const
	{
		return this->xDevicePitch;
	}

	inline void GenericHeader::SetXDevicePitch(const R32 size)
	{
		this->xDevicePitch = size;
	}

	inline R32 GenericHeader::YDevicePitch() const
	{
		return this->yDevicePitch;
	}

	inline void GenericHeader::SetYDevicePitch(const R32 size)
	{
		this->yDevicePitch = size;
	}

	inline void IndustryHeader::Format(char *fmt) const
	{
		OIIO::Strutil::safe_strcpy(fmt, this->format, 32);
	}

	inline void IndustryHeader::SetFormat(const char *fmt)
	{
		OIIO::Strutil::safe_strcpy(this->format, fmt, 32);
	}

	inline U32 IndustryHeader::FramePosition() const
	{
		return this->framePosition;
	}

	inline void IndustryHeader::SetFramePosition(const U32 pos)
	{
		this->framePosition = pos;
	}

	inline R32 IndustryHeader::FrameRate() const
	{
		return this->frameRate;
	}

	inline void IndustryHeader::SetFrameRate(const R32 rate)
	{
		this->frameRate = rate;
	}

	inline void IndustryHeader::FrameId(char *id) const
	{
		OIIO::Strutil::safe_strcpy(id, this->frameId, 32);
	}

	inline void IndustryHeader::SetFrameId(const char *id)
	{
		OIIO::Strutil::safe_strcpy(this->frameId, id, 32);
	}

	inline void IndustryHeader::SlateInfo(char *slate) const
	{
		OIIO::Strutil::safe_strcpy(slate, this->slateInfo, 100);
	}

	inline void IndustryHeader::SetSlateInfo(const char *slate)
	{
		OIIO::Strutil::safe_strcpy(this->slateInfo, slate, 100);
	}

	inline R32 GenericHeader::Gamma() const
	{
		return this->gamma;
	}

	inline void GenericHeader::SetGamma(const R32 g)
	{
		this->gamma = g;
	}

}

#endif
