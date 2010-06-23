/// -*- mode: C++; tab-width: 4 -*-
// vi: ts=4

/*! \file DPXStream.h */

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


#ifndef _DPX_DPXSTREAM_H
#define _DPX_DPXSTREAM_H 1


#include <cstdio>



/*!
 * \class InStream
 * \brief Input Stream for reading files
 */
class InStream 
{

  public:

	/*!
	 * \enum Origin
	 * \brief file pointing positioning offset
	 */
	enum Origin
	{
		kStart,							//!< beginning of the file
		kCurrent,						//!< current file pointer
		kEnd							//!< end of the file
	};	

	
	/*!
	 * \brief Constructor
	 */	
	InStream();
	
	/*!
	 * \brief Destructor 
	 */	
	virtual ~InStream();

	/*!
	 * \brief Open file
	 * \param fn File name
	 * \return success true/false
	 */
	virtual bool Open(const char * fn);
	
	/*!
	 * \brief Close file
	 */	
	virtual void Close();
	
	/*!
	 * \brief Rewind file pointer to beginning of file
	 */	
	virtual void Rewind();

	/*!
	 * \brief Read data from file
	 * \param buf data buffer
	 * \param size bytes to read
	 * \return number of bytes read
	 */
	virtual size_t Read(void * buf, const size_t size);


	/*!
	 * \brief Read data from file without any buffering as fast as possible
	 * \param buf data buffer
	 * \param size bytes to read
	 * \return number of bytes read
	 */
	virtual size_t ReadDirect(void * buf, const size_t size);
	
	/*!
	 * \brief Query if end of file has been reached
	 * \return end of file true/false
	 */	
	virtual bool EndOfFile() const;
	
	/*!
	 * \brief Seek to a position in the file
	 * \param offset offset from originating position
	 * \param origin originating position
	 * \return success true/false
	 */ 	
	virtual bool Seek(long offset, Origin origin);

  protected:
	FILE *fp;
};



/*!
 * \class OutStream
 * \brief Output Stream for writing files
 */
class OutStream 
{
		
  public:
		
	/*!
	 * \enum Origin
	 * \brief file pointing positioning offset
	 */
	enum Origin
	{
		kStart,							//!< beginning of the file
		kCurrent,						//!< current file pointer
		kEnd							//!< end of the file
	};
		
	/*!
	 * \brief Constructor
	 */
	OutStream();
		
	/*!
	 * \brief Destructor 
	 */
	virtual ~OutStream();
		
	/*!
	 * \brief Open file
	 * \param fn File name
	 * \return success true/false
	 */
	virtual bool Open(const char *fn);
		
	/*!
	 * \brief Close file
	 */
	virtual void Close();
		
	/*!
	 * \brief Write data to file
	 * \param buf data buffer
	 * \param size bytes to write
	 * \return number of bytes written
	 */
	virtual size_t Write(void * buf, const size_t size);
		
	/*!
	 * \brief Seek to a position in the file
	 * \param offset offset from originating position
	 * \param origin originating position
	 * \return success true/false
	 */ 
	virtual bool Seek(long offset, Origin origin);
		
	/*!
	 * \brief Flush any buffers
	 */
	virtual void Flush();
		
		
  protected:
	FILE *fp;
};





#endif

