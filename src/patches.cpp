#include <OpenImageIO/imageio.h>
#include <stdio.h>
#include <stdlib.h>
#include <cv.h>
#include<cvaux.h>
#include <cxcore.h>
#include <highgui.h>

OIIO_NAMESPACE_USING

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*

This code performs the conversion between OIIO and OpenCV basic image structures
It's been tested on many image formats and depth of 8 Bits per channel
It would be great to have some suggestions and comments from your side 

Mail me at : bhavya.6187@gmail.com 


*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//This function takes an ImageSpec (containing the image specifications) and an unsigned char array as input and returns an IplImage
// Right now works for all the formats but only on 8 bit images with any number of channels

IplImage* OIIO_To_IplImage(ImageSpec spec, unsigned char* pixels) 
{

	if(pixels == NULL)
		return NULL;
		
	if(spec.width <= 0 || spec.height <= 0 || spec.nchannels <= 0)
		return NULL;

	int channels = spec.nchannels;
	IplImage* input = cvCreateImage(cvSize(spec.width,spec.height) , IPL_DEPTH_8U , spec.nchannels); // OpenCV function for allocating memory to image

	int baseIndex = 0;
	int baseIndex1 = 0;
	bool flag = false;
	
	for( int i= 0 ; i < input->height ; i++)
	{
		for(int j=0 ; j < (input->width) ; j++)
		{
			for(int k = 0 ; k < channels ; k++)
			{
				// The Main difference in storage of Data in OpenCV and OIIO with one storing in RGB and other in BGR
				input->imageData[baseIndex1 + (j*channels) + k] = (unsigned char)pixels[baseIndex + (j*channels) + channels - k - 1];
				
			}
			
		}
		
		
		baseIndex += spec.width*channels;;
		baseIndex1 += spec.width*channels + (channels - 1);
		
		// Separate BaseIndexes b'coz openCV allocates channel-1 bytes extra at the end of line
		
		for(int k = 1 ; k < channels ; k++)
			{
				input->imageData[baseIndex1 - k] = (unsigned char)0;
				
			}
		
	}
	
	cvNamedWindow("Transformed", CV_WINDOW_AUTOSIZE); 
  	cvMoveWindow("Transformed", 100, 100);
     cvShowImage("Transformed", input );
     cvWaitKey(0);
	return input;
	
}

// This function takes an IplImage as input and returns an unsigned char array which can be readily written in an image using OIIO functions.
// The use of this function is demonstrated in main.

unsigned char* IplImage_To_OIIO(IplImage* src)
{
	if(src == NULL)
		return NULL;
	unsigned char *pixels;
	int xres, yres, channels;
	xres = src->width;
	yres = src->height;
	channels = src->nChannels;

	pixels = new unsigned char [xres*yres*channels];
	int baseIndex=0, baseIndex1=0;
	
	for( int i= 0 ; i < yres ; i++)
	{
		for(int j=0 ; j < xres ; j++)
		{
			for(int k = 0 ; k < channels ; k++)
			{
				pixels[baseIndex + (j*channels) + channels - k - 1] = (unsigned char)src->imageData[baseIndex1 + (j*channels) + k] ;
			}
			
		}
		
		baseIndex += xres*channels;
		baseIndex1 += xres*channels + (channels - 1) ;
		
	}
	return pixels;
}

// This is a usage example for the function IplImage_To_OIIO
/*int main(int argc, char** argv) 
{
	const char *filename = "foo.bmp";
	const char *filename1 = "oiio.bmp";
	IplImage* src = cvLoadImage(filename,CV_LOAD_IMAGE_UNCHANGED); //OpenCV function to load image from file name
	if(src == NULL)
		return 0;
	unsigned char *pixels;
	pixels = IplImage_To_OIIO(src);;
	
	int xres , yres , channels;
	xres = src->width;
	yres = src->height;
	channels = src->nChannels;
	
	ImageSpec spec (xres, yres, channels, TypeDesc::UINT8);
	ImageOutput *out = ImageOutput::create (filename1);
	if (! out)
		return 0;
	out->open (filename1, spec);
	out->write_image (TypeDesc::UINT8, pixels);
	out->close ();
	
	return 1;
	
	
}*/
