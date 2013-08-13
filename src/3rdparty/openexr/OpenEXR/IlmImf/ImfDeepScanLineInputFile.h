///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2011, Industrial Light & Magic, a division of Lucas
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


#ifndef INCLUDED_IMF_DEEP_SCAN_LINE_INPUT_FILE_H
#define INCLUDED_IMF_DEEP_SCAN_LINE_INPUT_FILE_H

//-----------------------------------------------------------------------------
//
//      class DeepScanLineInputFile
//
//-----------------------------------------------------------------------------

#include "ImfThreading.h"
#include "ImfGenericInputFile.h"
#include "ImfNamespace.h"
#include "ImfForward.h"
#include "ImfExport.h"
#include "ImfDeepScanLineOutputFile.h"

OPENEXR_IMF_INTERNAL_NAMESPACE_HEADER_ENTER


class IMF_EXPORT DeepScanLineInputFile : public GenericInputFile
{
  public:

    //------------
    // Constructor
    //------------

    DeepScanLineInputFile (const char fileName[],
                           int numThreads = globalThreadCount());

    DeepScanLineInputFile (const Header &header, OPENEXR_IMF_INTERNAL_NAMESPACE::IStream *is,
                           int version, /*version field from file*/
                           int numThreads = globalThreadCount());


    //-----------------------------------------
    // Destructor -- deallocates internal data
    // structures, but does not close the file.
    //-----------------------------------------

    virtual ~DeepScanLineInputFile ();


    //------------------------
    // Access to the file name
    //------------------------

    const char *        fileName () const;


    //--------------------------
    // Access to the file header
    //--------------------------

    const Header &      header () const;


    //----------------------------------
    // Access to the file format version
    //----------------------------------

    int                 version () const;


    //-----------------------------------------------------------
    // Set the current frame buffer -- copies the FrameBuffer
    // object into the InputFile object.
    //
    // The current frame buffer is the destination for the pixel
    // data read from the file.  The current frame buffer must be
    // set at least once before readPixels() is called.
    // The current frame buffer can be changed after each call
    // to readPixels().
    //-----------------------------------------------------------

    void                setFrameBuffer (const DeepFrameBuffer &frameBuffer);


    //-----------------------------------
    // Access to the current frame buffer
    //-----------------------------------

    const DeepFrameBuffer & frameBuffer () const;


    //---------------------------------------------------------------
    // Check if the file is complete:
    //
    // isComplete() returns true if all pixels in the data window are
    // present in the input file, or false if any pixels are missing.
    // (Another program may still be busy writing the file, or file
    // writing may have been aborted prematurely.)
    //---------------------------------------------------------------

    bool                isComplete () const;


    //---------------------------------------------------------------
    // Read pixel data:
    //
    // readPixels(s1,s2) reads all scan lines with y coordinates
    // in the interval [min (s1, s2), max (s1, s2)] from the file,
    // and stores them in the current frame buffer.
    //
    // Both s1 and s2 must be within the interval
    // [header().dataWindow().min.y, header.dataWindow().max.y]
    //
    // The scan lines can be read from the file in random order, and
    // individual scan lines may be skipped or read multiple times.
    // For maximum efficiency, the scan lines should be read in the
    // order in which they were written to the file.
    //
    // readPixels(s) calls readPixels(s,s).
    //
    // If threading is enabled, readPixels (s1, s2) tries to perform
    // decopmression of multiple scanlines in parallel.
    //
    //---------------------------------------------------------------

    void                readPixels (int scanLine1, int scanLine2);
    void                readPixels (int scanLine);

    
  
    //---------------------------------------------------------------
    // Extract pixel data from pre-read block
    //
    // readPixels(rawPixelData,frameBuffer,s1,s2) reads all scan lines with y coordinates
    // in the interval [min (s1, s2), max (s1, s2)] from the data provided and
    // stores them in the provided frameBuffer.
    // the data can be obtained from a call to rawPixelData()
    //
    //
    // Both s1 and s2 must be within the data specified
    //
    // you must provide a frameBuffer with a samplecountslice, which must have been read
    // and the data valid - readPixels uses your sample count buffer to compute
    // offsets to the data it needs
    //
    // This call does not block, and is thread safe for clients with an existing
    // threading model. The InputFile's frameBuffer is not used in this call.
    //
    // This call is only provided for clients which have an existing threading model in place
    // and unpredictable access patterns to the data.
    // The fastest way to read an entire image is to enable threading,use setFrameBuffer then
    // readPixels(header().dataWindow().min.y, header.dataWindow().max.y)
    //
    //---------------------------------------------------------------
    
    void                readPixels (const char * rawPixelData,
                                    const DeepFrameBuffer & frameBuffer,
                                    int scanLine1,
                                    int scanLine2) const;

    //----------------------------------------------
    // Read a block of raw pixel data from the file,
    // without uncompressing it (this function is
    // used to implement OutputFile::copyPixels()).
    // note: returns the entire payload of the relevant chunk of data, not including part number
    // including compressed and uncompressed sizes
    // on entry, if pixelDataSize is insufficiently large, no bytes are read (pixelData can safely be NULL)
    // on exit, pixelDataSize is the number of bytes required to read the chunk
    // 
    //----------------------------------------------

    void                rawPixelData (int firstScanLine,
                                      char * pixelData,
                                      Int64 &pixelDataSize);

                                      
    //-------------------------------------------------
    // firstScanLineInChunk() returns the row number of the first row that's stored in the
    // same chunk as scanline y. Depending on the compression mode, this may not be the same as y
    //
    // lastScanLineInChunk() returns the row number of the last row that's stored in the same
    // chunk as scanline y.  Depending on the compression mode, this may not be the same as y.
    // The last chunk in the file may be smaller than all the others
    //
    //------------------------------------------------
    int                 firstScanLineInChunk(int y) const;
    int                 lastScanLineInChunk (int y) const;
                                      
    //-----------------------------------------------------------
    // Read pixel sample counts into a slice in the frame buffer.
    //
    // readPixelSampleCounts(s1, s2) reads all the counts of
    // pixel samples with y coordinates in the interval
    // [min (s1, s2), max (s1, s2)] from the file, and stores
    // them in the slice naming "sample count".
    //
    // Both s1 and s2 must be within the interval
    // [header().dataWindow().min.y, header.dataWindow().max.y]
    //
    // readPixelSampleCounts(s) calls readPixelSampleCounts(s,s).
    // 
    //-----------------------------------------------------------

    void                readPixelSampleCounts (int scanline1,
                                               int scanline2);
    void                readPixelSampleCounts (int scanline);
    
    
    //----------------------------------------------------------
    // Read pixel sample counts into the provided frameBuffer
    // using a block read of data read by rawPixelData    
    // for multi-scanline compression schemes, you must decode the entire block
    // so scanline1=firstScanLineInChunk(y) and scanline2=lastScanLineInChunk(y)
    // 
    // This call does not block, and is thread safe for clients with an existing
    // threading model. The InputFile's frameBuffer is not used in this call.
    //
    // The fastest way to read an entire image is to enable threading in OpenEXR, use setFrameBuffer then
    // readPixelSampleCounts(header().dataWindow().min.y, header.dataWindow().max.y)
    //
    //----------------------------------------------------------
    void                readPixelSampleCounts (const char * rawdata , 
                                               const DeepFrameBuffer & frameBuffer,
                                               int scanLine1 , 
                                               int scanLine2) const;

    struct Data;

  private:

    Data *              _data;

    DeepScanLineInputFile   (InputPartData* part);

    void                initialize(const Header& header);
    void compatibilityInitialize(OPENEXR_IMF_INTERNAL_NAMESPACE::IStream & is);
    void multiPartInitialize(InputPartData* part);

    friend class         InputFile;
    friend class MultiPartInputFile;
    friend void DeepScanLineOutputFile::copyPixels(DeepScanLineInputFile &);
};


OPENEXR_IMF_INTERNAL_NAMESPACE_HEADER_EXIT

#endif
