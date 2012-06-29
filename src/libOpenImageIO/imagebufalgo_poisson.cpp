/*
  Copyright 2012 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/

/* This header has to be included before boost/regex.hpp header
   If it is included after, there is an error
   "undefined reference to CSHA1::Update (unsigned char const*, unsigned long)"
*/
#include "SHA1.h"

/// \file
/// Implementation of ImageBufAlgo algorithms.

#include <OpenEXR/ImathFun.h>
#include <OpenEXR/half.h>

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "imagebuf.h"
#include "imagebufalgo.h"
#include "dassert.h"
#include "sysutil.h"
#include "filter.h"


OIIO_NAMESPACE_ENTER
{
  
template<typename PXLTYPE>
bool ImageBufAlgo::pixelCmp(PXLTYPE *a, PXLTYPE *b, int channels)
{
    if(memcmp(a, b, channels*sizeof(PXLTYPE)) == 0)
        return true;
    else
        return false;
    
}

template<typename PXLTYPE>
void ImageBufAlgo::pixelSub(PXLTYPE *a, PXLTYPE *b, int channels)
{
    for(int i = 0; i < channels; i++)
        a[i] -= b[i];
}
    
namespace
{

template<typename T>
static inline bool
smoothImageCompletion_ (ImageBuf &dst, const ImageBuf &src, const ImageBuf &mask)
{
    ImageBufAlgo::SmoothImageCompletion<T> sic(dst, src, mask);
    sic.solve();
    return true;
}

}


bool
ImageBufAlgo::smoothImageCompletion(ImageBuf &dst, const ImageBuf &src, const ImageBuf &mask)
{
    switch (src.spec().format.basetype) {
    case TypeDesc::FLOAT : return smoothImageCompletion_<float> (dst, src, mask); break;
    case TypeDesc::UINT8 : return smoothImageCompletion_<unsigned char> (dst, src, mask); break;
    case TypeDesc::INT8  : return smoothImageCompletion_<char> (dst, src, mask); break;
    case TypeDesc::UINT16: return smoothImageCompletion_<unsigned short> (dst, src, mask); break;
    case TypeDesc::INT16 : return smoothImageCompletion_<short> (dst, src, mask); break;
    case TypeDesc::UINT  : return smoothImageCompletion_<unsigned int> (dst, src, mask); break;
    case TypeDesc::INT   : return smoothImageCompletion_<int> (dst, src, mask); break;
    case TypeDesc::UINT64: return smoothImageCompletion_<unsigned long long> (dst, src, mask); break;
    case TypeDesc::INT64 : return smoothImageCompletion_<long long> (dst, src, mask); break;
    case TypeDesc::HALF  : return smoothImageCompletion_<half> (dst, src, mask); break;
    case TypeDesc::DOUBLE: return smoothImageCompletion_<double> (dst, src, mask); break; 
    default:
        return false;
    }
};

template <class T>
ImageBufAlgo::PoissonImageEditing<T>::PoissonImageEditing(ImageBuf &output, const ImageBuf &src, const ImageBuf &mask) :
      img (src),
      maskImg (mask),  
      out (output)
{
    
}

template <class T>
bool ImageBufAlgo::PoissonImageEditing<T>::solve()
{
    if(!verifyMask())
        return false;
    
    buildMapping();
    buildSparseLinearSystem();
    computeOutputPixels();
    
    return true;
}

template <class T>
bool ImageBufAlgo::PoissonImageEditing<T>::verifyMask()
{
    //TO DO: add image validation
    return true;
}

template <class T>
void ImageBufAlgo::PoissonImageEditing<T>::buildMapping()
{
    int w = maskImg.spec().full_width;
    int h = maskImg.spec().full_height;
    int nchannels = maskImg.spec().nchannels;
    int posInSeq = 0; //position of masked pixel in a sequence
    
    ImageBuf::ConstIterator<T> p (maskImg, 1, h-1, 1, w-1);
    std::vector<T> maskingColor(maskImg.spec().nchannels, 0.0f);
  
    // Loop over all pixels from mask image ...
    for ( ; p.valid();  ++p) 
    {
        if (ImageBufAlgo::pixelCmp<T>((T*)(p.rawptr()), &maskingColor[0], nchannels))
        {
            mapping[p.x() + p.y()*w ] = posInSeq;
            posInSeq++;
        }
    }
}

template <class T>
void ImageBufAlgo::PoissonImageEditing<T>::buildSparseLinearSystem()
{   
    int N = (int)mapping.size();
    
    A = Eigen::SparseMatrix<double>(N,N);
    
    int w = maskImg.spec().full_width;
    int h = maskImg.spec().full_height;
    
    int mnchannels = maskImg.nchannels();
    int inchannels = img.nchannels();
    
    b.resize(inchannels);
    
    for(int k = 0; k < b.size(); k++)
        b[k] = Eigen::VectorXd(N);
    
    x.resize(inchannels);
    
    for(int k = 0; k < x.size(); k++)
        x[k] = Eigen::VectorXd(N);
    
    
    // TO DO: check if this is the best way to iterate over pixels
    ImageBuf::ConstIterator<T> cMPxl (maskImg, 1, w-1, 1, h-1); // center mask pixel
    ImageBuf::ConstIterator<T> lMPxl (maskImg, 0, w-2, 1, h-1); // left
    ImageBuf::ConstIterator<T> rMPxl (maskImg, 2, w,   1, h-1);   // right
    ImageBuf::ConstIterator<T> dMPxl (maskImg, 1, w-1, 2, h);   // down
    ImageBuf::ConstIterator<T> uMPxl (maskImg, 1, w-1, 0, h-2); // up
    
    ImageBuf::ConstIterator<T> cSPxl (img, 1, w-1, 1, h-1); // center mask pixel
    ImageBuf::ConstIterator<T> lSPxl (img, 0, w-2, 1, h-1); // left
    ImageBuf::ConstIterator<T> rSPxl (img, 2, w,   1, h-1);   // right
    ImageBuf::ConstIterator<T> dSPxl (img, 1, w-1, 2, h);   // down
    ImageBuf::ConstIterator<T> uSPxl (img, 1, w-1, 0, h-2); // up
    
    std::vector<T> maskingColor(mnchannels, 0.0f);
    std::vector<T> bVal (inchannels, 0.0f); //right side value of the equation
    
    int i = 0;
    
    // Loop over all pixels
    while (cMPxl.valid()) 
    {   
        if (ImageBufAlgo::pixelCmp<T>((T*)(cMPxl.rawptr()), &maskingColor[0], mnchannels))
        {
            int x = cMPxl.x();
            int y = cMPxl.y();
            int key = y*w+x;
            
            getGuidanceVector(bVal, x, y, inchannels);

            if ( ImageBufAlgo::pixelCmp<T>((T*)(uMPxl.rawptr()), &maskingColor[0], mnchannels) ) // x, y-1
                A.insert(i,mapping[key-w]) = 1;
            else 
                ImageBufAlgo::pixelSub<T>(&bVal[0], (T*)(uSPxl.rawptr()), inchannels);
            
            if ( ImageBufAlgo::pixelCmp<T>((T*)(lMPxl.rawptr()), &maskingColor[0], mnchannels) ) // x-1,y
                A.insert(i,mapping[key-1]) = 1;             
            else 
                ImageBufAlgo::pixelSub<T>(&bVal[0], (T*)(lSPxl.rawptr()), inchannels);

            A.insert(i,i) = -4;

            if ( ImageBufAlgo::pixelCmp<T>((T*)(rMPxl.rawptr()), &maskingColor[0], mnchannels) ) // x+1,y
                A.insert(i,mapping[key+1]) = 1;
            else 
                ImageBufAlgo::pixelSub<T>(&bVal[0], (T*)(rSPxl.rawptr()), inchannels);

            if ( ImageBufAlgo::pixelCmp<T>((T*)(dMPxl.rawptr()), &maskingColor[0], mnchannels) ) // x,y+1
                A.insert(i,mapping[key+w]) = 1;
            else
                ImageBufAlgo::pixelSub<T>(&bVal[0], (T*)(dSPxl.rawptr()), inchannels);
            
            for(int k = 0; k < inchannels; k++)
                b[k](i) = bVal[k];

            i++;
        }
        
        cMPxl++;
        lMPxl++;
        rMPxl++;
        dMPxl++;
        uMPxl++;
        
        cSPxl++;
        lSPxl++;
        rSPxl++;
        dSPxl++;
        uSPxl++;     
    }
}

template <class T>
void ImageBufAlgo::PoissonImageEditing<T>::computeOutputPixels()
{
    int w = maskImg.spec().full_width;
    int h = maskImg.spec().full_height;
    int posInSeq = 0; //position of masked pixel
    
    int mnchannels = maskImg.nchannels();
    int inchannels = img.nchannels();
    std::vector<T> maskingColor(mnchannels, 0.0f);
    
    
    Eigen::SparseLDLT< Eigen::SparseMatrix<double> > solver;
   
    solver.compute(A);
    if(!solver.succeeded())
    {
        std::cerr << "factorization error - quit\n";
        return;
    }
    
    for(int k = 0; k < inchannels; k++)
        x[k] = solver.solve(b[k]);
    
    posInSeq = 0;
    
    ImageBuf::ConstIterator<T> cMPxl2 (maskImg, 1, w-1, 1, h-1);
    ImageBuf::ConstIterator<T> sPxl (img, 1, w-1, 1, h-1);
    ImageBuf::Iterator<T> oPxl (this->out, 1, w-1, 1, h-1);
    
    
    while (cMPxl2.valid()) 
    {
        if (ImageBufAlgo::pixelCmp<T>((T*)(cMPxl2.rawptr()), &maskingColor[0], mnchannels))
        {   
            //FIXME - clamping range should depend on pixel type
            for(int k = 0; k < inchannels; k++)
                oPxl[k] = clamp<T>(x[k](posInSeq), 0, 1);
               
            posInSeq++;
        }
        else {
            for(int i = 0; i < inchannels; i++)
                oPxl[i] = sPxl[i];
        }
        
        cMPxl2++;
        sPxl++;
        oPxl++;
    }
}
    



//------------ Smooth image completion ----------------//
template <class T>
ImageBufAlgo::SmoothImageCompletion<T>::SmoothImageCompletion(ImageBuf &output, const ImageBuf& src, const ImageBuf& mask) :
        PoissonImageEditing<T>(output, src, mask)
{

}

template <class T>
void ImageBufAlgo::SmoothImageCompletion<T>::getGuidanceVector(std::vector<T> &pel, int x, int y, int nchannels)
{
    for(int i = 0; i < nchannels; i++)
        pel[i] = 0;
}



}
OIIO_NAMESPACE_EXIT
