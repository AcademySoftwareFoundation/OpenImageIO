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


#ifndef _DPX_RUNLENGTHENCODING_H
#define _DPX_RUNLENGTHENCODING_H 1


#include "DPX.h"
#include "Codec.h"


namespace dpx
{

	/*!
	 * \brief compress / decompress data segments, used for RLE compression
	 */
	class RunLengthEncoding : public Codec
	{
	public:
		/*!
		 * \brief constructor
		 */
		RunLengthEncoding();
		
		/*!
		 * \brief destructor
		 */
		virtual ~RunLengthEncoding();
	
		/*!
		 * \brief reset instance
		 */	
		virtual void Reset();

		/*!
		 * \brief read data
		 * \param dpxHeader dpx header information
		 * \param fd field descriptor
		 * \param element element (0-7)
		 * \param block image area to read
		 * \param data buffer
		 * \param size size of the buffer component
		 * \return success
		 */		
		virtual bool Read(const dpx::Header &dpxHeader, 
						  ElementReadStream *fd, 
						  const int element, 
						  const Block &block, 
						  void *data, 
						  const DataSize size);
		
	protected:
		U8 *buf;			//!< intermediate buffer
	};


}


#endif


