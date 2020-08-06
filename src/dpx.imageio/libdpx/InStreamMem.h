/// -*- mode: C++; tab-width: 4 -*-
// vi: ts=4

// Copyright 2020-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

/*! \file InStreamMem.h */

#ifndef _DPX_INSTREAMMEM_H
#define _DPX_INSTREAMMEM_H 1


#include "DPXStream.h"


/*!
 * \class InStreamMem
 * \brief Input Stream for reading files from memory
 */
class InStreamMem : public InStream
{
public:

	/*!
	* \brief Constructor
	*/	
	InStreamMem();

	virtual ~InStreamMem();

	bool Open(const char * fn) override;

	bool Open(const void * memBuf, const size_t memSize) override;

	void Close() override;

	void Rewind() override;

	size_t Read(void * buf, const size_t size) override;

	size_t ReadDirect(void * buf, const size_t size) override;

	bool EndOfFile() const override;

	bool Seek(long offset, Origin origin) override;

	long Tell() override;

protected:

	const void	*memBuf;
	size_t		 memSize;
	size_t		 curPos;		//!< Current position in memory (/file)
};



#endif

