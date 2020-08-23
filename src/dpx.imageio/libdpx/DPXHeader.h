// -*- mode: C++; tab-width: 4 -*-
// vi: ts=4

/*! \file DPXHeader.h */

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
 
 
// SMPTE DPX graphic file format v2.0


#ifndef _DPX_DPXHEADER_H
#define _DPX_DPXHEADER_H 1

#include <cstring>
#include <OpenImageIO/strutil.h>
#include "DPXStream.h"



/*!
 * \def SMPTE_VERSION
 * \brief SMPTE 268M-2003 DPX Version
 */
#define SMPTE_VERSION		"V2.0"

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
 * \brief HEX value of "SDPX"
 */
#define MAGIC_COOKIE		0x53445058




namespace dpx 
{


	// DPX data types 

	/*!
	 * \typedef unsigned char U8
	 * \brief Unsigned 8 bit integer
	 */
	typedef unsigned char	U8;

	/*!
	 * \typedef unsigned char U16
	 * \brief Unsigned 16 bit integer
	 */	
	typedef unsigned short	U16;

	/*!
	 * \typedef unsigned char U32
	 * \brief Unsigned 32 bit integer
	 */		
	typedef unsigned int	U32;
	
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
		kFloat,											//!<
		kDouble											//!<
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
		kUndefinedOrientation = 0xffff					//!< Undefined orientation
	};


	/*!
	 * \enum Descriptor
	 * \brief Image element Descriptor
	 */
	enum Descriptor 
	{
		kUserDefinedDescriptor = 0,						//!< User defined descriptor
		kRed = 1,										//!< Red
		kGreen = 2,										//!< Green
		kBlue = 3,										//!< Blue
		kAlpha = 4,										//!< Alpha
		kLuma = 6,										//!< Luma (Y)
		kColorDifference = 7,							//!< Color difference
		kDepth = 8,										//!< Depth
		kCompositeVideo = 9,							//!< Composite video
		kRGB = 50,										//!< R,G,B
		kRGBA = 51,										//!< R,G,B,A
		kABGR = 52,										//!< A,B,G,R
		kCbYCrY = 100,									//!< Cb,Y,Cr,Y (4:2:2)
		kCbYACrYA = 101,								//!< Cb,Y,A,Cr,Y,A (4:2:2:4)
		kCbYCr = 102,									//!< Cb,Y,Cr (4:4:4)
		kCbYCrA = 103,									//!< Cb,Y,Cr,A (4:4:4:4)
		kUserDefined2Comp = 150,						//!< User defined 2 component element
		kUserDefined3Comp = 151,						//!< User defined 3 component element
		kUserDefined4Comp = 152,						//!< User defined 4 component element
		kUserDefined5Comp = 153,						//!< User defined 5 component element
		kUserDefined6Comp = 154,						//!< User defined 6 component element
		kUserDefined7Comp = 155,						//!< User defined 7 component element
		kUserDefined8Comp = 156,						//!< User defined 8 component element
		kUndefinedDescriptor = 0xff						//!< Undefined descriptor
	};


	/*!
	 * \enum Characteristic
	 * \brief Transfer Characteristic and Colorimetric Specification
	 */
	enum Characteristic 
	{
		kUserDefined = 0,					//!< User defined
		kPrintingDensity = 1,				//!< Printing density
		kLinear = 2,						//!< Linear, transfer only
		kLogarithmic = 3,					//!< Logarithmic, transfer only
		kUnspecifiedVideo = 4,				//!< Unspecified video
		kSMPTE274M = 5,						//!< SMPTE 274M
		kITUR709 = 6,						//!< ITU-R 709-4
		kITUR601 = 7,						//!< ITU-R 601-5 system B or G
		kITUR602 = 8,						//!< ITU-R 601-5 system M
		kNTSCCompositeVideo = 9,			//!< NTSC composite video
		kPALCompositeVideo = 10,			//!< PAL composite video
		kZLinear = 11,						//!< Z depth linear, transfer only
		kZHomogeneous = 12,					//!< Z depth homogeneous, transfer only
        kADX = 13,                          //!< SMPTE ST 2065-3 Academy Density Exchange Encoding (ADX)
		kUndefinedCharacteristic = 0xff		//!< Undefined
	};

	
	/*!
	 * \enum VideoSignal
	 * \brief Video Signal Standard
	 */
	enum VideoSignal 
	{
		kUndefined = 0,									//!< Undefined
		kNTSC = 1,										//!< NTSC
		kPAL = 2,										//!< PAL
		kPAL_M = 3,										//!< PAL-M
		kSECAM = 4,										//!< SECAM
		k525LineInterlace43AR = 50,						//!< YCbCr ITU-R 601-5 525-line, 2:1 interlace, 4:3 aspect ratio
		k625LineInterlace43AR = 51,						//!< YCbCr ITU-R 601-5 625-line, 2:1 interlace, 4:3 aspect ratio
		k525LineInterlace169AR = 100,					//!< YCbCr ITU-R 601-5 525-line, 2:1 interlace, 16:9 aspect ratio
		k625LineInterlace169AR = 101,					//!< YCbCr ITU-R 601-5 625-line, 2:1 interlace, 16:9 aspect ratio
		k1050LineInterlace169AR = 150,					//!< YCbCr 1050-line, 2:1 interlace, 16:9 aspect ratio
		k1125LineInterlace169AR_274 = 151,				//!< YCbCr 1125-line, 2:1 interlace, 16:9 aspect ratio (SMPTE 274M)
		k1250LineInterlace169AR = 152,					//!< YCbCr 1250-line, 2:1 interlace, 16:9 aspect ratio
		k1125LineInterlace169AR_240 = 153,				//!< YCbCr 1125-line, 2:1 interlace, 16:9 aspect ratio (SMPTE 240M)		
		k525LineProgressive169AR = 200,					//!< YCbCr 525-line, 1:1 progressive, 16:9 aspect ratio
		k625LineProgressive169AR = 201,					//!< YCbCr 625-line, 1:1 progressive, 16:9 aspect ratio
		k750LineProgressive169AR = 202,					//!< YCbCr 750-line, 1:1 progressive, 16:9 aspect ratio (SMPTE 296M)
		k1125LineProgressive169AR = 203,				//!< YCbCr 1125-line, 1:1 progressive, 16:9 aspect ratio (SMPTE 274M)
        k255 = 255
	};


	/*!
	 * \enum Packing
	 * \brief Component data packing method
	 */
	enum Packing
	{
		kPacked = 0,									//!< Packed into 32-bit words
		kFilledMethodA = 1,								//!< Filled to 32-bit words, method A
		kFilledMethodB = 2								//!< Filled to 32-bit words, method B
	};


	/*!
	 * \enum Encoding
	 * \brief Component data encoding method
	 */
	enum Encoding
	{
		kNone = 0,										//<! No encoding
		kRLE = 1										//<! Run length encoding
	};

	
	/*!
	 * \struct ImageElement
	 * \brief Data Structure for Image Element
	 */
	struct ImageElement
	{
		U32					dataSign;					//!< Data sign (0 = unsigned, 1 = signed)
		U32					lowData;					//!< Reference low data code value
		R32					lowQuantity;				//!< Reference low quantity represented
		U32					highData;					//!< Reference high data code value
		R32					highQuantity;				//!< Reference high quantity represented
		U8					descriptor;					//!< Descriptor \see Descriptor
		U8					transfer;					//!< Transfer characteristic \see Characteristic
		U8					colorimetric;				//!< Colorimetric Specification \see Characteristic
		U8					bitDepth;					//!< Bit depth, valid values are 8,10,12,16,32,64
		U16					packing;					//!< Packing \see Packing
		U16					encoding;					//!< Encoding \see Encoding
		U32					dataOffset;					//!< Offset to data
		U32					endOfLinePadding;			//!< End-of-Line Padding
		U32					endOfImagePadding;			//!< End-of-Image Padding
		ASCII				description[32];			//!< Description of Image Element

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
		ASCII				version[8];					//!< Version number of header format
		U32					fileSize;					//!< Total file size (in bytes)
		U32					dittoKey;					//!< Ditto Key (0 = same as previous frame, 1 = new)
		U32					genericSize;				//!< Generic Header length (in bytes)
		U32					industrySize;				//!< Industry Header length (in bytes)
		U32					userSize;					//!< User defined header length (in bytes)
		ASCII				fileName[100];				//!< File name
		ASCII				creationTimeDate[24];		//!< Create date time /see DateTimeFormat
		ASCII				creator[100];				//!< Creator
		ASCII				project[200];				//!< Project name
		ASCII				copyright[200];				//!< Copyright statement
		U32					encryptKey;					//!< Encryption Key (0xffffffff if unencrypted)
		ASCII				reserved1[104];				//!< Reserved
		/* end of group */
		//@}		
	

		/*!
		 * \name Image Information Members
		 */
		 //@{
		U16					imageOrientation;			//!< Image orientation \see Orientation
		U16					numberOfElements;			//!< Number of elements (1-8)
		U32					pixelsPerLine;				//!< Pixels per line
		U32					linesPerElement;			//!< Lines per element
		ImageElement		chan[MAX_ELEMENTS];			//!< Image element data structures
		ASCII				reserved2[52];				//!< Reserved
		/* end of group */
		//@}
		

		/*!
		 * \name Image Origination Members
		 */
		 //@{		
		U32					xOffset;					//!< X offset
		U32					yOffset;					//!< Y offset
		R32					xCenter;					//!< X center
		R32					yCenter;					//!< Y center
		U32					xOriginalSize;				//!< X original size
		U32					yOriginalSize;				//!< Y original size
		ASCII				sourceImageFileName[100];	//!< Source image file name
		ASCII				sourceTimeDate[24];			//!< Source date and time /see DateTimeFormat
		ASCII				inputDevice[32];			//!< Input device name
		ASCII				inputDeviceSerialNumber[32];	//!< Input device serial number
		U16					border[4];					//!< Border validity
		U32					aspectRatio[2];				//!< Pixel aspect ratio (horizontal:vertical)
		R32					xScannedSize;				//!< X scanned size
		R32					yScannedSize;				//!< Y scanned size
		ASCII				reserved3[20];				//!< Reserved
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
		 * \brief Get the ditto key
		 * \return ditto key
		 */			
		inline U32			DittoKey() const;
		
		/*!
		 * \brief Set the ditto key
		 * \param key ditto key
		 */	
		inline void 		SetDittoKey(const U32 key);
		
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
		inline void			CreationTimeDate(char *ct) const;
		
		/*!
		 * \brief Set the creation time/date
		 * \param ct buffer with creation time/date
		 */
		inline void 		SetCreationTimeDate(const char *ct);

		
		/*!
		 * \brief Set the creation time/date
		 * \param secs number of seconds since January 1, 1970 00:00
		 */
		void				SetCreationTimeDate(const long secs);
		
		/*!
		 * \brief Get the creator
		 * \param creat buffer to store creator (100+1 chars)
		 */
		inline void			Creator(char *creat) const;
		
		/*!
		 * \brief Set the creator
		 * \param creat buffer with creator
		 */
		inline void 		SetCreator(const char *creat);
		
		/*!
		 * \brief Get the project
		 * \param prj buffer to store project (200+1 chars)
		 */
		inline void			Project(char *prj) const;
		
		/*!
		 * \brief Set the project
		 * \param prj buffer with project
		 */
		inline void 		SetProject(const char *prj);
		
		/*!
		 * \brief Get the copyright information
		 * \param copy buffer to store copyright string (200+1 chars)
		 */
		inline void			Copyright(char *copy) const;	
		
		/*!
		 * \brief Set the copyright information
		 * \param copy buffer with copyright string
		 */
		inline void 		SetCopyright(const char *copy);
		
		/*!
		 * \brief Get the encryption key (no encryption is 0xffffffff)
		 * \return encryption key 
		 */		
		inline U32			EncryptKey() const;
		
		/*!
		 * \brief Set the encryption key (no encryption is 0xffffffff)
		 * \param key encryption key 
		 */	
		inline void 		SetEncryptKey(const U32 key);
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
		inline U16			NumberOfElements() const;
		
		/*!
		 * \brief Set the number of elements
		 * \param num element count 
		 */
		inline void			SetNumberOfElements(const U16 num);
		
		/*!
		 * \brief Get the pixels per line
		 * \return pixel count 
		 */	
		inline U32			PixelsPerLine() const;
		
		/*!
		 * \brief Set the pixels per line
		 * \param ppl pixel count 
		 */	
		inline void			SetPixelsPerLine(const U32 ppl);
		
		/*!
		 * \brief Get the lines per element
		 * \return lines count 
		 */	
		inline U32			LinesPerElement() const;
		
		/*!
		 * \brief Set the lines per element
		 * \param lpe lines count 
		 */	
		inline void			SetLinesPerElement(const U32 lpe);
		
		/*!
		 * \brief Get the data sign (0 = unsigned, 1 = signed)
		 * \param i element index (0-7)
		 * \return data sign 
		 */	
		inline U32			DataSign(const int i) const;
		
		/*!
		 * \brief Set the data sign (0 = unsigned, 1 = signed)
		 * \param i element index (0-7)
		 * \param sign data sign 
		 */	
		inline void			SetDataSign(const int i, const U32 sign);
		
		/*!
		 * \brief Get the minimum data value
		 * \param i element index (0-7)
		 * \return minimum value
		 */	
		inline U32			LowData(const int i) const;
		
		/*!
		 * \brief Set the minimum data value
		 * \param i element index (0-7)
		 * \param data minimum value
		 */
		inline void			SetLowData(const int i, const U32 data);
		
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
		inline U32			HighData(const int i) const;
		
		/*!
		 * \brief Set the maximum data value
		 * \param i element index (0-7)
		 * \param data maximum value 
		 */	
		inline void			SetHighData(const int i, const U32 data);
		
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
		 * \brief Get the component defintion
		 * \param i element index (0-7)
		 * \return component descriptor 
		 */	
		inline Descriptor	ImageDescriptor(const int i) const;
		
		/*!
		 * \brief Set the component defintion
		 * \param i element index (0-7)
		 * \param desc component descriptor 
		 */	
		inline void			SetImageDescriptor(const int i, const Descriptor desc);
		
		/*!
		 * \brief Get the amplitude transfer function
		 * \param i element index (0-7)
		 * \return transfer characteristic 
		 */	
		inline Characteristic	Transfer(const int i) const;
		
		/*!
		 * \brief Set the amplitude transfer function
		 * \param i element index (0-7)
		 * \param ch transfer characteristic 
		 */
		inline void			SetTransfer(const int i, const Characteristic ch);
		
		/*!
		 * \brief Get the color reference
		 * \param i element index (0-7)
		 * \return colorimetric specification 
		 */	
		inline Characteristic	Colorimetric(const int i) const;
		
		/*!
		 * \brief Set the color reference
		 * \param i element index (0-7)
		 * \param c colorimetric specification 
		 */	
		inline void			SetColorimetric(const int i, const Characteristic c);
		
		/*!
		 * \brief Get the bit size of each component
		 * \param i element index (0-7)
		 * \return bit size 
		 */	
		inline U8			BitDepth(const int i) const;
		
		/*!
		 * \brief Set the bit size of each component
		 * \param i element index (0-7)
		 * \param depth bit size 
		 */	
		inline void			SetBitDepth(const int i, const U8 depth);
		
		/*!
		 * \brief Get the data packing mode
		 * \param i element index (0-7)
		 * \return packing method
		 */	
		inline Packing		ImagePacking(const int i) const;
		
		/*!
		 * \brief Set the data packing mode
		 * \param i element index (0-7)
		 * \param pack packing method
		 */	
		inline void			SetImagePacking(const int i, const Packing pack);
		
		/*!
		 * \brief Get the encoding method
		 * \param i element index (0-7)
		 * \return encoding method 
		 */	
		inline Encoding		ImageEncoding(const int i) const;
		
		/*!
		 * \brief Set the encoding method
		 * \param i element index (0-7)
		 * \param enc encoding method 
		 */	
		inline void			SetImageEncoding(const int i, const Encoding enc);
		
		/*!
		 * \brief Get the offset to element
		 * \param i element index (0-7)
		 * \return offset in bytes from the start of the file
		 */	
		inline U32			DataOffset(const int i) const;
		
		/*!
		 * \brief Set the offset to element
		 * \param i element index (0-7)
		 * \param offset offset in bytes from the start of the file
		 */	
		inline void			SetDataOffset(const int i, const U32 offset);
		
		/*!
		 * \brief Get the number of bytes padding the end of each line
		 * \param i element index (0-7)
		 * \return count 
		 */	
		inline U32			EndOfLinePadding(const int i) const;
		
		/*!
		 * \brief Set the number of bytes padding the end of each line
		 * \param i element index (0-7)
		 * \param eolp count 
		 */	
		inline void			SetEndOfLinePadding(const int i, const U32 eolp);
		
		/*!
		 * \brief Get the number of bytes padding the end of the image element
		 * \param i element index (0-7)
		 * \return count 
		 */	
		inline U32			EndOfImagePadding(const int i) const;
		
		/*!
		 * \brief Set the number of bytes padding the end of the image element
		 * \param i element index (0-7)
		 * \param eoip count 
		 */
		inline void			SetEndOfImagePadding(const int i, const U32 eoip);
		
		/*!
		 * \brief Get the element description
		 * \param i element index (0-7)
		 * \param desc buffer to write description string (32+1 chars) 
		 */	
		inline void			Description(const int i, char *desc) const;
		
		/*!
		 * \brief Set the element description
		 * \param i element index (0-7)
		 * \param desc buffer 
		 */	
		inline void			SetDescription(const int i, const char *desc);
		
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
		inline U32			XOffset() const;
		
		/*!
		 * \brief Set the line offset (in pixels) from the first pixel in original image
		 * \param offset offset count 
		 */	
		inline void			SetXOffset(const U32 offset);
		
		/*!
		 * \brief Get the frame offset (in lines) from the first line in original image
		 * \return offset count 
		 */	
		inline U32			YOffset() const;
		
		/*!
		 * \brief Set the frame offset (in lines) from the first line in original image
		 * \param offset offset count 
		 */
		inline void			SetYOffset(const U32 offset);
		
		/*!
		 * \brief Get the X image center in pixels
		 * \return pixel position 
		 */	
		inline R32			XCenter() const;
		
		/*!
		 * \brief Set the X image center in pixels
		 * \param center pixel position 
		 */
		inline void			SetXCenter(const R32 center);
		
		/*!
		 * \brief Get the Y image center in pixels
		 * \return pixel position 
		 */	
		inline R32			YCenter() const;
		
		/*!
		 * \brief Set the Y image center in pixels
		 * \param center pixel position 
		 */	
		inline void			SetYCenter(const R32 center);
		
		/*!
		 * \brief Get the number of pixels per line in the original image
		 * \return size 
		 */	
		inline U32			XOriginalSize() const;
		
		/*!
		 * \brief GSt the number of pixels per line in the original image
		 * \param size size 
		 */	
		inline void			SetXOriginalSize(const U32 size);
		
		/*!
		 * \brief Get the number of lines per image in the original image
		 * \return size 
		 */
		inline U32			YOriginalSize() const;
		
		/*!
		 * \brief Set the number of lines per image in the original image
		 * \param size size 
		 */
		inline void			SetYOriginalSize(const U32 size);
		
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
		inline void			SourceTimeDate(char *td) const;
		
		/*!
		 * \brief Set the source image time and date that this image was extracted
		 * \param td buffer with time/date string
		 */
		inline void			SetSourceTimeDate(const char *td);
		
		/*!
		 * \brief Set the source image time and date that this image was extracted
		 * \param secs number of seconds since January 1, 1970 00:00
		 */
		void				SetSourceTimeDate(const long secs);
		
		/*!
		 * \brief Get the input device name
		 * \param dev buffer to write device (32+1) 
		 */		
		inline void			InputDevice(char *dev) const;
		
		/*!
		 * \brief Set the input device name
		 * \param dev buffer with device name 
		 */	
		inline void 		SetInputDevice(const char *dev);
		
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
		 * \brief Get the pixel offset for the border region
		 *
		 * There are 4 border pixel offsets that define a region -- X-left, X-right, Y-top, Y-bottom
		 *
		 * \param i border index (0-3)
		 * \return offset in pixels 
		 */	
		inline U16			Border(const int i) const;

		/*!
		 * \brief Set the pixel offset for the border region
		 *
		 * There are 4 border pixel offsets that define a region -- X-left, X-right, Y-top, Y-bottom
		 *
		 * \param i border index (0-3)
		 * \param bord offset in pixels 
		 */	
		inline void			SetBorder(const int i, const U16 bord);
		
		/*!
		 * \brief Get the pixel aspect ratio (horizontal:vertical)
		 * \param i aspect ratio index (0-1)
		 * \return ratio quantity 
		 */	
		inline U32			AspectRatio(const int i) const;
		
		/*!
		 * \brief Set the pixel aspect ratio (horizontal:vertical)
		 * \param i aspect ratio index (0-1)
		 * \param ar ratio quantity 
		 */	
		inline void			SetAspectRatio(const int i, const U32 ar);
		
		/*!
		 * \brief Get the horizontal size of the original scanned optical image
		 * \return size in millimeters 
		 */	
		inline R32			XScannedSize() const;
		
		/*!
		 * \brief Set the horizontal size of the original scanned optical image
		 * \param size size in millimeters 
		 */
		inline void			SetXScannedSize(const R32 size);
		
		/*!
		 * \brief Get the vertical size of the original scanned optical image
		 * \return size in millimeters 
		 */	
		inline R32			YScannedSize() const;
		
		/*!
		 * \brief Set the vertical size of the original scanned optical image
		 * \param size size in millimeters 
		 */	
		inline void			SetYScannedSize(const R32 size);
		
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
		 * \brief Number of components for the element
		 * \return number of components
		 */
		int ImageElementComponentCount(const int element) const;
				
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
		ASCII				filmManufacturingIdCode[2];	//!< Film edge code manufacturing ID code
		ASCII				filmType[2];				//!< Film edge code type
		ASCII				perfsOffset[2];				//!< Film edge code offset in perfs
		ASCII				prefix[6];					//!< Film edge code prefix
		ASCII				count[4];					//!< Film edge code count
		ASCII				format[32];					//!< Format string, e.g. Academy
		U32					framePosition;				//!< Frame position in sequence
		U32					sequenceLength;				//!< Sequence length
		U32					heldCount;					//!< Held count (1 = default)
		R32					frameRate;					//!< Frame rate of original (frame / sec)
		R32					shutterAngle;				//!< Shutter angle of camera (degrees)
		ASCII				frameId[32];				//!< Frame identification, e.g. keyframe
		ASCII				slateInfo[100];				//!< Slate information
		ASCII				reserved4[56];				//!< Reserved
		/* end of group */
		//@}	

				
		/*!
		 * \name Television Industry Specific Members
		 */
		 //@{	
		U32					timeCode;					//!< Time code
		U32					userBits;					//!< User bits
		U8					interlace;					//!< Interlace (0 = noninterlace, 1 = 2:1 interlace)
		U8					fieldNumber;				//!< Field number
		U8					videoSignal;				//!< Video signal \see VideoSignal
		U8					zero;						//!< Structure alignment padding
		R32					horizontalSampleRate;		//!< Horizontal sample rate (in Hz)
		R32					verticalSampleRate;			//!< Vertical sample rate (in Hz)
		R32					temporalFrameRate;			//!< Temporal sample rate (in Hz)
		R32					timeOffset;					//!< Time offset from sync to first pixel (in ms)
		R32					gamma;						//!< Gamma
		R32					blackLevel;					//!< Black level
		R32					blackGain;					//!< Black gain
		R32					breakPoint;					//!< Break point
		R32					whiteLevel;					//!< White level
		R32					integrationTimes;			//!< Integration time (in sec)
		ASCII				reserved5[76];				//!< Reserved
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
		void				SetFileEdgeCode(const char *edge);
		
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
		 * \brief Get the total number of frames in sequence
		 * \return length 
		 */	
		inline U32			SequenceLength() const;
		
		/*!
		 * \brief Set the total number of frames in sequence
		 * \param len length 
		 */	
		inline void			SetSequenceLength(const U32 len);
		
		/*!
		 * \brief Get the how many sequential frames for which to hold current frame
		 * \return count 
		 */	
		inline U32			HeldCount() const;
		
		/*!
		 * \brief Set the how many sequential frames for which to hold current frame
		 * \param count count 
		 */	
		inline void			SetHeldCount(const U32 count);
		
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
		 * \brief Get the shutter angle of the motion picture camera
		 * \return degrees of the temporal sampling aperture
		 */
		inline R32			ShutterAngle() const;
		
		/*!
		 * \brief Set the shutter angle of the motion picture camera
		 * \param angle degrees of the temporal sampling aperture
		 */
		inline void			SetShutterAngle(const R32 angle);
		
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
		 * \param slate buffer to write slate information (100+1 chars)
		 */
		inline void			SlateInfo(char *slate) const;
		
		/*!
		 * \brief Set the production information from the camera slate
		 * \param slate buffer with slate information
		 */
		inline void			SetSlateInfo(const char *slate);
		
		/* end of group */
		//@}
		
		/*!
		 * \name Television Industry Specific Methods
		 */
		 //@{			
		
		/*!
		 * \brief Get the time code
		 * \param str buffer to write time code (12 chars)
		 */
		void				TimeCode(char *str) const;
		
		/*!
		 * \brief Set the time code
		 * \param str buffer with time code
		 */
		void				SetTimeCode(const char *str);
		
		/*!
		 * \brief Get the user bits
		 * \param str buffer to write user bits (12 chars)
		 */
		void				UserBits(char *str) const;
		
		/*!
		 * \brief Set the user bits
		 * \param str buffer with user bits
		 */
		void				SetUserBits(const char *str);
		
		/*!
		 * \brief Get the interlace (0 = noninterlace, 1 = 2:1 interlace)
		 * \return interlace value
		 */
		inline U8			Interlace() const;
		
		/*!
		 * \brief Set the interlace (0 = noninterlace, 1 = 2:1 interlace)
		 * \param lace interlace value
		 */
		inline void 		SetInterlace(const U8 lace);
		
		/*!
		 * \brief Get the field number of the video decoded
		 * \return field number
		 */
		inline U8			FieldNumber() const;
		
		/*!
		 * \brief Set the field number of the video decoded
		 * \param fn field number
		 */
		inline void			SetFieldNumber(const U8 fn);
		
		/*!
		 * \brief Get the video source
		 * \return signal
		 */
		inline VideoSignal	Signal() const;
		
		/*!
		 * \brief Set the video source
		 * \param vs signal
		 */
		inline void			SetSignal(const VideoSignal vs);
		
		/*!
		 * \brief Get the clock rate at which samples were acquired
		 * \return rate
		 */
		inline R32			HorizontalSampleRate() const;
		
		/*!
		 * \brief Set the clock rate at which samples were acquired
		 * \param rate rate
		 */
		inline void			SetHorizontalSampleRate(const R32 rate);
		
		/*!
		 * \brief Get the rate at which scanning the whole image is repeated
		 * \return rate
		 */
		inline R32			VerticalSampleRate() const;
		
		/*!
		 * \brief Set the rate at which scanning the whole image is repeated
		 * \param rate rate
		 */
		inline void			SetVerticalSampleRate(const R32 rate);
		
		/*!
		 * \brief Get the applied gamma correction
		 * \return rate
		 */
		inline R32			TemporalFrameRate() const;
		
		/*!
		 * \brief Set the applied gamma correction
		 * \param rate gamma
		 */
		inline void			SetTemporalFrameRate(const R32 rate);
		
		/*!
		 * \brief Get the time offset from sync to first pixel
		 * \return time in microseconds
		 */
		inline R32			TimeOffset() const;
		
		/*!
		 * \brief Set the time offset from sync to first pixel
		 * \param offset time in microseconds
		 */
		inline void			SetTimeOffset(const R32 offset);
		
		/*!
		 * \brief Get the applied gamma correction
		 * \return gamma
		 */
		inline R32			Gamma() const;
		
		/*!
		 * \brief Set the applied gamma correction
		 * \param g gamma
		 */
		inline void			SetGamma(const R32 g);
		
		/*!
		 * \brief Get the reference black level
		 * \return value
		 */
		inline R32			BlackLevel() const;
		
		/*!
		 * \brief Set the reference black level
		 * \param bl value
		 */
		inline void			SetBlackLevel(const R32 bl);
		
		/*!
		 * \brief Get the gain applied to signals below the breakpoint
		 * \return value
		 */
		inline R32			BlackGain() const;
		
		/*!
		 * \brief Set the gain applied to signals below the breakpoint
		 * \param bg value
		 */
		inline void			SetBlackGain(const R32 bg);
		
		/*!
		 * \brief Get the breakpoint which gamma is applied
		 * \return value
		 */
		inline R32			BreakPoint() const;
		
		/*!
		 * \brief Set the breakpoint which gamma is applied
		 * \param bp value
		 */
		inline void			SetBreakPoint(const R32 bp);
		
		/*!
		 * \brief Get the reference white level
		 * \return value
		 */
		inline R32			WhiteLevel() const;
		
		/*!
		 * \brief Set the reference white level
		 * \param wl value
		 */
		inline void			SetWhiteLevel(const R32 wl);
		
		/*!
		 * \brief Get the temporal sampling rate of television cameras
		 * \return rate
		 */
		inline R32			IntegrationTimes() const;
		
		/*!
		 * \brief Set the temporal sampling rate of television cameras
		 * \param times rate
		 */
		inline void			SetIntegrationTimes(const R32 times);
		
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
		U32			Size() const;

		/*!
		 * \brief Calculate all of the offset members in the header
		 */
		void				CalculateOffsets();
		
		/*!
		 * \brief Determine whether the components of an element should be swapped \see ComponentOrdering
		 * \param element image element
		 * \return swap order of components
		 */
		bool				DatumSwap(const int element) const;

		/*!
		 * \brief Set whether reader/writer should swap component ordering
		 * \param swap allow swapping true/false
		 */
		void				SetDatumSwap(const bool swap);

		// system check, used only during platform port
		bool				Check();

		/*!
		 * \brief Height of the element adjusted for orientation
		 * \return height 
		 */
		U32					Height() const;
		
		/*!
		 * \brief Width of the element adjusted for orientation
		 * \return width 
		 */
		U32					Width() const;

			
	protected:
		bool DetermineByteSwap(const U32 magic) const;
		bool datumSwap;
	};





	/*!
	 * \brief User Defined data
	 */
	struct UserDefinedData
	{
		ASCII				userId[32];					//!< user data identification string
		U8 *				data;						//!< user data
	};



	inline bool Header::RequiresByteSwap() const
	{
		return this->DetermineByteSwap(this->magicNumber);
	}

	inline U32 Header::Size() const
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
		OIIO::Strutil::safe_strcpy(v, this->version, sizeof(this->version));
		v[8] = '\0';
	}
	
	inline void GenericHeader::SetVersion(const char * v)
	{
		OIIO::Strutil::safe_strcpy(this->version, v, sizeof(this->version));
	}
	
	inline U32 GenericHeader::FileSize() const
	{
		return this->fileSize;
	}
	
	inline void GenericHeader::SetFileSize(const U32 fs)
	{
		this->fileSize = fs;
	}
	
	inline U32 GenericHeader::DittoKey() const
	{
		return this->dittoKey;
	}
	
	inline void GenericHeader::SetDittoKey(const U32 key)
	{
		this->dittoKey = key;
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
		OIIO::Strutil::safe_strcpy(fn, this->fileName, sizeof(this->fileName));
	}
	
	inline void GenericHeader::SetFileName(const char *fn)
	{
		OIIO::Strutil::safe_strcpy(this->fileName, fn, sizeof(this->fileName));
	}
	
	inline void GenericHeader::CreationTimeDate(char *ct) const
	{
		OIIO::Strutil::safe_strcpy(ct, this->creationTimeDate, sizeof(this->creationTimeDate));
	}
	
	inline void GenericHeader::SetCreationTimeDate(const char *ct)
	{
		OIIO::Strutil::safe_strcpy(this->creationTimeDate, ct, sizeof(this->creationTimeDate));
	}
	
	inline void GenericHeader::Creator(char *creat) const
	{
		OIIO::Strutil::safe_strcpy(creat, this->creator, sizeof(this->creator));
	}
	
	inline void GenericHeader::SetCreator(const char *creat)
	{
		OIIO::Strutil::safe_strcpy(this->creator, creat, sizeof(this->creator));
	}
	
	inline void GenericHeader::Project(char *prj) const
	{
		OIIO::Strutil::safe_strcpy(prj, this->project, sizeof(this->project));
	}
	
	inline void GenericHeader::SetProject(const char *prj)
	{
		OIIO::Strutil::safe_strcpy(this->project, prj, sizeof(this->project));
	}
	
	inline void GenericHeader::Copyright(char *copy) const
	{
		OIIO::Strutil::safe_strcpy(copy, this->copyright, sizeof(this->copyright));
	}
	
	inline void GenericHeader::SetCopyright(const char *copy)
	{
		OIIO::Strutil::safe_strcpy(this->copyright, copy, sizeof(this->copyright));
	}
	
	inline U32 GenericHeader::EncryptKey() const
	{
		return this->encryptKey;
	}
	
	inline void GenericHeader::SetEncryptKey(const U32 key)
	{
		this->encryptKey = key;
	}
	
	
	inline Orientation GenericHeader::ImageOrientation() const
	{
		return Orientation(this->imageOrientation);
	}
	
	inline void GenericHeader::SetImageOrientation(const Orientation orient)
	{
		this->imageOrientation = orient;
	}
	
	inline U16 GenericHeader::NumberOfElements() const
	{
		return this->numberOfElements;
	}
	
	inline void GenericHeader::SetNumberOfElements(const U16 num)
	{
		this->numberOfElements = num;
	}
	
	inline U32 GenericHeader::PixelsPerLine() const
	{
		return this->pixelsPerLine;
	}
	
	inline void GenericHeader::SetPixelsPerLine(const U32 ppl)
	{
		this->pixelsPerLine = ppl;
	}
	
	inline U32 GenericHeader::LinesPerElement() const
	{
		return this->linesPerElement;
	}
	
	inline void GenericHeader::SetLinesPerElement(const U32 lpe)
	{
		this->linesPerElement = lpe;
	}
	
	inline U32 GenericHeader::DataSign(const int i) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return 0xffffffff;
		return this->chan[i].dataSign;
	}

	inline void GenericHeader::SetDataSign(const int i, const U32 sign)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		this->chan[i].dataSign = sign;
	}

	inline U32 GenericHeader::LowData(const int i) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return 0xffffffff;
		return this->chan[i].lowData;
	}

	inline void GenericHeader::SetLowData(const int i, const U32 data)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		this->chan[i].lowData = data;
	}

	inline R32 GenericHeader::LowQuantity(const int i) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return R32(0xffffffff);
		return this->chan[i].lowQuantity;
	}

	inline void GenericHeader::SetLowQuantity(const int i, const R32 quant)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		this->chan[i].lowQuantity = quant;
	}

	inline U32 GenericHeader::HighData(const int i) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return 0xffffffff;
		return this->chan[i].highData;
	}

	inline void GenericHeader::SetHighData(const int i, const U32 data)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		this->chan[i].highData = data;
	}

	inline R32 GenericHeader::HighQuantity(const int i) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return R32(0xffffffff);
		return this->chan[i].highQuantity;
	}

	inline void GenericHeader::SetHighQuantity(const int i, const R32 quant)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		this->chan[i].highQuantity = quant;
	}

	inline Descriptor GenericHeader::ImageDescriptor(const int i) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return Descriptor(0xff);
		return Descriptor(this->chan[i].descriptor);
	}

	inline void GenericHeader::SetImageDescriptor(const int i, const Descriptor desc)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		this->chan[i].descriptor = desc;
	}

	inline Characteristic GenericHeader::Transfer(const int i) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return Characteristic(0xff);
		return Characteristic(this->chan[i].transfer);
	}

	inline void GenericHeader::SetTransfer(const int i, const Characteristic ch)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		this->chan[i].transfer = ch;
	}

	inline Characteristic GenericHeader::Colorimetric(const int i) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return Characteristic(0xff);
		return Characteristic(this->chan[i].colorimetric);
	}

	inline void GenericHeader::SetColorimetric(const int i, const Characteristic c)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		this->chan[i].colorimetric = c;
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

	inline Packing GenericHeader::ImagePacking(const int i) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return Packing(0xff);
		return Packing(this->chan[i].packing);
	}

	inline void GenericHeader::SetImagePacking(const int i, const Packing pack)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		this->chan[i].packing = pack;
	}

	inline Encoding GenericHeader::ImageEncoding(const int i) const
	{
		Encoding e = kNone;
		
		if (i < 0 || i >= MAX_ELEMENTS)
			return kNone;
	
		if (this->chan[i].encoding == 1)
			e = kRLE;
		
		return e;
	}

	inline void GenericHeader::SetImageEncoding(const int i, const Encoding enc)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
	
		this->chan[i].encoding = (enc == kNone ? 0 : 1);
	}

	inline U32 GenericHeader::DataOffset(const int i) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return 0xffffffff;
		return this->chan[i].dataOffset;
	}

	inline void GenericHeader::SetDataOffset(const int i, const U32 offset)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		this->chan[i].dataOffset = offset;
	}

	inline U32 GenericHeader::EndOfLinePadding(const int i) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return 0xffffffff;
		if (this->chan[i].endOfLinePadding == 0xffffffff)
				return 0;
		return this->chan[i].endOfLinePadding;
	}

	inline void GenericHeader::SetEndOfLinePadding(const int i, const U32 eolp)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		this->chan[i].endOfLinePadding = eolp;
	}

	inline U32 GenericHeader::EndOfImagePadding(const int i) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return 0xffffffff;
		if (this->chan[i].endOfImagePadding == 0xffffffff)
			return 0;	
		return this->chan[i].endOfImagePadding;
	}

	inline void GenericHeader::SetEndOfImagePadding(const int i, const U32 eoip)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		this->chan[i].endOfImagePadding = eoip;
	}

	inline void GenericHeader::Description(const int i, char *desc) const
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		OIIO::Strutil::safe_strcpy(desc, this->chan[i].description, 32);
	}

	inline void GenericHeader::SetDescription(const int i, const char *desc)
	{
		if (i < 0 || i >= MAX_ELEMENTS)
			return;
		OIIO::Strutil::safe_strcpy(this->chan[i].description, desc, 32);
	}
	
	
	inline U32 GenericHeader::XOffset() const
	{
		return this->xOffset;
	}
	
	inline void GenericHeader::SetXOffset(const U32 offset)
	{
		this->xOffset = offset;
	}
	
	inline U32 GenericHeader::YOffset() const
	{
		return this->yOffset;
	}
	
	inline void GenericHeader::SetYOffset(const U32 offset)
	{
		this->yOffset = offset;
	}
	
	inline R32 GenericHeader::XCenter() const
	{
		return this->xCenter;
	}
	
	inline void GenericHeader::SetXCenter(const R32 center)
	{
		this->xCenter = center;
	}
	
	inline R32 GenericHeader::YCenter() const
	{
		return this->yCenter;
	}
	
	inline void GenericHeader::SetYCenter(const R32 center)
	{
		this->yCenter = center;
	}
	
	inline U32 GenericHeader::XOriginalSize() const
	{
		return this->xOriginalSize;
	}
	
	inline void GenericHeader::SetXOriginalSize(const U32 size)
	{
		this->xOriginalSize = size;
	}
	
	inline U32 GenericHeader::YOriginalSize() const
	{
		return this->yOriginalSize;
	}
	
	inline void GenericHeader::SetYOriginalSize(const U32 size)
	{
		this->yOriginalSize = size;
	}
	
	inline void GenericHeader::SourceImageFileName(char *fn) const
	{
		OIIO::Strutil::safe_strcpy(fn, this->sourceImageFileName, sizeof(this->sourceImageFileName));
	}
	
	inline void GenericHeader::SetSourceImageFileName(const char *fn)
	{
		OIIO::Strutil::safe_strcpy(this->sourceImageFileName, fn, sizeof(this->sourceImageFileName));
	}
	
	inline void GenericHeader::SourceTimeDate(char *td) const
	{
		OIIO::Strutil::safe_strcpy(td, this->sourceTimeDate, sizeof(this->sourceTimeDate));
	}
	
	inline void GenericHeader::SetSourceTimeDate(const char *td)
	{
		OIIO::Strutil::safe_strcpy(this->sourceTimeDate, td, sizeof(this->sourceTimeDate));
	}
	
	inline void GenericHeader::InputDevice(char *dev) const
	{
		OIIO::Strutil::safe_strcpy(dev, this->inputDevice, sizeof(this->inputDevice));
	}
	
	inline void  GenericHeader::SetInputDevice(const char *dev)
	{
		OIIO::Strutil::safe_strcpy(this->inputDevice, dev, sizeof(this->inputDevice));
	}
	
	inline void GenericHeader::InputDeviceSerialNumber(char *sn) const
	{
		OIIO::Strutil::safe_strcpy(sn, this->inputDeviceSerialNumber, sizeof(this->inputDeviceSerialNumber));
	}
	
	inline void GenericHeader::SetInputDeviceSerialNumber(const char *sn)
	{
		OIIO::Strutil::safe_strcpy(this->inputDeviceSerialNumber, sn, sizeof(this->inputDeviceSerialNumber));
	}
	
	inline U16 GenericHeader::Border(const int i) const
	{
		if (i < 0 || i > 3)
			return 0xffff;
		
		return this->border[i];
	}
	
	inline void GenericHeader::SetBorder(const int i, const U16 bord)
	{
		if (i < 0 || i > 3)
			return;
		this->border[i] = bord;
	}
	
	inline U32 GenericHeader::AspectRatio(const int i) const
	{
		if (i !=  0 && i != 1)
			return 0xffffffff;
		
		return this->aspectRatio[i];
	}
	
	inline void GenericHeader::SetAspectRatio(const int i, const U32 ar)
	{
		if (i !=  0 && i != 1)
			return;
		this->aspectRatio[i] = ar;
	}
	
	inline R32 GenericHeader::XScannedSize() const
	{
		return this->xScannedSize;
	}
	
	inline void GenericHeader::SetXScannedSize(const R32 size)
	{
		this->xScannedSize = size;
	}
	
	inline R32 GenericHeader::YScannedSize() const
	{
		return this->yScannedSize;
	}
	
	inline void GenericHeader::SetYScannedSize(const R32 size)
	{
		this->yScannedSize = size;
	}
	
	
	inline void IndustryHeader::Format(char *fmt) const
	{
		OIIO::Strutil::safe_strcpy(fmt, this->format, sizeof(this->format));
	}

	inline void IndustryHeader::SetFormat(const char *fmt)
	{
		OIIO::Strutil::safe_strcpy(this->format, fmt, sizeof(this->format));
	}

	inline U32 IndustryHeader::FramePosition() const
	{
		return this->framePosition;
	}

	inline void IndustryHeader::SetFramePosition(const U32 pos)
	{
		this->framePosition = pos;
	}

	inline U32 IndustryHeader::SequenceLength() const
	{
		return this->sequenceLength;
	}

	inline void IndustryHeader::SetSequenceLength(const U32 len)
	{
		this->sequenceLength = len;
	}

	inline U32 IndustryHeader::HeldCount() const
	{
		return this->heldCount;
	}

	inline void IndustryHeader::SetHeldCount(const U32 count)
	{
		this->heldCount = count;
	}

	inline R32 IndustryHeader::FrameRate() const
	{
		return this->frameRate;
	}

	inline void IndustryHeader::SetFrameRate(const R32 rate)
	{
		this->frameRate = rate;
	}

	inline R32 IndustryHeader::ShutterAngle() const
	{
		return this->shutterAngle;
	}

	inline void IndustryHeader::SetShutterAngle(const R32 angle)
	{
		this->shutterAngle = angle;
	}

	inline void IndustryHeader::FrameId(char *id) const
	{
		OIIO::Strutil::safe_strcpy(id, this->frameId, sizeof(this->frameId));
	}

	inline void IndustryHeader::SetFrameId(const char *id)
	{
		OIIO::Strutil::safe_strcpy(this->frameId, id, sizeof(this->frameId));
	}

	inline void IndustryHeader::SlateInfo(char *slate) const
	{
		OIIO::Strutil::safe_strcpy(slate, this->slateInfo, sizeof(this->slateInfo));
	}

	inline void IndustryHeader::SetSlateInfo(const char *slate)
	{
		OIIO::Strutil::safe_strcpy(this->slateInfo, slate, sizeof(this->slateInfo));
	}


	inline U8 IndustryHeader::Interlace() const
	{
		return this->interlace;
	}

	inline void IndustryHeader::SetInterlace(const U8 lace)
	{
		this->interlace = lace;
	}

	inline U8 IndustryHeader::FieldNumber() const
	{
		return this->fieldNumber;
	}

	inline void IndustryHeader::SetFieldNumber(const U8 fn)
	{
		this->fieldNumber = fn;
	}

	inline VideoSignal IndustryHeader::Signal() const
	{
		return VideoSignal(this->videoSignal);
	}

	inline void IndustryHeader::SetSignal(const VideoSignal vs)
	{
		this->videoSignal = vs;
	}

	inline R32 IndustryHeader::HorizontalSampleRate() const
	{
		return this->horizontalSampleRate;
	}

	inline void IndustryHeader::SetHorizontalSampleRate(const R32 rate)
	{
		this->horizontalSampleRate = rate;
	}

	inline R32 IndustryHeader::VerticalSampleRate() const
	{
		return this->verticalSampleRate;
	}

	inline void IndustryHeader::SetVerticalSampleRate(const R32 rate)
	{
		this->verticalSampleRate = rate;
	}

	inline R32 IndustryHeader::TemporalFrameRate() const
	{
		return this->temporalFrameRate;
	}

	inline void IndustryHeader::SetTemporalFrameRate(const R32 rate)
	{
		this->temporalFrameRate = rate;
	}

	inline R32 IndustryHeader::TimeOffset() const
	{
		return this->timeOffset;
	}

	inline void IndustryHeader::SetTimeOffset(const R32 offset)
	{
		this->timeOffset = offset;
	}

	inline R32 IndustryHeader::Gamma() const
	{
		return this->gamma;
	}

	inline void IndustryHeader::SetGamma(const R32 g)
	{
		this->gamma = g;
	}

	inline R32 IndustryHeader::BlackLevel() const
	{
		return this->blackLevel;
	}

	inline void IndustryHeader::SetBlackLevel(const R32 bl)
	{
		this->blackLevel = bl;
	}

	inline R32 IndustryHeader::BlackGain() const
	{
		return this->blackGain;
	}

	inline void IndustryHeader::SetBlackGain(const R32 bg)
	{
		this->blackGain = bg;
	}

	inline R32 IndustryHeader::BreakPoint() const
	{
		return this->breakPoint;
	}

	inline void IndustryHeader::SetBreakPoint(const R32 bp)
	{
		this->breakPoint = bp;
	}

	inline R32 IndustryHeader::WhiteLevel() const
	{
		return this->whiteLevel;
	}

	inline void IndustryHeader::SetWhiteLevel(const R32 wl)
	{
		this->whiteLevel = wl;
	}

	inline R32 IndustryHeader::IntegrationTimes() const
	{
		return this->integrationTimes;
	}

	inline void IndustryHeader::SetIntegrationTimes(const R32 times)
	{
		this->integrationTimes = times;
	}
	
}

#endif
