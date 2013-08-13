///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004, Industrial Light & Magic, a division of Lucas
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


//-----------------------------------------------------------------------------
//
//	class ScanLineInputFile
//
//-----------------------------------------------------------------------------

#include "ImfScanLineInputFile.h"
#include "ImfChannelList.h"
#include "ImfMisc.h"
#include "ImfStdIO.h"
#include "ImfCompressor.h"
#include "ImathBox.h"
#include "ImathFun.h"
#include <ImfXdr.h>
#include <ImfConvert.h>
#include <ImfThreading.h>
#include <ImfPartType.h>
#include "IlmThreadPool.h"
#include "IlmThreadSemaphore.h"
#include "IlmThreadMutex.h"
#include "Iex.h"
#include <string>
#include <vector>
#include <assert.h>
#include "ImfVersion.h"
#include "ImfOptimizedPixelReading.h"
#include "ImfNamespace.h"
#include "ImfStandardAttributes.h"


OPENEXR_IMF_INTERNAL_NAMESPACE_SOURCE_ENTER


using IMATH_NAMESPACE::Box2i;
using IMATH_NAMESPACE::divp;
using IMATH_NAMESPACE::modp;
using std::string;
using std::vector;
using std::ifstream;
using std::min;
using std::max;
using ILMTHREAD_NAMESPACE::Mutex;
using ILMTHREAD_NAMESPACE::Lock;
using ILMTHREAD_NAMESPACE::Semaphore;
using ILMTHREAD_NAMESPACE::Task;
using ILMTHREAD_NAMESPACE::TaskGroup;
using ILMTHREAD_NAMESPACE::ThreadPool;

namespace {

struct InSliceInfo
{
    PixelType	typeInFrameBuffer;
    PixelType	typeInFile;
    char *	base;
    size_t	xStride;
    size_t	yStride;
    int		xSampling;
    int		ySampling;
    bool	fill;
    bool	skip;
    double	fillValue;

    InSliceInfo (PixelType typeInFrameBuffer = HALF,
		 PixelType typeInFile = HALF,
	         char *base = 0,
	         size_t xStride = 0,
	         size_t yStride = 0,
	         int xSampling = 1,
	         int ySampling = 1,
	         bool fill = false,
	         bool skip = false,
	         double fillValue = 0.0);
};


InSliceInfo::InSliceInfo (PixelType tifb,
			  PixelType tifl,
			  char *b,
			  size_t xs, size_t ys,
			  int xsm, int ysm,
			  bool f, bool s,
			  double fv)
:
    typeInFrameBuffer (tifb),
    typeInFile (tifl),
    base (b),
    xStride (xs),
    yStride (ys),
    xSampling (xsm),
    ySampling (ysm),
    fill (f),
    skip (s),
    fillValue (fv)
{
    // empty
}


struct LineBuffer
{
    const char *	uncompressedData;
    char *		buffer;
    int			dataSize;
    int			minY;
    int			maxY;
    Compressor *	compressor;
    Compressor::Format	format;
    int			number;
    bool		hasException;
    string		exception;

    LineBuffer (Compressor * const comp);
    ~LineBuffer ();

    inline void		wait () {_sem.wait();}
    inline void		post () {_sem.post();}

  private:

    Semaphore		_sem;
};


LineBuffer::LineBuffer (Compressor *comp):
    uncompressedData (0),
    buffer (0),
    dataSize (0),
    compressor (comp),
    format (defaultFormat(compressor)),
    number (-1),
    hasException (false),
    exception (),
    _sem (1)
{
    // empty
}


LineBuffer::~LineBuffer ()
{
    delete compressor;
}

} // namespace


struct ScanLineInputFile::Data: public Mutex
{
    Header		header;		    // the image header
    int			version;            // file's version
    FrameBuffer		frameBuffer;	    // framebuffer to write into
    LineOrder		lineOrder;          // order of the scanlines in file
    int			minX;		    // data window's min x coord
    int			maxX;		    // data window's max x coord
    int			minY;		    // data window's min y coord
    int			maxY;		    // data window's max x coord
    vector<Int64>	lineOffsets;	    // stores offsets in file for
					    // each line
    bool		fileIsComplete;	    // True if no scanlines are missing
    					    // in the file
    int			nextLineBufferMinY; // minimum y of the next linebuffer
    vector<size_t>	bytesPerLine;       // combined size of a line over all
                                            // channels
    vector<size_t>	offsetInLineBuffer; // offset for each scanline in its
                                            // linebuffer
    vector<InSliceInfo>	slices;             // info about channels in file
    
    vector<LineBuffer*> lineBuffers;        // each holds one line buffer
    int			linesInBuffer;      // number of scanlines each buffer
                                            // holds
    size_t		lineBufferSize;     // size of the line buffer
    int                 partNumber;         // part number

    bool                memoryMapped;       // if the stream is memory mapped
    OptimizationMode    optimizationMode;   // optimizibility of the input file

    Data (int numThreads);
    ~Data ();
    
    inline LineBuffer * getLineBuffer (int number); // hash function from line
    						    // buffer indices into our
						    // vector of line buffers
};


ScanLineInputFile::Data::Data (int numThreads):
        partNumber(-1),
        memoryMapped(false)
{
    //
    // We need at least one lineBuffer, but if threading is used,
    // to keep n threads busy we need 2*n lineBuffers
    //

    lineBuffers.resize (max (1, 2 * numThreads));
}


ScanLineInputFile::Data::~Data ()
{
    for (size_t i = 0; i < lineBuffers.size(); i++)
        delete lineBuffers[i];
}


inline LineBuffer *
ScanLineInputFile::Data::getLineBuffer (int lineBufferNumber)
{
    return lineBuffers[lineBufferNumber % lineBuffers.size()];
}


namespace {


void
reconstructLineOffsets (OPENEXR_IMF_INTERNAL_NAMESPACE::IStream &is,
			LineOrder lineOrder,
			vector<Int64> &lineOffsets)
{
    Int64 position = is.tellg();

    try
    {
	for (unsigned int i = 0; i < lineOffsets.size(); i++)
	{
	    Int64 lineOffset = is.tellg();

	    int y;
	    OPENEXR_IMF_INTERNAL_NAMESPACE::Xdr::read <OPENEXR_IMF_INTERNAL_NAMESPACE::StreamIO> (is, y);

	    int dataSize;
	    OPENEXR_IMF_INTERNAL_NAMESPACE::Xdr::read <OPENEXR_IMF_INTERNAL_NAMESPACE::StreamIO> (is, dataSize);

	    Xdr::skip <StreamIO> (is, dataSize);

	    if (lineOrder == INCREASING_Y)
		lineOffsets[i] = lineOffset;
	    else
		lineOffsets[lineOffsets.size() - i - 1] = lineOffset;
	}
    }
    catch (...)
    {
	//
	// Suppress all exceptions.  This functions is
	// called only to reconstruct the line offset
	// table for incomplete files, and exceptions
	// are likely.
	//
    }

    is.clear();
    is.seekg (position);
}


void
readLineOffsets (OPENEXR_IMF_INTERNAL_NAMESPACE::IStream &is,
		 LineOrder lineOrder,
		 vector<Int64> &lineOffsets,
		 bool &complete)
{
    for (unsigned int i = 0; i < lineOffsets.size(); i++)
    {
	OPENEXR_IMF_INTERNAL_NAMESPACE::Xdr::read <OPENEXR_IMF_INTERNAL_NAMESPACE::StreamIO> (is, lineOffsets[i]);
    }

    complete = true;

    for (unsigned int i = 0; i < lineOffsets.size(); i++)
    {
	if (lineOffsets[i] <= 0)
	{
	    //
	    // Invalid data in the line offset table mean that
	    // the file is probably incomplete (the table is
	    // the last thing written to the file).  Either
	    // some process is still busy writing the file,
	    // or writing the file was aborted.
	    //
	    // We should still be able to read the existing
	    // parts of the file.  In order to do this, we
	    // have to make a sequential scan over the scan
	    // line data to reconstruct the line offset table.
	    //

	    complete = false;
	    reconstructLineOffsets (is, lineOrder, lineOffsets);
	    break;
	}
    }
}


void
readPixelData (InputStreamMutex *streamData,
               ScanLineInputFile::Data *ifd,
	       int minY,
	       char *&buffer,
	       int &dataSize)
{
    //
    // Read a single line buffer from the input file.
    //
    // If the input file is not memory-mapped, we copy the pixel data into
    // into the array pointed to by buffer.  If the file is memory-mapped,
    // then we change where buffer points to instead of writing into the
    // array (hence buffer needs to be a reference to a char *).
    //

    int lineBufferNumber = (minY - ifd->minY) / ifd->linesInBuffer;

    Int64 lineOffset = ifd->lineOffsets[lineBufferNumber];

    if (lineOffset == 0)
	THROW (IEX_NAMESPACE::InputExc, "Scan line " << minY << " is missing.");

    //
    // Seek to the start of the scan line in the file,
    // if necessary.
    //

    if ( !isMultiPart(ifd->version) )
    {
        if (ifd->nextLineBufferMinY != minY)
            streamData->is->seekg (lineOffset);
    }
    else
    {
        //
        // In a multi-part file, the file pointer may have been moved by
        // other parts, so we have to ask tellg() where we are.
        //
        if (streamData->is->tellg() != ifd->lineOffsets[lineBufferNumber])
            streamData->is->seekg (lineOffset);
    }

    //
    // Read the data block's header.
    //

    int yInFile;

    //
    // Read the part number when we are dealing with a multi-part file.
    //
    if (isMultiPart(ifd->version))
    {
        int partNumber;
        OPENEXR_IMF_INTERNAL_NAMESPACE::Xdr::read <OPENEXR_IMF_INTERNAL_NAMESPACE::StreamIO> (*streamData->is, partNumber);
        if (partNumber != ifd->partNumber)
        {
            THROW (IEX_NAMESPACE::ArgExc, "Unexpected part number " << partNumber
                   << ", should be " << ifd->partNumber << ".");
        }
    }

    OPENEXR_IMF_INTERNAL_NAMESPACE::Xdr::read <OPENEXR_IMF_INTERNAL_NAMESPACE::StreamIO> (*streamData->is, yInFile);
    OPENEXR_IMF_INTERNAL_NAMESPACE::Xdr::read <OPENEXR_IMF_INTERNAL_NAMESPACE::StreamIO> (*streamData->is, dataSize);
    
    if (yInFile != minY)
        throw IEX_NAMESPACE::InputExc ("Unexpected data block y coordinate.");

    if (dataSize > (int) ifd->lineBufferSize)
	throw IEX_NAMESPACE::InputExc ("Unexpected data block length.");

    //
    // Read the pixel data.
    //

    if (streamData->is->isMemoryMapped ())
        buffer = streamData->is->readMemoryMapped (dataSize);
    else
        streamData->is->read (buffer, dataSize);

    //
    // Keep track of which scan line is the next one in
    // the file, so that we can avoid redundant seekg()
    // operations (seekg() can be fairly expensive).
    //

    if (ifd->lineOrder == INCREASING_Y)
        ifd->nextLineBufferMinY = minY + ifd->linesInBuffer;
    else
        ifd->nextLineBufferMinY = minY - ifd->linesInBuffer;
}

OptimizationMode
detectOptimizationMode (const FrameBuffer&frameBuffer,
                        const ChannelList& channels,
                        const std::vector<std::string> * views)
{
    OptimizationMode optimizationMode;
    
    optimizationMode._source = getOptimizationInfo(channels,views);
    optimizationMode._destination = getOptimizationInfo(frameBuffer,views);
    
    // Special case where only channels RGB are specified in the framebuffer
    // but the stride is 4 * sizeof(half), meaning we want to have RGBA but
    // a dummy value for A
    if (optimizationMode._destination._format  == OptimizationMode::PIXELFORMAT_RGB &&
        optimizationMode._destination._xStride == 8)
    {
        optimizationMode._destination._format = OptimizationMode::PIXELFORMAT_RGBA;
        optimizationMode._destination._alphaFillValueRight = 1.0f;
        optimizationMode._destination._alphaFillValueLeft  = 1.0f;
    }
    
    return optimizationMode;
}

                        

//
// A LineBufferTask encapsulates the task uncompressing a set of
// scanlines (line buffer) and copying them into the frame buffer.
//

class LineBufferTask : public Task
{
  public:

    LineBufferTask (TaskGroup *group,
                    ScanLineInputFile::Data *ifd,
		    LineBuffer *lineBuffer,
                    int scanLineMin,
		    int scanLineMax,
                    OptimizationMode optimizationMode);

    virtual ~LineBufferTask ();

    virtual void		execute ();

  private:

    ScanLineInputFile::Data *	_ifd;
    LineBuffer *		_lineBuffer;
    int				_scanLineMin;
    int				_scanLineMax;
    OptimizationMode            _optimizationMode;
};


LineBufferTask::LineBufferTask
    (TaskGroup *group,
     ScanLineInputFile::Data *ifd,
     LineBuffer *lineBuffer,
     int scanLineMin,
     int scanLineMax,OptimizationMode optimizationMode)
:
    Task (group),
    _ifd (ifd),
    _lineBuffer (lineBuffer),
    _scanLineMin (scanLineMin),
    _scanLineMax (scanLineMax),
    _optimizationMode(optimizationMode)
{
    // empty
}


LineBufferTask::~LineBufferTask ()
{
    //
    // Signal that the line buffer is now free
    //

    _lineBuffer->post ();
}


void
LineBufferTask::execute ()
{
    try
    {
        //
        // Uncompress the data, if necessary
        //
    
        if (_lineBuffer->uncompressedData == 0)
        {
            int uncompressedSize = 0;
            int maxY = min (_lineBuffer->maxY, _ifd->maxY);
    
            for (int i = _lineBuffer->minY - _ifd->minY;
                 i <= maxY - _ifd->minY;
		 ++i)
	    {
                uncompressedSize += (int) _ifd->bytesPerLine[i];
	    }
    
            if (_lineBuffer->compressor &&
                _lineBuffer->dataSize < uncompressedSize)
            {
                _lineBuffer->format = _lineBuffer->compressor->format();

                _lineBuffer->dataSize = _lineBuffer->compressor->uncompress
                    (_lineBuffer->buffer,
                     _lineBuffer->dataSize,
		     _lineBuffer->minY,
                     _lineBuffer->uncompressedData);
            }
            else
            {
                //
                // If the line is uncompressed, it's in XDR format,
                // regardless of the compressor's output format.
                //
    
                _lineBuffer->format = Compressor::XDR;
                _lineBuffer->uncompressedData = _lineBuffer->buffer;
            }
        }
        
        int yStart, yStop, dy;

        if (_ifd->lineOrder == INCREASING_Y)
        {
            yStart = _scanLineMin;
            yStop = _scanLineMax + 1;
            dy = 1;
        }
        else
        {
            yStart = _scanLineMax;
            yStop = _scanLineMin - 1;
            dy = -1;
        }
    
        for (int y = yStart; y != yStop; y += dy)
        {
            //
            // Convert one scan line's worth of pixel data back
            // from the machine-independent representation, and
            // store the result in the frame buffer.
            //
    
            const char *readPtr = _lineBuffer->uncompressedData +
                                  _ifd->offsetInLineBuffer[y - _ifd->minY];
    
            //
            // Iterate over all image channels.
            //
    
            for (unsigned int i = 0; i < _ifd->slices.size(); ++i)
            {
                //
                // Test if scan line y of this channel contains any data
		// (the scan line contains data only if y % ySampling == 0).
                //
    
                const InSliceInfo &slice = _ifd->slices[i];
    
                if (modp (y, slice.ySampling) != 0)
                    continue;
    
                //
                // Find the x coordinates of the leftmost and rightmost
                // sampled pixels (i.e. pixels within the data window
                // for which x % xSampling == 0).
                //
    
                int dMinX = divp (_ifd->minX, slice.xSampling);
                int dMaxX = divp (_ifd->maxX, slice.xSampling);
    
                //
		// Fill the frame buffer with pixel data.
                //
    
                if (slice.skip)
                {
                    //
                    // The file contains data for this channel, but
                    // the frame buffer contains no slice for this channel.
                    //
    
                    skipChannel (readPtr, slice.typeInFile, dMaxX - dMinX + 1);
                }
                else
                {
                    //
                    // The frame buffer contains a slice for this channel.
                    //
    
                    char *linePtr  = slice.base +
                                        divp (y, slice.ySampling) *
                                        slice.yStride;
    
                    char *writePtr = linePtr + dMinX * slice.xStride;
                    char *endPtr   = linePtr + dMaxX * slice.xStride;
                    
                    copyIntoFrameBuffer (readPtr, writePtr, endPtr,
                                         slice.xStride, slice.fill,
                                         slice.fillValue, _lineBuffer->format,
                                         slice.typeInFrameBuffer,
                                         slice.typeInFile);
                }
            }
        }
    }
    catch (std::exception &e)
    {
        if (!_lineBuffer->hasException)
        {
            _lineBuffer->exception = e.what();
            _lineBuffer->hasException = true;
        }
    }
    catch (...)
    {
        if (!_lineBuffer->hasException)
        {
            _lineBuffer->exception = "unrecognized exception";
            _lineBuffer->hasException = true;
        }
    }
}


#ifdef IMF_HAVE_SSE2
//
// IIF format is more restricted than a perfectly generic one,
// so it is possible to perform some optimizations.
//
class LineBufferTaskIIF : public Task
{
    public:
        
        LineBufferTaskIIF (TaskGroup *group,
                           ScanLineInputFile::Data *ifd,
                           LineBuffer *lineBuffer,
                           int scanLineMin,
                           int scanLineMax,
                           OptimizationMode optimizationMode);
                           
        virtual ~LineBufferTaskIIF ();
                           
        virtual void                execute ();
        
        template<typename TYPE>
        void getWritePointer (int y,
                              unsigned short*& pOutWritePointerRight,
                              size_t& outPixelsToCopySSE,
                              size_t& outPixelsToCopyNormal) const;
                              
        template<typename TYPE>
        void getWritePointerStereo (int y,
                                    unsigned short*& outWritePointerRight,
                                    unsigned short*& outWritePointerLeft,
                                    size_t& outPixelsToCopySSE,
                                    size_t& outPixelsToCopyNormal) const;

    private:
        
        ScanLineInputFile::Data *   _ifd;
        LineBuffer *                _lineBuffer;
        int                         _scanLineMin;
        int                         _scanLineMax;
        OptimizationMode            _optimizationMode;
};

LineBufferTaskIIF::LineBufferTaskIIF
    (TaskGroup *group,
     ScanLineInputFile::Data *ifd,
     LineBuffer *lineBuffer,
     int scanLineMin,
     int scanLineMax,
     OptimizationMode optimizationMode)
    :
     Task (group),
     _ifd (ifd),
     _lineBuffer (lineBuffer),
     _scanLineMin (scanLineMin),
     _scanLineMax (scanLineMax),
     _optimizationMode (optimizationMode)
{
     /*
     //
     // indicates the optimised path has been taken
     //
     static bool could_optimise=false;
     if(could_optimise==false)
     {
         std::cerr << " optimised path\n";
         could_optimise=true;
     }
     */
}
 
LineBufferTaskIIF::~LineBufferTaskIIF ()
{
     //
     // Signal that the line buffer is now free
     //
     
     _lineBuffer->post ();
}
 
// Return 0 if we are to skip because of sampling
template<typename TYPE>
void LineBufferTaskIIF::getWritePointer 
                            (int y,
                             unsigned short*& outWritePointerRight,
                             size_t& outPixelsToCopySSE,
                             size_t& outPixelsToCopyNormal) const
{
      // Channels are saved alphabetically, so the order is B G R.
      // The last slice (R) will give us the location of our write pointer.
      // The only slice that we support skipping is alpha, i.e. the first one.  
      // This does not impact the write pointer or the pixels to copy at all.
      size_t nbSlicesInFile = _ifd->slices.size();
      size_t nbSlicesInFrameBuffer = 0;
      
      if (_optimizationMode._destination._format ==
          OptimizationMode::PIXELFORMAT_RGB)
      {
          nbSlicesInFrameBuffer = 3;
      }
      else if (_optimizationMode._destination._format ==
          OptimizationMode::PIXELFORMAT_RGBA)
      {
          nbSlicesInFrameBuffer = 4;
      }
      
      int sizeOfSingleValue = sizeof(TYPE);
      
      
      const InSliceInfo& redSlice = _ifd->slices[nbSlicesInFile - 1];
      
      if (modp (y, redSlice.ySampling) != 0)
      {
          outPixelsToCopySSE    = 0;
          outPixelsToCopyNormal = 0;
          outWritePointerRight  = 0;
      }
      
      char* linePtr1  = redSlice.base +
      divp (y, redSlice.ySampling) *
      redSlice.yStride;
      
      int dMinX1 = divp (_ifd->minX, redSlice.xSampling);
      int dMaxX1 = divp (_ifd->maxX, redSlice.xSampling);
      
      // Construct the writePtr so that we start writing at
      // linePtr + Min offset in the line.
      outWritePointerRight =  (unsigned short*)(linePtr1 +
      dMinX1 * redSlice.xStride);
      
      size_t bytesToCopy  = ((linePtr1 + dMaxX1 * redSlice.xStride) -
      (linePtr1 + dMinX1 * redSlice.xStride)) + 2;
      size_t shortsToCopy = bytesToCopy / sizeOfSingleValue;
      size_t pixelsToCopy = (shortsToCopy / nbSlicesInFrameBuffer) + 1;
      
      // We only support writing to SSE if we have no pixels to copy normally
      outPixelsToCopySSE    = pixelsToCopy / 8;
      outPixelsToCopyNormal = pixelsToCopy % 8;
      
}


template<typename TYPE>
void LineBufferTaskIIF::getWritePointerStereo 
                          (int y,
                           unsigned short*& outWritePointerRight,
                           unsigned short*& outWritePointerLeft,
                           size_t& outPixelsToCopySSE,
                           size_t& outPixelsToCopyNormal) const
{
     // We can either have 6 slices or 8, depending on whether we are
     // working with mono or stereo
     size_t nbSlices = _ifd->slices.size();
     size_t nbSlicesInFrameBuffer = 0;
     
     if (_optimizationMode._destination._format ==
         OptimizationMode::PIXELFORMAT_RGB)
     {
         nbSlicesInFrameBuffer = 6;
     }
     else if (_optimizationMode._destination._format ==
         OptimizationMode::PIXELFORMAT_RGBA)
     {
         nbSlicesInFrameBuffer = 8;
     }
     
     int sizeOfSingleValue = sizeof(TYPE);
     
     const InSliceInfo& redSliceRight = _ifd->slices[(nbSlices / 2) - 1];
     
     if (modp (y, redSliceRight.ySampling) != 0)
     {
         outPixelsToCopySSE    = 0;
         outPixelsToCopyNormal = 0;
         outWritePointerRight  = 0;
         outWritePointerLeft   = 0;
     }
     
     char * linePtr1  = redSliceRight.base +
     divp (y, redSliceRight.ySampling) *
     redSliceRight.yStride;
     
     int dMinX1 = divp (_ifd->minX, redSliceRight.xSampling);
     int dMaxX1 = divp (_ifd->maxX, redSliceRight.xSampling);
     
     // Construct the writePtr so that we start writing at
     // linePtr + Min offset in the line.
     outWritePointerRight =  (unsigned short*)(linePtr1 +
     dMinX1 * redSliceRight.xStride);
     
     const InSliceInfo& redSliceLeft = _ifd->slices[(nbSlices) - 1];
     
     if (modp (y, redSliceLeft.ySampling) != 0)
     {
         outPixelsToCopySSE    = 0;
         outPixelsToCopyNormal = 0;
         outWritePointerRight  = 0;
         outWritePointerLeft   = 0;
     }
     
     char* linePtr2  = redSliceLeft.base +
     divp (y, redSliceLeft.ySampling) *
     redSliceLeft.yStride;
     
     dMinX1 = divp (_ifd->minX, redSliceLeft.xSampling);
     dMaxX1 = divp (_ifd->maxX, redSliceLeft.xSampling);
     
     // Construct the writePtr so that we start writing at
     // linePtr + Min offset in the line.
     outWritePointerLeft =  (unsigned short*)(linePtr2 +
     dMinX1 * redSliceLeft.xStride);
     
     size_t bytesToCopy  = ((linePtr1 + dMaxX1 * redSliceRight.xStride) -
     (linePtr1 + dMinX1 * redSliceRight.xStride)) + 2;
     size_t shortsToCopy = bytesToCopy / sizeOfSingleValue;
     
     // Divide nb slices by 2 since we are in stereo and we will have
     // the same number of pixels as a mono image but double the slices.
     size_t pixelsToCopy = (shortsToCopy / (nbSlicesInFrameBuffer / 2)) + 1;
     
     // We only support writing to SSE if we have no pixels to copy normally
     outPixelsToCopySSE    = pixelsToCopy / 8;
     outPixelsToCopyNormal = pixelsToCopy % 8;
}

void
LineBufferTaskIIF::execute()
{
    try
    {
        //
        // Uncompress the data, if necessary
        //
        
        if (_lineBuffer->uncompressedData == 0)
        {
            int uncompressedSize = 0;
            int maxY = min (_lineBuffer->maxY, _ifd->maxY);
            
            for (int i = _lineBuffer->minY - _ifd->minY;
            i <= maxY - _ifd->minY;
            ++i)
            {
                uncompressedSize += (int) _ifd->bytesPerLine[i];
            }
            
            if (_lineBuffer->compressor &&
                _lineBuffer->dataSize < uncompressedSize)
            {
                _lineBuffer->format = _lineBuffer->compressor->format();
                
                _lineBuffer->dataSize =
                _lineBuffer->compressor->uncompress (_lineBuffer->buffer,
                                                     _lineBuffer->dataSize,
                                                     _lineBuffer->minY,
                                                     _lineBuffer->uncompressedData);
            }
            else
            {
                //
                // If the line is uncompressed, it's in XDR format,
                // regardless of the compressor's output format.
                //
                
                _lineBuffer->format = Compressor::XDR;
                _lineBuffer->uncompressedData = _lineBuffer->buffer;
            }
        }
        
        int yStart, yStop, dy;
        
        if (_ifd->lineOrder == INCREASING_Y)
        {
            yStart = _scanLineMin;
            yStop = _scanLineMax + 1;
            dy = 1;
        }
        else
        {
            yStart = _scanLineMax;
            yStop = _scanLineMin - 1;
            dy = -1;
        }
        
        for (int y = yStart; y != yStop; y += dy)
        {
            if (modp (y, _optimizationMode._destination._ySampling) != 0)
                continue;
            
            //
            // Convert one scan line's worth of pixel data back
            // from the machine-independent representation, and
            // store the result in the frame buffer.
            //
            
            // Set the readPtr to read at the start of uncompressedData
            // but with an offet based on calculated array.
            // _ifd->offsetInLineBuffer contains offsets based on which
            // line we are currently processing.
            // Stride will be taken into consideration later.
            const char* readPtr = _lineBuffer->uncompressedData +
            _ifd->offsetInLineBuffer[y - _ifd->minY];
            
            size_t pixelsToCopySSE = 0;
            size_t pixelsToCopyNormal = 0;
            
            unsigned short* writePtrLeft = 0;
            unsigned short* writePtrRight = 0;
            
            int nbReadChannels = _optimizationMode._source.getNbChannels();
            
            // read pointers are now (if fully populated)
            // A (right)
            // B (right)
            // G (right)
            // R (right)
            // A (left)
            // B (left)
            // G (left)
            // R (left)
            
            if (_optimizationMode._destination._multiview == OptimizationMode::MULTIVIEW_MONO)
            {
                getWritePointer<half>(y, writePtrRight, pixelsToCopySSE, pixelsToCopyNormal);
            }
            else if (_optimizationMode._destination._multiview == OptimizationMode::MULTIVIEW_STEREO)
            {
                getWritePointerStereo<half>(y, writePtrRight, writePtrLeft, pixelsToCopySSE, pixelsToCopyNormal);
            }
            
            if (writePtrRight == 0 && pixelsToCopySSE == 0 && pixelsToCopyNormal == 0)
            {
                continue;
            }
            
            unsigned short* readPointers[8];
            
            for (int i = 0; i < nbReadChannels; ++i)
            {
                readPointers[i] = (unsigned short*)readPtr + (i * (pixelsToCopySSE * 8 + pixelsToCopyNormal));
            }
            
            if (_optimizationMode._destination._format == OptimizationMode::PIXELFORMAT_RGB)
            {
                // RGB to RGB
                if (_optimizationMode._source._format == OptimizationMode::PIXELFORMAT_RGB)
                {
                    optimizedWriteToRGB(readPointers[2], readPointers[1], readPointers[0], writePtrRight, pixelsToCopySSE, pixelsToCopyNormal);
                    
                    if (_optimizationMode._destination._multiview == OptimizationMode::MULTIVIEW_STEREO)
                    {
                        optimizedWriteToRGB(readPointers[5], readPointers[4], readPointers[3], writePtrLeft, pixelsToCopySSE, pixelsToCopyNormal);
                    }                
                }
                // RGBA to RGB (skip A)
                else if (_optimizationMode._source._format == OptimizationMode::PIXELFORMAT_RGBA)
                {
                    optimizedWriteToRGB(readPointers[3], readPointers[2], readPointers[1], writePtrRight, pixelsToCopySSE, pixelsToCopyNormal);
                    
                    if (_optimizationMode._destination._multiview == OptimizationMode::MULTIVIEW_STEREO)
                    {
                        optimizedWriteToRGB(readPointers[7], readPointers[6], readPointers[5], writePtrLeft, pixelsToCopySSE, pixelsToCopyNormal);
                    }   
                }
            }
            else if (_optimizationMode._destination._format == OptimizationMode::PIXELFORMAT_RGBA)
            {
                // RGB to RGBA (fill A)
                if (_optimizationMode._source._format == OptimizationMode::PIXELFORMAT_RGB)
                {
                    optimizedWriteToRGBAFillA(readPointers[2], readPointers[1], readPointers[0], half(_optimizationMode._destination._alphaFillValueRight).bits(), writePtrRight, pixelsToCopySSE, pixelsToCopyNormal);
                    
                    if (_optimizationMode._destination._multiview == OptimizationMode::MULTIVIEW_STEREO)
                    {
                        optimizedWriteToRGBAFillA(readPointers[5], readPointers[4], readPointers[3], half(_optimizationMode._destination._alphaFillValueLeft).bits(), writePtrLeft, pixelsToCopySSE, pixelsToCopyNormal);
                    }                
                }
                // RGBA to RGBA 
                else if (_optimizationMode._source._format == OptimizationMode::PIXELFORMAT_RGBA)
                {
                    optimizedWriteToRGBA(readPointers[3], readPointers[2], readPointers[1], readPointers[0], writePtrRight, pixelsToCopySSE, pixelsToCopyNormal);
                    
                    if (_optimizationMode._destination._multiview == OptimizationMode::MULTIVIEW_STEREO)
                    {
                        optimizedWriteToRGBA(readPointers[7], readPointers[6], readPointers[5], readPointers[4], writePtrLeft, pixelsToCopySSE, pixelsToCopyNormal);
                    }  
                }
            }
            
            // If we are in NO_OPTIMIZATION mode, this class will never
            // get instantiated, so no need to check for it and duplicate
            // the code.
        }
    }
    catch (std::exception &e)
    {
        if (!_lineBuffer->hasException)
        {
            _lineBuffer->exception = e.what();
            _lineBuffer->hasException = true;
        }
    }
    catch (...)
    {
        if (!_lineBuffer->hasException)
        {
            _lineBuffer->exception = "unrecognized exception";
            _lineBuffer->hasException = true;
        }
    }
}
#endif


Task *
newLineBufferTask (TaskGroup *group,
                   InputStreamMutex *streamData,
                   ScanLineInputFile::Data *ifd,
                   int number,
                   int scanLineMin,
                   int scanLineMax,
                   OptimizationMode optimizationMode)
{
     //
     // Wait for a line buffer to become available, fill the line
     // buffer with raw data from the file if necessary, and create
     // a new LineBufferTask whose execute() method will uncompress
     // the contents of the buffer and copy the pixels into the
     // frame buffer.
     //
     
     LineBuffer *lineBuffer = ifd->getLineBuffer (number);
     
     try
     {
         lineBuffer->wait ();
         
         if (lineBuffer->number != number)
         {
             lineBuffer->minY = ifd->minY + number * ifd->linesInBuffer;
             lineBuffer->maxY = lineBuffer->minY + ifd->linesInBuffer - 1;
             
             lineBuffer->number = number;
             lineBuffer->uncompressedData = 0;
             
             readPixelData (streamData, ifd, lineBuffer->minY,
                            lineBuffer->buffer,
                            lineBuffer->dataSize);
         }
     }
     catch (std::exception &e)
     {
         if (!lineBuffer->hasException)
         {
             lineBuffer->exception = e.what();
             lineBuffer->hasException = true;
         }
         lineBuffer->number = -1;
         lineBuffer->post();
         throw;
     }
     catch (...)
     {
         //
         // Reading from the file caused an exception.
         // Signal that the line buffer is free, and
         // re-throw the exception.
         //
         
         lineBuffer->exception = "unrecognized exception";
         lineBuffer->hasException = true;
         lineBuffer->number = -1;
         lineBuffer->post();
         throw;
     }
     
     scanLineMin = max (lineBuffer->minY, scanLineMin);
     scanLineMax = min (lineBuffer->maxY, scanLineMax);
     
     
     Task* retTask = 0;
     
     
     if (optimizationMode._destination._format != OptimizationMode::PIXELFORMAT_OTHER &&
         optimizationMode._source._format != OptimizationMode::PIXELFORMAT_OTHER)
     {
#ifdef IMF_HAVE_SSE2
         
         retTask = new LineBufferTaskIIF (group, ifd, lineBuffer,
                                          scanLineMin, scanLineMax,
                                          optimizationMode);
#endif
      //if SSE2 not defined, both source and destination formats will be PIXELFORMAT_OTHER (
     }
     else
     {
         retTask = new LineBufferTask (group, ifd, lineBuffer,
                                       scanLineMin, scanLineMax,
                                       optimizationMode);
     }
     
     return retTask;
     
 }
 
  


} // namespace


void ScanLineInputFile::initialize(const Header& header)
{
    try
    {
        _data->header = header;

        _data->lineOrder = _data->header.lineOrder();

        const Box2i &dataWindow = _data->header.dataWindow();

        _data->minX = dataWindow.min.x;
        _data->maxX = dataWindow.max.x;
        _data->minY = dataWindow.min.y;
        _data->maxY = dataWindow.max.y;

        size_t maxBytesPerLine = bytesPerLineTable (_data->header,
                                                    _data->bytesPerLine);

        for (size_t i = 0; i < _data->lineBuffers.size(); i++)
        {
            _data->lineBuffers[i] = new LineBuffer (newCompressor
                                                (_data->header.compression(),
                                                 maxBytesPerLine,
                                                 _data->header));
        }

        _data->linesInBuffer =
            numLinesInBuffer (_data->lineBuffers[0]->compressor);

        _data->lineBufferSize = maxBytesPerLine * _data->linesInBuffer;

        if (!_streamData->is->isMemoryMapped())
        {
            for (size_t i = 0; i < _data->lineBuffers.size(); i++)
            {
                _data->lineBuffers[i]->buffer = (char *) EXRAllocAligned(_data->lineBufferSize*sizeof(char),16);
            }
        }
        _data->nextLineBufferMinY = _data->minY - 1;

        offsetInLineBufferTable (_data->bytesPerLine,
                                 _data->linesInBuffer,
                                 _data->offsetInLineBuffer);

        int lineOffsetSize = (dataWindow.max.y - dataWindow.min.y +
                              _data->linesInBuffer) / _data->linesInBuffer;

        _data->lineOffsets.resize (lineOffsetSize);
    }
    catch (...)
    {
        delete _data;
        _data=NULL;
        throw;
    }
}


ScanLineInputFile::ScanLineInputFile(InputPartData* part)
{
    if (part->header.type() != SCANLINEIMAGE)
        throw IEX_NAMESPACE::ArgExc("Can't build a ScanLineInputFile from a type-mismatched part.");

    _data = new Data(part->numThreads);
    _streamData = part->mutex;
    _data->memoryMapped = _streamData->is->isMemoryMapped();

    _data->version = part->version;

    initialize(part->header);

    _data->lineOffsets = part->chunkOffsets;

    _data->partNumber = part->partNumber;
    //
    // (TODO) change this code later.
    // The completeness of the file should be detected in MultiPartInputFile.
    //
    _data->fileIsComplete = true;
}


ScanLineInputFile::ScanLineInputFile
    (const Header &header,
     OPENEXR_IMF_INTERNAL_NAMESPACE::IStream *is,
     int numThreads)
:
    _data (new Data (numThreads)),
    _streamData (new InputStreamMutex())
{
    _streamData->is = is;
    _data->memoryMapped = is->isMemoryMapped();

    initialize(header);
    
    //
    // (TODO) this is nasty - we need a better way of working out what type of file has been used.
    // in any case I believe this constructor only gets used with single part files
    // and 'version' currently only tracks multipart state, so setting to 0 (not multipart) works for us
    //
    
    _data->version=0;
    readLineOffsets (*_streamData->is,
                     _data->lineOrder,
                     _data->lineOffsets,
                     _data->fileIsComplete);
}


ScanLineInputFile::~ScanLineInputFile ()
{
    if (!_data->memoryMapped)
    {
        for (size_t i = 0; i < _data->lineBuffers.size(); i++)
        {
            EXRFreeAligned(_data->lineBuffers[i]->buffer);
        }
    }
            

    //
    // ScanLineInputFile should never delete the stream,
    // because it does not own the stream.
    // We just delete the Mutex here.
    //
    if (_data->partNumber == -1)
        delete _streamData;

    delete _data;
}


const char *
ScanLineInputFile::fileName () const
{
    return _streamData->is->fileName();
}


const Header &
ScanLineInputFile::header () const
{
    return _data->header;
}


int
ScanLineInputFile::version () const
{
    return _data->version;
}


void	
ScanLineInputFile::setFrameBuffer (const FrameBuffer &frameBuffer)
{
    Lock lock (*_streamData);

    
    
    const ChannelList &channels = _data->header.channels();
    for (FrameBuffer::ConstIterator j = frameBuffer.begin();
	 j != frameBuffer.end();
	 ++j)
    {
	ChannelList::ConstIterator i = channels.find (j.name());

	if (i == channels.end())
	    continue;

	if (i.channel().xSampling != j.slice().xSampling ||
	    i.channel().ySampling != j.slice().ySampling)
	    THROW (IEX_NAMESPACE::ArgExc, "X and/or y subsampling factors "
				"of \"" << i.name() << "\" channel "
				"of input file \"" << fileName() << "\" are "
				"not compatible with the frame buffer's "
				"subsampling factors.");
    }

    //
    // Check if the new frame buffer descriptor is
    // compatible with the image file header.
    //
    
    
    if (!GLOBAL_SYSTEM_LITTLE_ENDIAN)
    {
        _data->optimizationMode._destination._format = OptimizationMode::PIXELFORMAT_OTHER;
        _data->optimizationMode._source._format      = OptimizationMode::PIXELFORMAT_OTHER;
    }
    else
    {
        StringVector * v=NULL;
        if(hasMultiView(_data->header))
        {
            v = &multiView(_data->header);
        }
        _data->optimizationMode = detectOptimizationMode(frameBuffer, channels,v);
    }
    
    // Uncomment the  line below to disable optimization code path
    _data->optimizationMode._destination._format = Imf::OptimizationMode::PIXELFORMAT_OTHER;
    

    //
    // Initialize the slice table for readPixels().
    //

    vector<InSliceInfo> slices;
    ChannelList::ConstIterator i = channels.begin();

    for (FrameBuffer::ConstIterator j = frameBuffer.begin();
	 j != frameBuffer.end();
	 ++j)
    {
	while (i != channels.end() && strcmp (i.name(), j.name()) < 0)
	{
	    //
	    // Channel i is present in the file but not
	    // in the frame buffer; data for channel i
	    // will be skipped during readPixels().
	    //

	    slices.push_back (InSliceInfo (i.channel().type,
					   i.channel().type,
					   0, // base
					   0, // xStride
					   0, // yStride
					   i.channel().xSampling,
					   i.channel().ySampling,
					   false,  // fill
					   true, // skip
					   0.0)); // fillValue
	    ++i;
	}

	bool fill = false;

	if (i == channels.end() || strcmp (i.name(), j.name()) > 0)
	{
	    //
	    // Channel i is present in the frame buffer, but not in the file.
	    // In the frame buffer, slice j will be filled with a default value.
	    //

	    fill = true;
	}

	slices.push_back (InSliceInfo (j.slice().type,
				       fill? j.slice().type:
				             i.channel().type,
				       j.slice().base,
				       j.slice().xStride,
				       j.slice().yStride,
				       j.slice().xSampling,
				       j.slice().ySampling,
				       fill,
				       false, // skip
				       j.slice().fillValue));

	if (i != channels.end() && !fill)
	    ++i;
    }

    //
    // Store the new frame buffer.
    //

    _data->frameBuffer = frameBuffer;
    _data->slices = slices;
}


const FrameBuffer &
ScanLineInputFile::frameBuffer () const
{
    Lock lock (*_streamData);
    return _data->frameBuffer;
}


bool
ScanLineInputFile::isComplete () const
{
    return _data->fileIsComplete;
}

bool ScanLineInputFile::isOptimizationEnabled() const
{
    if (_data->slices.size() == 0)
        throw IEX_NAMESPACE::ArgExc ("No frame buffer specified "
        "as pixel data destination.");
    
    return _data->optimizationMode._source._format!=OptimizationMode::PIXELFORMAT_OTHER && 
           _data->optimizationMode._destination._format!=OptimizationMode::PIXELFORMAT_OTHER;
}


void	
ScanLineInputFile::readPixels (int scanLine1, int scanLine2)
{
    try
    {
        Lock lock (*_streamData);

	if (_data->slices.size() == 0)
	    throw IEX_NAMESPACE::ArgExc ("No frame buffer specified "
			       "as pixel data destination.");

	int scanLineMin = min (scanLine1, scanLine2);
	int scanLineMax = max (scanLine1, scanLine2);

	if (scanLineMin < _data->minY || scanLineMax > _data->maxY)
	    throw IEX_NAMESPACE::ArgExc ("Tried to read scan line outside "
			       "the image file's data window.");

        //
        // We impose a numbering scheme on the lineBuffers where the first
        // scanline is contained in lineBuffer 1.
        //
        // Determine the first and last lineBuffer numbers in this scanline
        // range. We always attempt to read the scanlines in the order that
        // they are stored in the file.
        //

        int start, stop, dl;

        if (_data->lineOrder == INCREASING_Y)
        {
            start = (scanLineMin - _data->minY) / _data->linesInBuffer;
            stop  = (scanLineMax - _data->minY) / _data->linesInBuffer + 1;
            dl = 1;
        }
        else
        {
            start = (scanLineMax - _data->minY) / _data->linesInBuffer;
            stop  = (scanLineMin - _data->minY) / _data->linesInBuffer - 1;
            dl = -1;
        }

        //
        // Create a task group for all line buffer tasks.  When the
	// task group goes out of scope, the destructor waits until
	// all tasks are complete.
        //
        
        {
            TaskGroup taskGroup;
    
            //
            // Add the line buffer tasks.
            //
            // The tasks will execute in the order that they are created
            // because we lock the line buffers during construction and the
            // constructors are called by the main thread.  Hence, in order
	    // for a successive task to execute the previous task which
	    // used that line buffer must have completed already.
            //
    
            for (int l = start; l != stop; l += dl)
            {
                ThreadPool::addGlobalTask (newLineBufferTask (&taskGroup,
                                                              _streamData,
                                                              _data, l,
                                                              scanLineMin,
                                                              scanLineMax,
                                                              _data->optimizationMode));
            }
        
	    //
            // finish all tasks
	    //
        }
        
	//
	// Exeption handling:
	//
	// LineBufferTask::execute() may have encountered exceptions, but
	// those exceptions occurred in another thread, not in the thread
	// that is executing this call to ScanLineInputFile::readPixels().
	// LineBufferTask::execute() has caught all exceptions and stored
	// the exceptions' what() strings in the line buffers.
	// Now we check if any line buffer contains a stored exception; if
	// this is the case then we re-throw the exception in this thread.
	// (It is possible that multiple line buffers contain stored
	// exceptions.  We re-throw the first exception we find and
	// ignore all others.)
	//

	const string *exception = 0;

        for (size_t i = 0; i < _data->lineBuffers.size(); ++i)
	{
            LineBuffer *lineBuffer = _data->lineBuffers[i];

	    if (lineBuffer->hasException && !exception)
		exception = &lineBuffer->exception;

	    lineBuffer->hasException = false;
	}

	if (exception)
	    throw IEX_NAMESPACE::IoExc (*exception);
    }
    catch (IEX_NAMESPACE::BaseExc &e)
    {
	REPLACE_EXC (e, "Error reading pixel data from image "
		        "file \"" << fileName() << "\". " << e);
	throw;
    }
}


void	
ScanLineInputFile::readPixels (int scanLine)
{
    readPixels (scanLine, scanLine);
}


void
ScanLineInputFile::rawPixelData (int firstScanLine,
				 const char *&pixelData,
				 int &pixelDataSize)
{
    try
    {
        Lock lock (*_streamData);

	if (firstScanLine < _data->minY || firstScanLine > _data->maxY)
	{
	    throw IEX_NAMESPACE::ArgExc ("Tried to read scan line outside "
			       "the image file's data window.");
	}

        int minY = lineBufferMinY
	    (firstScanLine, _data->minY, _data->linesInBuffer);

	readPixelData
	    (_streamData, _data, minY, _data->lineBuffers[0]->buffer, pixelDataSize);

	pixelData = _data->lineBuffers[0]->buffer;
    }
    catch (IEX_NAMESPACE::BaseExc &e)
    {
	REPLACE_EXC (e, "Error reading pixel data from image "
		        "file \"" << fileName() << "\". " << e);
	throw;
    }
}

OPENEXR_IMF_INTERNAL_NAMESPACE_SOURCE_EXIT
