#include "crop.h"
 
namespace ImageBufAlgo 
{
	
	bool crop(ImageBuf &out_image, const ImageBuf &in_image,
	          int xmin, int ymin, int xmax, int ymax,
	          int options) 
	{
		const ImageSpec &in_spec (in_image.spec());
		
		//check input
		if (xmin >= xmin)
		{
		    std::cerr << "crop ERROR: xmin should be smaller than xmax \n" ;
		    return false;
		}
		if (ymin >= ymin)
		{
		    std::cerr << "crop ERROR: ymin should be smaller than ymax \n" ;
		    return false;
		}
		if (xmin < in_spec.x || xmax > in_spec.x + in_spec.width)
		{
			std::cerr << "crop ERROR: x values are out of image bounds \n" ;
			return false;
		}
		if( options == CROP_TRANS && in_spec.alpha_channel == -1 )
		{
			std::cerr << "crop ERROR: no alpha channel present \n";
			return false;
		}		
		//manipulate the images

		ImageSpec out_spec = in_spec;		
		switch(options)
		{
			case CROP_WINDOW:
				out_spec.x = xmin;
				out_spec.y = ymin;
			break;	
			case CROP_BLACK:
			case CROP_WHITE:
			case CROP_TRANS:
				
			break;
			
			case CROP_CUT:
				out_spec.x = 0;
				out_spec.y = 0;
				out_spec.width = xmax-xmin+1;
				out_spec.height = ymax-ymin+1;
				
			break;
		}

		//create new ImageBuffer
		ImageBuf out_buf ("pic", out_spec);
		
		//copy pixels
                switch(options)
                {
                        case CROP_WINDOW:
                        break;
                        case CROP_BLACK:
                        case CROP_WHITE:
                        case CROP_TRANS:
                        case CROP_CUT:
			for (int i=xmin; i<xmax+1; i++)
				for (int j=ymin; j<ymin+1; j++)
				{
					color3f RGB;
					im_buf.getpixel (i*in_spec.width + j, RGB.getValue(), 3);
					im_buf.putpixel (i*in_spec.width + j, RGB.value, 3);
				}//for j
                        break;
                }

	
		//close files
	
		
	}//bool crop()
	
};//namespace ImageBufAlgo
