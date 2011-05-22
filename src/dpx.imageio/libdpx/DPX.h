// vi: ts=4

/*! \file DPX.h */

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


#ifndef _DPX_H
#define _DPX_H 1

#include <cstdio>
#include <limits>

#include "DPXHeader.h"
#include "DPXStream.h"


/*!
 * \def OPENDPX_VERSION 
 * \brief OpenDPX Version
 */
#define OPENDPX_VERSION		"0.5.0"



	
/*!
 * \namespace dpx
 * \brief OpenDPX namespace 
 */
namespace dpx
{
	// forward definitions
	class Codec;
	class ElementReadStream;
	
	/*! 
	 * \enum Endian
	 * \brief DPX files can be stored in big- or little-endian byte order 
	 */
	enum Endian
	{
		kLittleEndian,								//!< increasing numeric significance with increasing memory
		kBigEndian									//!< big end first
	};
	

	/*! \struct Block
	 * \brief Rectangle block definition defined by two points
	 */
	struct Block 
	{
		int x1, y1, x2, y2;
		
		/*!
		 * \brief Constructor
		 */
		inline Block();

		/*!
		 * \brief Constructor
		 *
		 * \param x1 upper left x coordinate
		 * \param y1 upper left y coordinate
		 * \param x2 lower right x coordinate
		 * \param y2 lower right y coordinate
		 */
		Block(const int x1, const int y1, const int x2, const int y2);

		/*!
		 * \brief Set the block coordinates
		 *
		 * \param x1 upper left x coordinate
		 * \param y1 upper left y coordinate
		 * \param x2 lower right x coordinate
		 * \param y2 lower right y coordinate
		 */
		void Set(const int x1, const int y1, const int x2, const int y2);

		/*!
		 * \brief Check to see if a point is within the block
		 *
		 * \param x x coordinate
		 * \param y y coordinate
		 * \return true/false if coordinates within block
		 */
		inline bool Inside(const int x, const int y) const;

		/*!
		 * \brief Rearrange coordinates if necessary so the first coordinate is upper left and the second is lower right
		 */
		inline void Check();
	};


	// Current platform endian byte order
	extern Endian systemByteOrder;

	// namespace functions
	
	/*!
	 * \brief determine if the image file is DPX
	 *
	 * \param file buffer to read and search
	 * \return true/false if identified as DPX
	 */
	bool IdentifyFile(InStream *file);

	/*!
	 * \brief determine if the image file is DPX
	 *
	 * \param data memory to search
	 * \return true/false if identified as DPX
	 */
	bool IdentifyFile(const void *data);
	
	/*!
	 * \brief returns a char * of the default DPX file extension
	 *
	 * \return .dpx file extenion
	 */
	inline const char *DefaultExtension();

	/*!
	 * returns a string of the highest SMPTE DPX version supported by this library
	 *
	 * \return SMPTE DPX version
	 */
	inline const char *Version();


	/*!
	 * \brief returns the version string for this library
	 *
	 * \return OpenDPX version
	 */
	inline const char *LibraryVersion();





	/*!
	 * \class Reader
	 * \brief DPX Image Reader class
	 */
	 
	class Reader
	{

	public:

		/*!
		 * \brief DPX header
		 */	
		Header header;

		/*!
		 * \brief Constructor
		 */			
		Reader();
		
		/*!
		 * \brief Destructor
		 */			
		virtual ~Reader();

		/*!
		 * \brief Set the InStream object to be used to read images
		 *
		 * \param stream Object to use for low level reads
		 */	
		void SetInStream(InStream *stream);

		/*!
		 * \brief clear any caching or memory allocated specific to an image
		 */			
		void Reset();
		
		/*!
		 * \brief Read the dpx header into the header member
		 *
		 * \return success true/false
		 */	
		bool ReadHeader();

		/*!
		 * \brief Read an image element into a buffer
		 * 
		 * the size of the buffer must be large enough
		 * simple calculation would be:
		 *     width * height * num_of_components * size_of_component
		 * 
		 * \param element element (0-7)
		 * \param data buffer
		 * \return success true/false
		 */			
		bool ReadImage(const int element, void *data);

		/*!
		 * \brief Read an image element into a buffer that matches the image description type
		 *
		 * The DataSize allows the user to specific the buffer DataSize which can differ
		 * from the image element.  It is possible, for example, to read an 8-bit per
		 * component (3 components per pixel for RGB) into 16-bits.
		 *
		 * \param data buffer
		 * \param size size of the buffer component
		 * \param desc element description type
		 * \return success true/false
		 */	
		bool ReadImage(void *data, const DataSize size = kWord,  
			const Descriptor desc = kRGB);

		/*!
		 * \brief Read a rectangular image block into a buffer from the specified image element
		 *
		 * \param element element (0-7)
		 * \param data buffer
		 * \param block image area to read
		 * \return success true/false
		 */	
		bool ReadBlock(const int element, unsigned char *data, Block &block);

		/*!
		 * \brief Read a rectangular image block into a buffer from the image element
		 * specified by the Descriptor type
		 *
		 * \param data buffer
		 * \param size size of the buffer component
		 * \param block image area to read
		 * \param desc element description type
		 * \return success true/false
		 */			
		bool ReadBlock(void *data, const DataSize size, Block &block,
			const Descriptor desc = kRGB);

		/*!
		 * \brief Read the user data into a buffer.  
		 *
		 * Buffer must be large enough to hold the user data.
		 *
		 * \param data buffer
		 * \return success true/false
		 */	
		bool ReadUserData(unsigned char *data);
		

	protected:			
		InStream *fd;
		
		Codec *codex[MAX_ELEMENTS];
		ElementReadStream *rio;
	};
	
	
	
	





	/*!
	 * \class Writer
	 * \brief DPX Image Writer class
	 */
	 
	class Writer
	{
	
	public:
	
		/*!
		 * \brief DPX Header
		 */		
		Header header;

		/*!
		 * \brief Constructor
		 */			
		Writer();
		
		/*!
		 * \brief Destructor
		 */	
		virtual ~Writer();
		
		/*!
		 * \brief Start defining the header and writing the images
		 */		
		void Start();

		/*!
		 * \brief Set the basic file information about DPX
		 *
		 * \param fileName name of this created file (100 characters max)
		 * \param creationTimeDate creation time and date - format is "YYYY:MM:DD:HH:MM:SSLTZ"
		 *               where HH is 24 hour time, LTZ is local time zone using either
		 *               three character notation (i.e., -04) or five character notation
		 *               representing hours and minutes offset from Greenwich Mean time
		 *               (i.e., -0700) (24 characters max)
		 * \param creator creator (100 characters max)
		 * \param project project name (200 characters max)
		 * \param copyright copyright statement (200 characters max)
		 * \param encryptKey encryption key
		 * \param swapEndian whether to write the image header in reverse to native endianness
		 */			
		void SetFileInfo(const char *fileName, const char *creationTimeDate = 0, const char *creator = 0,
			const char *project = 0, const char *copyright = 0, const U32 encryptKey = ~0,
			const bool swapEndian = false);

		/*!
		 * \brief Set the Width and Height of the images
		 *
		 * \param width width of the image
		 * \param height height of the image
		 */			
		void SetImageInfo(const U32 width, const U32 height);

		/*!
		 * \brief Get the next available element
		 * \return next available
		 */
		int NextAvailElement() const;


		/*!
		 * \brief Set the parameters on an element  
		 *
		 * There are 8 elements maximum in an single DPX and each element used must be set before writing the header
		 *
		 * \param element element number (0-7)
		 * \param desc image descriptor
		 * \param bitDepth bit depth of image, valid values are [8,10,12,16,32,64]
		 * \param transfer transfer characteristic
		 * \param colorimetric colorimetric specification
		 * \param packing packing type
		 * \param encoding encoding type
		 * \param dataSign
		 * \param lowData
		 * \param lowQuantity
		 * \param highData
		 * \param highQuantity
		 * \param eolnPadding end of line padding (in bytes)
		 * \param eoimPadding end of image padding (in bytes)
		 */			
		void SetElement(const int element = 0, 
			const Descriptor desc = kRGB, 
			const U8 bitDepth = 10,
			const Characteristic transfer = kLogarithmic, 
			const Characteristic colorimetric = kLogarithmic,
			const Packing packing = kFilledMethodA, 
			const Encoding encoding = kNone,
			const U32 dataSign = 0, 
			const U32 lowData = ~0, const R32 lowQuantity = std::numeric_limits<float>::quiet_NaN(),
			const U32 highData = ~0, const R32 highQuantity = std::numeric_limits<float>::quiet_NaN(),
			const U32 eolnPadding = 0, const U32 eoimPadding = 0);

		/*!
		 * \brief Set the OutStream object will use to write the files
		 *
		 * \param stream OutStream object
		 */	
		void SetOutStream(OutStream *stream);

		/*!
		 * \brief Set the size of the user data area
		 * 
		 * \param size size of user data
		 */	
		void SetUserData(const long size);

		/*!
		 * \brief Write the header
		 *
		 * \return success true/false
		 */	
		bool WriteHeader();
		
		/*!
		 * \brief Write the user data
		 *
		 * \param data buffer - must match size set in Writer::SetUserData()
		 * \return success true/false
		 */
		bool WriteUserData(void *data);

		/*!
		 * \brief Write the entire element to the dpx file
		 *
		 * \param element element number (0-7)
		 * \param data buffer
		 * \return success true/false
		 */			
		bool WriteElement(const int element, void *data);
		bool WriteElement(const int element, void *data, const DataSize size);
		bool WriteElement(const int element, void *data, const long count);

		/**
		 * \brief Finish up writing image
		 *
		 * \return success true/false
		 */			
		bool Finish();


	protected:
		long fileLoc;
		OutStream *fd;
		
		bool WriteThrough(void *, const U32, const U32, const int, const int, const U32, const U32, char *);
		
	};
	
}



inline const char *dpx::DefaultExtension()
{
	return "dpx";
}


inline const char *dpx::Version()
{
	return SMPTE_VERSION;
}


inline const char *dpx::LibraryVersion()
{
	return OPENDPX_VERSION;
}


inline dpx::Block::Block() : x1(0), y1(0), x2(0), y2(0)
{ 
}


inline dpx::Block::Block(const int x1, const int y1, const int x2, const int y2) : x1(x1), y1(y1), x2(x2), y2(y2)
{ 
	this->Check();
}


inline void dpx::Block::Set(const int x1, const int y1, const int x2, const int y2)
{
	this->x1 = x1; 
	this->y1 = y1; 
	this->x2 = x2;
	this->y2 = y2;
}


// check the coordinates that x1 < x2 and y1 < y2
inline void dpx::Block::Check()
{
	if (this->x1 > this->x2)
	{
		int t = x1;
		this->x1 = this->x2;
		this->x2 = t;
	}
	if (this->y1 > this->y2)
	{
		int t = y1;
		this->y1 = this->y2;
		this->y2 = t;
	}
}


inline bool dpx::Block::Inside(const int x, const int y) const
{
	if (x >= this->x1 && x <= this->x2 && y >= this->y1 && y <= this->y2)
			return true;
	return false;
}


#endif

