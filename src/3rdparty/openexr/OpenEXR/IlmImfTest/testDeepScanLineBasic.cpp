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

#include "testDeepScanLineBasic.h"


#include <assert.h>
#include <string.h>

#include <ImfDeepScanLineInputFile.h>
#include <ImfDeepScanLineOutputFile.h>
#include <ImfDeepFrameBuffer.h>
#include <ImfPartType.h>
#include <ImfChannelList.h>
#include <ImfArray.h>
#include <IlmThreadPool.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>

#include "tmpDir.h"

using namespace OPENEXR_IMF_NAMESPACE;
using namespace std;
using namespace IMATH_NAMESPACE;
using namespace ILMTHREAD_NAMESPACE;

namespace
{

const int width = 273;
const int height = 173;
const int minX = 10;
const int minY = 11;
const Box2i dataWindow(V2i(minX, minY), V2i(minX + width - 1, minY + height - 1));
const Box2i displayWindow(V2i(0, 0), V2i(minX + width * 2, minY + height * 2));
const char filename[] = IMF_TMP_DIR "imf_test_deep_scanline_basic.exr";

vector<int> channelTypes;
Array2D<unsigned int> sampleCount;
Header header;

void generateRandomFile(int channelCount, Compression compression, bool bulkWrite)
{
    cout << "generating " << flush;
    header = Header(displayWindow, dataWindow,
                    1,
                    IMATH_NAMESPACE::V2f (0, 0),
                    1,
                    INCREASING_Y,
                    compression);

    cout << "compression " << compression << " " << flush;

    //
    // Add channels.
    //

    channelTypes.clear();

    for (int i = 0; i < channelCount; i++)
    {
        int type = rand() % 3;
        stringstream ss;
        ss << i;
        string str = ss.str();
        if (type == 0)
            header.channels().insert(str, Channel(UINT));
        if (type == 1)
            header.channels().insert(str, Channel(HALF));
        if (type == 2)
            header.channels().insert(str, Channel(FLOAT));
        channelTypes.push_back(type);
    }

    header.setType(DEEPSCANLINE);

    Array<Array2D< void* > > data(channelCount);
    for (int i = 0; i < channelCount; i++)
        data[i].resizeErase(height, width);

    sampleCount.resizeErase(height, width);

    remove (filename);
    DeepScanLineOutputFile file(filename, header, 8);

    DeepFrameBuffer frameBuffer;

    frameBuffer.insertSampleCountSlice (Slice (UINT,                    // type // 7
                                        (char *) (&sampleCount[0][0]
                                                  - dataWindow.min.x
                                                  - dataWindow.min.y * width),               // base // 8
                                        sizeof (unsigned int) * 1,          // xStride// 9
                                        sizeof (unsigned int) * width));    // yStride// 10

    for (int i = 0; i < channelCount; i++)
    {
        PixelType type;
        if (channelTypes[i] == 0)
            type = UINT;
        if (channelTypes[i] == 1)
            type = HALF;
        if (channelTypes[i] == 2)
            type = FLOAT;

        stringstream ss;
        ss << i;
        string str = ss.str();

        int sampleSize;
        if (channelTypes[i] == 0) sampleSize = sizeof (unsigned int);
        if (channelTypes[i] == 1) sampleSize = sizeof (half);
        if (channelTypes[i] == 2) sampleSize = sizeof (float);

        int pointerSize = sizeof(char *);

        frameBuffer.insert (str,                            // name // 6
                            DeepSlice (type,                    // type // 7
                            (char *) (&data[i][0][0]
                                      - dataWindow.min.x
                                      - dataWindow.min.y * width),               // base // 8
                            pointerSize * 1,          // xStride// 9
                            pointerSize * width,      // yStride// 10
                            sampleSize));             // sampleStride
    }

    file.setFrameBuffer(frameBuffer);

    cout << "writing " << flush;
    if (bulkWrite)
    {
        cout << "bulk " << flush;
        for (int i = 0; i < height; i++)
        {
            //
            // Fill in data at the last minute.
            //

            for (int j = 0; j < width; j++)
            {
                sampleCount[i][j] = rand() % 10 + 1;
                for (int k = 0; k < channelCount; k++)
                {
                    if (channelTypes[k] == 0)
                        data[k][i][j] = new unsigned int[sampleCount[i][j]];
                    if (channelTypes[k] == 1)
                        data[k][i][j] = new half[sampleCount[i][j]];
                    if (channelTypes[k] == 2)
                        data[k][i][j] = new float[sampleCount[i][j]];
                    for (int l = 0; l < sampleCount[i][j]; l++)
                    {
                        if (channelTypes[k] == 0)
                            ((unsigned int*)data[k][i][j])[l] = (i * width + j) % 2049;
                        if (channelTypes[k] == 1)
                            ((half*)data[k][i][j])[l] = (i * width + j) % 2049;
                        if (channelTypes[k] == 2)
                            ((float*)data[k][i][j])[l] = (i * width + j) % 2049;
                    }
                }
            }
        }

        file.writePixels(height);
    }
    else
    {
        cout << "per-line " << flush;
        for (int i = 0; i < height; i++)
        {
            //
            // Fill in data at the last minute.
            //

            for (int j = 0; j < width; j++)
            {
                sampleCount[i][j] = rand() % 10 + 1;
                for (int k = 0; k < channelCount; k++)
                {
                    if (channelTypes[k] == 0)
                        data[k][i][j] = new unsigned int[sampleCount[i][j]];
                    if (channelTypes[k] == 1)
                        data[k][i][j] = new half[sampleCount[i][j]];
                    if (channelTypes[k] == 2)
                        data[k][i][j] = new float[sampleCount[i][j]];
                    for (int l = 0; l < sampleCount[i][j]; l++)
                    {
                        if (channelTypes[k] == 0)
                            ((unsigned int*)data[k][i][j])[l] = (i * width + j) % 2049;
                        if (channelTypes[k] == 1)
                            ((half*)data[k][i][j])[l] = (i * width + j) % 2049;
                        if (channelTypes[k] == 2)
                            ((float*)data[k][i][j])[l] = (i * width + j) % 2049;
                    }
                }
            }
            file.writePixels(1);
        }
    }

    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
            for (int k = 0; k < channelCount; k++)
            {
                if (channelTypes[k] == 0)
                    delete[] (unsigned int*) data[k][i][j];
                if (channelTypes[k] == 1)
                    delete[] (half*) data[k][i][j];
                if (channelTypes[k] == 2)
                    delete[] (float*) data[k][i][j];
            }
}

void readFile(int channelCount, bool bulkRead,bool randomChannels)
{
    if(randomChannels)
    {
      cout << " reading random channels " << flush;
    }else{
      cout << " reading all channels " << flush;
    }
    
    DeepScanLineInputFile file(filename, 8);

    const Header& fileHeader = file.header();
    assert (fileHeader.displayWindow() == header.displayWindow());
    assert (fileHeader.dataWindow() == header.dataWindow());
    assert (fileHeader.pixelAspectRatio() == header.pixelAspectRatio());
    assert (fileHeader.screenWindowCenter() == header.screenWindowCenter());
    assert (fileHeader.screenWindowWidth() == header.screenWindowWidth());
    assert (fileHeader.lineOrder() == header.lineOrder());
    assert (fileHeader.compression() == header.compression());
    assert (fileHeader.channels() == header.channels());
    assert (fileHeader.type() == header.type());

    Array2D<unsigned int> localSampleCount;
    localSampleCount.resizeErase(height, width);
    Array<Array2D< void* > > data(channelCount);
    for (int i = 0; i < channelCount; i++)
        data[i].resizeErase(height, width);

    DeepFrameBuffer frameBuffer;

    frameBuffer.insertSampleCountSlice (Slice (UINT,                    // type // 7
                                        (char *) (&localSampleCount[0][0]
                                                  - dataWindow.min.x
                                                  - dataWindow.min.y * width),               // base // 8)
                                        sizeof (unsigned int) * 1,          // xStride// 9
                                        sizeof (unsigned int) * width));    // yStride// 10

    vector<int> read_channel(channelCount);
    
    
    int channels_added=0;
    
    for (int i = 0; i < channelCount; i++)
    {
        if(randomChannels)
        {
	     read_channel[i] = rand() % 2;
	     
        }
        if(!randomChannels || read_channel[i]==1)
	{
            PixelType type;
            if (channelTypes[i] == 0)
                type = UINT;
            if (channelTypes[i] == 1)
                type = HALF;
            if (channelTypes[i] == 2)
                type = FLOAT;

            stringstream ss;
            ss << i;
            string str = ss.str();

            int sampleSize;
            if (channelTypes[i] == 0) sampleSize = sizeof (unsigned int);
            if (channelTypes[i] == 1) sampleSize = sizeof (half);
            if (channelTypes[i] == 2) sampleSize = sizeof (float);

            int pointerSize = sizeof (char *);

            frameBuffer.insert (str,                            // name // 6
                                DeepSlice (type,                    // type // 7
                                (char *) (&data[i][0][0]
                                          - dataWindow.min.x
                                          - dataWindow.min.y * width),               // base // 8)
                                pointerSize * 1,          // xStride// 9
                                pointerSize * width,      // yStride// 10
                                sampleSize));             // sampleStride
	    channels_added++;
        }
    }
    
    
    if(channels_added==0)
    {
      cout << "skipping " <<flush;
      return;
    }
    
    file.setFrameBuffer(frameBuffer);

    if (bulkRead)
    {
        cout << "bulk " << flush;
        file.readPixelSampleCounts(dataWindow.min.y, dataWindow.max.y);
        for (int i = 0; i < dataWindow.max.y - dataWindow.min.y + 1; i++)
        {
            int y = i + dataWindow.min.y;

            for (int j = 0; j < width; j++)
                assert(localSampleCount[i][j] == sampleCount[i][j]);

            for (int j = 0; j < width; j++)
            {
                for (int k = 0; k < channelCount; k++)
                {
   		     if(!randomChannels || read_channel[k]==1)
		     {
                         if (channelTypes[k] == 0)
                              data[k][i][j] = new unsigned int[localSampleCount[i][j]];
                         if (channelTypes[k] == 1)
                              data[k][i][j] = new half[localSampleCount[i][j]];
                         if (channelTypes[k] == 2)
                             data[k][i][j] = new float[localSampleCount[i][j]];
		     }
                }
            }
        }
        
        file.readPixels(dataWindow.min.y, dataWindow.max.y);
    }
    
    else
    {
        cout << "per-line " << flush;
        for (int i = 0; i < dataWindow.max.y - dataWindow.min.y + 1; i++)
        {
            int y = i + dataWindow.min.y;
            file.readPixelSampleCounts(y);

            for (int j = 0; j < width; j++)
                assert(localSampleCount[i][j] == sampleCount[i][j]);

            for (int j = 0; j < width; j++)
            {
                for (int k = 0; k < channelCount; k++)
                {
		    if( !randomChannels || read_channel[k]==1)
		    {
                        if (channelTypes[k] == 0)
                            data[k][i][j] = new unsigned int[localSampleCount[i][j]];
                        if (channelTypes[k] == 1)
                            data[k][i][j] = new half[localSampleCount[i][j]];
                        if (channelTypes[k] == 2)
                            data[k][i][j] = new float[localSampleCount[i][j]];
		    }
                }
            }

            file.readPixels(y);
	    
        }
    }

    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
            for (int k = 0; k < channelCount; k++)
            {
	        if( !randomChannels || read_channel[k]==1 )
		{
                    for (int l = 0; l < sampleCount[i][j]; l++)
                    {
                        if (channelTypes[k] == 0)
                        {
                            unsigned int* value = (unsigned int*)(data[k][i][j]);
                            if (value[l] != (i * width + j) % 2049)
                                cout << j << ", " << i << " error, should be "
                                     << (i * width + j) % 2049 << ", is " << value[l]
                                     << endl << flush;
                            assert (value[l] == (i * width + j) % 2049);
                        }
                        if (channelTypes[k] == 1)
                        {
                            half* value = (half*)(data[k][i][j]);
                            if (value[l] != (i * width + j) % 2049)
                                cout << j << ", " << i << " error, should be "
                                     << (i * width + j) % 2049 << ", is " << value[l]
                                     << endl << flush;
                            assert (((half*)(data[k][i][j]))[l] == (i * width + j) % 2049);
                        }
                        if (channelTypes[k] == 2)
                        {
                            float* value = (float*)(data[k][i][j]);
                            if (value[l] != (i * width + j) % 2049)
                                cout << j << ", " << i << " error, should be "
                                     << (i * width + j) % 2049 << ", is " << value[l]
                                     << endl << flush;
                            assert (((float*)(data[k][i][j]))[l] == (i * width + j) % 2049);
                        }
                    }
		}
            }

    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
            for (int k = 0; k < channelCount; k++)
            {
      	        if( !randomChannels || read_channel[k]==1 )
		{
                    if (channelTypes[k] == 0)
                        delete[] (unsigned int*) data[k][i][j];
                    if (channelTypes[k] == 1)
                        delete[] (half*) data[k][i][j];
                    if (channelTypes[k] == 2)
                        delete[] (float*) data[k][i][j];
		}
            }
}

void readWriteTest(int channelCount, int testTimes)
{
    cout << "Testing files with " << channelCount << " channels " << testTimes << " times."
         << endl << flush;
    for (int i = 0; i < testTimes; i++)
    {
        int compressionIndex = i % 3;
        Compression compression;
        switch (compressionIndex)
        {
            case 0:
                compression = NO_COMPRESSION;
                break;
            case 1:
                compression = RLE_COMPRESSION;
                break;
            case 2:
                compression = ZIPS_COMPRESSION;
                break;
        }

        generateRandomFile(channelCount, compression, false);
        readFile(channelCount, false , false );
	if(channelCount>1) readFile(channelCount, false , true );
        remove (filename);
        cout << endl << flush;

        generateRandomFile(channelCount, compression, true);
        readFile(channelCount, true , false );
	if(channelCount>1) readFile(channelCount, true , true );
        remove (filename);
        cout << endl << flush;
    }
}

void testCompressionTypeChecks()
{
    Header h;
    h.setType(DEEPTILE);
    h.compression()=NO_COMPRESSION;
    h.sanityCheck();
    h.compression()=ZIPS_COMPRESSION;
    h.sanityCheck();
    h.compression()=RLE_COMPRESSION;
    h.sanityCheck();
    
    cout << "accepted valid compression types\n";
    //
    // these should fail
    //
    try{
        h.compression()=ZIP_COMPRESSION;
        h.sanityCheck();    
        assert(false);
    }catch(...){ 
        cout << "correctly identified bad compression setting (zip)\n";
    }
    try{
        h.compression()=B44_COMPRESSION;
        h.sanityCheck();
        assert(false);
    }catch(...){ 
        cout << "correctly identified bad compression setting (b44)\n";
    }
    try{
        h.compression()=B44A_COMPRESSION;
        h.sanityCheck();
        assert(false);
    }catch(...) { 
        cout << "correctly identified bad compression setting (b44a)\n";
    }
    try{
        h.compression()=PXR24_COMPRESSION;
        h.sanityCheck();
        assert(false);
    }catch(...) {
        cout << "correctly identified bad compression setting (pxr24)\n";
    }
    
    
    return;
}

}; // namespace

void testDeepScanLineBasic()
{
    try
    {
        cout << "\n\nTesting the DeepScanLineInput/OutputFile for basic use:\n" << endl;

        srand(1);

        int numThreads = ThreadPool::globalThreadPool().numThreads();
        ThreadPool::globalThreadPool().setNumThreads(4);

        
        testCompressionTypeChecks();
	
        readWriteTest(1, 100);
	readWriteTest(3, 50);        
        readWriteTest(10, 10);

        ThreadPool::globalThreadPool().setNumThreads(numThreads);

        cout << "ok\n" << endl;
    }
    catch (const std::exception &e)
    {
        cerr << "ERROR -- caught exception: " << e.what() << endl;
        assert (false);
    }
}
