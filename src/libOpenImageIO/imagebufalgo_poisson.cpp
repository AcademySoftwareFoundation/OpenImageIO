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

#include <iostream>
#include "imagebufalgo.h"

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
    bool success = sic.solve();
    return success;
}

template<typename T>
static inline bool
seamlessCloning_ (ImageBuf &dst, const ImageBuf &src, const ImageBuf &mask, const ImageBuf &src2, bool isMixed)
{
    ImageBufAlgo::SeamlessCloning<T> sc(dst, src, mask, src2, isMixed);
    bool success = sc.solve();
    return success;
}

}

bool
ImageBufAlgo::smoothImageCompletion(ImageBuf &dst, const ImageBuf &src, const ImageBuf &mask)
{
    switch (src.spec().format.basetype) {
    case TypeDesc::FLOAT : return smoothImageCompletion_<float> (dst, src, mask); break; 
    default:
        return false;
    }
};

bool
ImageBufAlgo::seamlessCloning(ImageBuf &dst, const ImageBuf &src, const ImageBuf &mask, const ImageBuf &src2, bool isMixed)
{
    switch (src.spec().format.basetype) {
        case TypeDesc::FLOAT : return seamlessCloning_<float> (dst, src, mask, src2, isMixed); break;
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
    bool success = computeOutputPixels();
    
    return success;
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
    
    ImageBuf::ConstIterator<T> p (maskImg, 1, w-1, 1, h-1);
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
    
    std::vector< Eigen::Triplet<double> > tripletList;
    tripletList.reserve(5*N);
    
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
                tripletList.push_back(Eigen::Triplet<double>(i,mapping[key-w],1));
            else 
                ImageBufAlgo::pixelSub<T>(&bVal[0], (T*)(uSPxl.rawptr()), inchannels);
            
            if ( ImageBufAlgo::pixelCmp<T>((T*)(lMPxl.rawptr()), &maskingColor[0], mnchannels) ) // x-1,y        
                tripletList.push_back(Eigen::Triplet<double>(i,mapping[key-1],1));
            else 
                ImageBufAlgo::pixelSub<T>(&bVal[0], (T*)(lSPxl.rawptr()), inchannels);

            tripletList.push_back(Eigen::Triplet<double>(i,i,-4));

            if ( ImageBufAlgo::pixelCmp<T>((T*)(rMPxl.rawptr()), &maskingColor[0], mnchannels) ) // x+1,y
                tripletList.push_back(Eigen::Triplet<double>(i,mapping[key+1],1));
            else 
                ImageBufAlgo::pixelSub<T>(&bVal[0], (T*)(rSPxl.rawptr()), inchannels);

            if ( ImageBufAlgo::pixelCmp<T>((T*)(dMPxl.rawptr()), &maskingColor[0], mnchannels) ) // x,y+1
                tripletList.push_back(Eigen::Triplet<double>(i,mapping[key+w],1));
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
    
    A.setFromTriplets(tripletList.begin(), tripletList.end());
}

template <class T>
bool ImageBufAlgo::PoissonImageEditing<T>::computeOutputPixels()
{
    int w = maskImg.spec().full_width;
    int h = maskImg.spec().full_height;
    int posInSeq = 0; //position of masked pixel
    
    int mnchannels = maskImg.nchannels();
    int inchannels = img.nchannels();
    std::vector<T> maskingColor(mnchannels, 0.0f);
    
    //We can use one of these
    //TO DO: find out which is the fastest solution
    Eigen::SimplicialLDLT< Eigen::SparseMatrix<double> > solver;
    //Eigen::ConjugateGradient < Eigen::SparseMatrix<double> > solver;
    //Eigen::BiCGSTAB < Eigen::SparseMatrix<double> > solver;
   
    solver.compute(A);
    if(solver.info()!=Eigen::Success)
    {
    //    std::cout << "factorization error - quit\n";
        return false;
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
    
    return true;
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



//------------ Seamless cloning ------------------------//
template <class T>
ImageBufAlgo::SeamlessCloning<T>::SeamlessCloning(ImageBuf &output, const ImageBuf& src, const ImageBuf& mask, const ImageBuf& _src2, bool _isMixed) :
        PoissonImageEditing<T>(output, src, mask),
        src2 (_src2),
        isMixed (_isMixed)
{

}

template <class T>
void ImageBufAlgo::SeamlessCloning<T>::getGuidanceVector(std::vector<T> &pel, int x, int y, int nchannels)
{   
    ImageBuf::ConstIterator<T> p (src2, x, x+1, y, y+1);
    ImageBuf::ConstIterator<T> lp (src2, x-1, x, y, y+1);
    ImageBuf::ConstIterator<T> rp (src2, x+1, x+2, y, y+1);
    ImageBuf::ConstIterator<T> dp (src2, x, x+1, y+1, y+2);
    ImageBuf::ConstIterator<T> up (src2, x, x+1, y-1, y);
    
    for(int i = 0; i < nchannels; i++)
        pel[i] = lp[i] + rp[i] + dp[i] + up[i] - 4*p[i];
    
    if(isMixed) 
    {    
        ImageBuf::ConstIterator<T> p1 (this->img, x, x+1, y, y+1);
        ImageBuf::ConstIterator<T> lp1 (this->img, x-1, x, y, y+1);
        ImageBuf::ConstIterator<T> rp1 (this->img, x+1, x+2, y, y+1);
        ImageBuf::ConstIterator<T> dp1 (this->img, x, x+1, y+1, y+2);
        ImageBuf::ConstIterator<T> up1 (this->img, x, x+1, y-1, y);
        
        for(int i = 0; i < nchannels; i++)
        {
            T tmp = lp1[i] + rp1[i] + dp1[i] + up1[i] - 4*p1[i];
            if(fabs(tmp) > fabs(pel[i]))
                pel[i] = tmp;
        }
    }
 
    
}




}
OIIO_NAMESPACE_EXIT
