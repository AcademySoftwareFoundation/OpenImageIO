#ifndef _CROP_H
#define _CROP_H

#include "imageio.h"
#include "imagebuf.h"
#include "imageio.h"

using namespace OpenImageIO;

namespace ImageBufAlgo 
{

	enum CropOptions 
	{
		CROP_BLACK,	//color to black all the pixels outside of the bounds
		CROP_WHITE,	//color to white all the pixels outside of the bounds
		CROP_TRANS,	//make all pixels out of boundes transperant
					//change all color channels to 0
		CROP_WINDOW,//reduce the window of pixel data, keep it in the 
					//same position
		CROP_CUT 	//cuts out a pixel region to make a new image at the 
					//origin
	};//enum CropOptions
	
	bool crop (ImageBuf &out_image, const ImageBuf &in_image,
	          int xmin, int ymin, int xmax, int ymax,
	          int options);

};//namespace ImageBufAlgo

#endif /*_CROP_H */
