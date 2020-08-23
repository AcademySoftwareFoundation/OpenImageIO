/* THIS CODE CARRIES NO GUARANTEE OF USABILITY OR FITNESS FOR ANY PURPOSE.
 * WHILE THE AUTHORS HAVE TRIED TO ENSURE THE PROGRAM WORKS CORRECTLY,
 * IT IS STRICTLY USE AT YOUR OWN RISK.  */

// clang-format off

#include "rgbe.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cctype>

/* This file contains code to read and write four byte rgbe file format
 developed by Greg Ward.  It handles the conversions between rgbe and
 pixels consisting of floats.  The data is assumed to be an array of floats.
 By default there are three floats per pixel in the order red, green, blue.
 (RGBE_DATA_??? values control this.)  Only the mimimal header reading and 
 writing is implemented.  Each routine does error checking and will return
 a status value as defined below.  This code is intended as a skeleton so
 feel free to modify it to suit your needs.

 (Place notice here if you modified the code.)
 posted to http://www.graphics.cornell.edu/~bjw/
 written by Bruce Walter  (bjw@graphics.cornell.edu)  5/26/95
 based on code written by Greg Ward

 Various modifications by Larry Gritz (lg@larrygritz.com), July 1998:
 1. Fix RGBE_ReadHeader to handle changes in the order of header fields
    that can be found in some .hdr files on the net.
 2. Correctly read and write images of all 8 orientations (not just -Y+X)
    and specify the orientation in the rgbe_header_info structure.
 3. Change the default programtype string from "RGBE" to "RADIANCE" since
    I noticed that some hdr/rgbe readers (including OS X's preview util)
    will only accept "RADIANCE" as the programtype.

Further changes by LG, 2018:
* Replace unsafe string ops and fixed size buffers for error messages with
  std::string and Strutil::sprintf.

*/

#if defined(_CPLUSPLUS) || defined(__cplusplus)
/* define if your compiler understands inline commands */
#define INLINE inline
#else
#define INLINE
#endif

/* offsets to red, green, and blue components in a data (float) pixel */
#define RGBE_DATA_RED    0
#define RGBE_DATA_GREEN  1
#define RGBE_DATA_BLUE   2
/* number of floats per pixel */
#define RGBE_DATA_SIZE   3

OIIO_PLUGIN_NAMESPACE_BEGIN

enum rgbe_error_codes {
  rgbe_read_error,
  rgbe_write_error,
  rgbe_format_error,
  rgbe_memory_error,
};

/* default error routine.  change this to change error handling */
static int rgbe_error(int rgbe_error_code, const char *msg, std::string &errbuf)
{
  switch (rgbe_error_code) {
  case rgbe_read_error:
    errbuf = "RGBE read error";
    break;
  case rgbe_write_error:
    errbuf = "RGBE write error";
    break;
  case rgbe_format_error:
    errbuf = Strutil::sprintf ("RGBE bad file format: %s\n", msg);
    break;
  default:
  case rgbe_memory_error:
    errbuf = Strutil::sprintf ("RGBE error: %s\n",msg);
    break;
  }
  return RGBE_RETURN_FAILURE;
}

/* standard conversion from float pixels to rgbe pixels */
/* note: you can remove the "inline"s if your compiler complains about it */
static INLINE void 
float2rgbe(unsigned char rgbe[4], float red, float green, float blue)
{
  float v;
  int e;

  v = red;
  if (green > v) v = green;
  if (blue > v) v = blue;
  if (v < 1e-32) {
    rgbe[0] = rgbe[1] = rgbe[2] = rgbe[3] = 0;
  }
  else {
    v = frexpf(v,&e) * 256.0f/v;
    rgbe[0] = (unsigned char) (red * v);
    rgbe[1] = (unsigned char) (green * v);
    rgbe[2] = (unsigned char) (blue * v);
    rgbe[3] = (unsigned char) (e + 128);
  }
}


inline void
float2rgbe(unsigned char* rgbe, const float* rgb)
{
    float red = rgb[0], green = rgb[1], blue = rgb[2];
    float v = red;
    if (green > v) v = green;
    if (blue > v) v = blue;
    if (v < 1e-32) {
        rgbe[0] = rgbe[1] = rgbe[2] = rgbe[3] = 0;
    } else {
        int e;
        v = frexpf(v,&e) * 256.0f/v;
        rgbe[0] = (unsigned char) (red * v);
        rgbe[1] = (unsigned char) (green * v);
        rgbe[2] = (unsigned char) (blue * v);
        rgbe[3] = (unsigned char) (e + 128);
    }
}



/* standard conversion from rgbe to float pixels */
/* note: Ward uses ldexp(col+0.5,exp-(128+8)).  However we wanted pixels */
/*       in the range [0,1] to map back into the range [0,1].            */
static INLINE void 
rgbe2float(float *red, float *green, float *blue, unsigned char rgbe[4])
{
  float f;

  if (rgbe[3]) {   /*nonzero pixel*/
    f = ldexpf(1.0f,rgbe[3]-(int)(128+8));
    *red = rgbe[0] * f;
    *green = rgbe[1] * f;
    *blue = rgbe[2] * f;
  }
  else
    *red = *green = *blue = 0.0;
}


inline void
rgbe2float(float* rgb, unsigned char* rgbe)
{
    if (rgbe[3]) {   /*nonzero pixel*/
        float f = ldexpf(1.0f,rgbe[3]-(int)(128+8));
        rgb[0] = rgbe[0] * f;
        rgb[1] = rgbe[1] * f;
        rgb[2] = rgbe[2] * f;
    } else {
      rgb[0] = rgb[1] = rgb[2] = 0.0;
    }
}



/* default minimal header. modify if you want more information in header */
int RGBE_WriteHeader(FILE *fp, int width, int height, rgbe_header_info *info,
                     std::string &errbuf)
{
  const char *programtype = "RADIANCE";
  /* N.B. from Larry Gritz:
   * Plenty of readers will refuse to read .rgbe/.hdr files if their
   * program type is not "RADIANCE".  So I changed the default
   * programtype from Bruce Walter's original "RGBE", which many readers
   * refuse to accept.  (Mac OS X's "preview" utility is one such reader!)
   */

  if (info && (info->valid & RGBE_VALID_PROGRAMTYPE))
    programtype = info->programtype;
  if (fprintf(fp,"#?%s\n",programtype) < 0)
      return rgbe_error(rgbe_write_error,NULL, errbuf);
  /* The #? is to identify file type, the programtype is optional. */
  if (info && (info->valid & RGBE_VALID_GAMMA)) {
    if (fprintf(fp,"GAMMA=%g\n",info->gamma) < 0)
      return rgbe_error(rgbe_write_error,NULL, errbuf);
  }
  if (info && (info->valid & RGBE_VALID_EXPOSURE)) {
    if (fprintf(fp,"EXPOSURE=%g\n",info->exposure) < 0)
      return rgbe_error(rgbe_write_error,NULL, errbuf);
  }
  if (fprintf(fp,"FORMAT=32-bit_rle_rgbe\n\n") < 0)
    return rgbe_error(rgbe_write_error,NULL, errbuf);
  if (fprintf(fp, "-Y %d +X %d\n", height, width) < 0)
    return rgbe_error(rgbe_write_error,NULL, errbuf);
  return RGBE_RETURN_SUCCESS;
}

/* minimal header reading.  modify if you want to parse more information */
int RGBE_ReadHeader(FILE *fp, int *width, int *height, rgbe_header_info *info,
                    std::string &errbuf)
{
  char buf[128];
  float tempf;
  size_t i;

  if (info) {
    info->valid = 0;
    info->programtype[0] = 0;
    info->gamma = info->exposure = 1.0;
  }
  if (fgets(buf,sizeof(buf)/sizeof(buf[0]),fp) == NULL)
    return rgbe_error(rgbe_read_error,NULL, errbuf);
  if ((buf[0] != '#')||(buf[1] != '?')) {
    /* if you want to require the magic token then uncomment the next line */
    /*return rgbe_error(rgbe_format_error,"bad initial token"); */
  }
  else if (info) {
    info->valid |= RGBE_VALID_PROGRAMTYPE;
    for(i=0;i<(int)sizeof(info->programtype)-1;i++) {
      if ((buf[i+2] == 0) || isspace(buf[i+2]))
	break;
      info->programtype[i] = buf[i+2];
    }
    info->programtype[i] = 0;
    if (fgets(buf,sizeof(buf)/sizeof(buf[0]),fp) == 0)
      return rgbe_error(rgbe_read_error,NULL, errbuf);
  }
  bool found_FORMAT_line = false;
  for(;;) {
    if ((buf[0] == 0)||(buf[0] == '\n')) {
        if (found_FORMAT_line)
            break;
        return rgbe_error(rgbe_format_error,"no FORMAT specifier found", errbuf);
    }
    else if (strcmp(buf,"FORMAT=32-bit_rle_rgbe\n") == 0) {
        found_FORMAT_line = true;
        /* LG says no:    break;       // format found so break out of loop */
    }
    else if (info && (sscanf(buf,"GAMMA=%g",&tempf) == 1)) {
      info->gamma = tempf;
      info->valid |= RGBE_VALID_GAMMA;
    }
    else if (info && (sscanf(buf,"EXPOSURE=%g",&tempf) == 1)) {
      info->exposure = tempf;
      info->valid |= RGBE_VALID_EXPOSURE;
    }
    if (fgets(buf,sizeof(buf)/sizeof(buf[0]),fp) == 0)
      return rgbe_error(rgbe_read_error,NULL, errbuf);
  }
  if (strcmp(buf,"\n") != 0) {
      printf ("Found '%s'\n", buf);
    return rgbe_error(rgbe_format_error,
		      "missing blank line after FORMAT specifier", errbuf);
  }
  if (fgets(buf,sizeof(buf)/sizeof(buf[0]),fp) == 0)
    return rgbe_error(rgbe_read_error,NULL, errbuf);

  if (sscanf(buf,"-Y %d +X %d",height,width) == 2) {
      if (info) {
          info->orientation = 1;
          info->valid |= RGBE_VALID_ORIENTATION;
      }
  } else if (sscanf(buf,"-Y %d -X %d",height,width) == 2) {
      if (info) {
          info->orientation = 2;
          info->valid |= RGBE_VALID_ORIENTATION;
      }
  } else if (sscanf(buf,"+Y %d -X %d",height,width) == 2) {
      if (info) {
          info->orientation = 3;
          info->valid |= RGBE_VALID_ORIENTATION;
      }
  } else if (sscanf(buf,"+Y %d +X %d",height,width) == 2) {
      if (info) {
          info->orientation = 4;
          info->valid |= RGBE_VALID_ORIENTATION;
      }
  } else if (sscanf(buf,"+X %d -Y %d",height,width) == 2) {
      if (info) {
          info->orientation = 5;
          info->valid |= RGBE_VALID_ORIENTATION;
      }
  } else if (sscanf(buf,"+X %d +Y %d",height,width) == 2) {
      if (info) {
          info->orientation = 6;
          info->valid |= RGBE_VALID_ORIENTATION;
      }
  } else if (sscanf(buf,"-X %d +Y %d",height,width) == 2) {
      if (info) {
          info->orientation = 7;
          info->valid |= RGBE_VALID_ORIENTATION;
      }
  } else if (sscanf(buf,"-X %d -Y %d",height,width) == 2) {
      if (info) {
          info->orientation = 8;
          info->valid |= RGBE_VALID_ORIENTATION;
      }
  } else {
    return rgbe_error(rgbe_format_error,"missing image size specifier", errbuf);
  }
  return RGBE_RETURN_SUCCESS;
}

/* simple write routine that does not use run length encoding */
/* These routines can be made faster by allocating a larger buffer and
   fread-ing and fwrite-ing the data in larger chunks */
int RGBE_WritePixels(FILE *fp, float *data, int64_t numpixels,
                     std::string &errbuf)
{
    std::unique_ptr<unsigned char[]> rgbe(new unsigned char [4*numpixels]);
    for (int64_t i = 0; i < numpixels; ++i)
        float2rgbe(&rgbe[4*i], data+3*i);
    if (fwrite(rgbe.get(), 4, numpixels, fp) != size_t(numpixels))
        return rgbe_error(rgbe_write_error, nullptr, errbuf);
    return RGBE_RETURN_SUCCESS;
}


/* simple read routine.  will not correctly handle run length encoding */
int RGBE_ReadPixels(FILE *fp, float *data, int numpixels,
                    std::string &errbuf)
{
    std::unique_ptr<unsigned char[]> rgbe(new unsigned char [4*numpixels]);
    if (fread(rgbe.get(), 4, numpixels, fp) != size_t(numpixels))
        return rgbe_error(rgbe_read_error,NULL, errbuf);
    for (int64_t i = 0; i < numpixels; ++i)
        rgbe2float(&data[3*i], &rgbe[4*i]);
    return RGBE_RETURN_SUCCESS;
}

/* The code below is only needed for the run-length encoded files. */
/* Run length encoding adds considerable complexity but does */
/* save some space.  For each scanline, each channel (r,g,b,e) is */
/* encoded separately for better compression. */

static int RGBE_WriteBytes_RLE(FILE *fp, unsigned char *data, int numbytes,
                               std::string &errbuf)
{
#define MINRUNLENGTH 4
  int cur, beg_run, run_count, old_run_count, nonrun_count;
  unsigned char buf[2];

  cur = 0;
  while(cur < numbytes) {
    beg_run = cur;
    /* find next run of length at least 4 if one exists */
    run_count = old_run_count = 0;
    while((run_count < MINRUNLENGTH) && (beg_run < numbytes)) {
      beg_run += run_count;
      old_run_count = run_count;
      run_count = 1;
      while((beg_run + run_count < numbytes) && (run_count < 127)
            && (data[beg_run] == data[beg_run + run_count]))
          run_count++;
      }
    /* if data before next big run is a short run then write it as such */
    if ((old_run_count > 1)&&(old_run_count == beg_run - cur)) {
      buf[0] = 128 + old_run_count;   /*write short run*/
      buf[1] = data[cur];
      if (fwrite(buf,sizeof(buf[0])*2,1,fp) < 1)
	return rgbe_error(rgbe_write_error,NULL, errbuf);
      cur = beg_run;
    }
    /* write out bytes until we reach the start of the next run */
    while(cur < beg_run) {
      nonrun_count = beg_run - cur;
      if (nonrun_count > 128) 
	nonrun_count = 128;
      buf[0] = nonrun_count;
      if (fwrite(buf,sizeof(buf[0]),1,fp) < 1)
	return rgbe_error(rgbe_write_error,NULL, errbuf);
      if (fwrite(&data[cur],sizeof(data[0])*nonrun_count,1,fp) < 1)
	return rgbe_error(rgbe_write_error,NULL, errbuf);
      cur += nonrun_count;
    }
    /* write out next run if one was found */
    if (run_count >= MINRUNLENGTH) {
      buf[0] = 128 + run_count;
      buf[1] = data[beg_run];
      if (fwrite(buf,sizeof(buf[0])*2,1,fp) < 1)
	return rgbe_error(rgbe_write_error,NULL, errbuf);
      cur += run_count;
    }
  }
  return RGBE_RETURN_SUCCESS;
#undef MINRUNLENGTH
}

int RGBE_WritePixels_RLE(FILE *fp, float *data, int scanline_width,
			 int num_scanlines, std::string &errbuf)
{
  unsigned char rgbe[4];
  unsigned char *buffer;
  int i, err;

  if ((scanline_width < 8)||(scanline_width > 0x7fff))
    /* run length encoding is not allowed so write flat*/
    return RGBE_WritePixels(fp, data, scanline_width*num_scanlines, errbuf);
  buffer = (unsigned char *)malloc(sizeof(unsigned char)*4*scanline_width);
  if (buffer == NULL) 
    /* no buffer space so write flat */
    return RGBE_WritePixels(fp, data, scanline_width*num_scanlines, errbuf);
  while(num_scanlines-- > 0) {
    rgbe[0] = 2;
    rgbe[1] = 2;
    rgbe[2] = scanline_width >> 8;
    rgbe[3] = scanline_width & 0xFF;
    if (fwrite(rgbe, sizeof(rgbe), 1, fp) < 1) {
      free(buffer);
      return rgbe_error(rgbe_write_error,NULL, errbuf);
    }
    for(i=0;i<scanline_width;i++) {
      float2rgbe(rgbe,data[RGBE_DATA_RED],
		 data[RGBE_DATA_GREEN],data[RGBE_DATA_BLUE]);
      buffer[i] = rgbe[0];
      buffer[i+scanline_width] = rgbe[1];
      buffer[i+2*scanline_width] = rgbe[2];
      buffer[i+3*scanline_width] = rgbe[3];
      data += RGBE_DATA_SIZE;
    }
    /* write out each of the four channels separately run length encoded */
    /* first red, then green, then blue, then exponent */
    for(i=0;i<4;i++) {
      if ((err = RGBE_WriteBytes_RLE(fp,&buffer[i*scanline_width],
				     scanline_width, errbuf)) != RGBE_RETURN_SUCCESS) {
	free(buffer);
	return err;
      }
    }
  }
  free(buffer);
  return RGBE_RETURN_SUCCESS;
}
      
int RGBE_ReadPixels_RLE(FILE *fp, float *data, int scanline_width,
			int num_scanlines, std::string &errbuf)
{
  unsigned char rgbe[4], *scanline_buffer, *ptr, *ptr_end;
  int i, count;
  unsigned char buf[2];

  if ((scanline_width < 8)||(scanline_width > 0x7fff))
    /* run length encoding is not allowed so read flat*/
    return RGBE_ReadPixels(fp, data, scanline_width*num_scanlines, errbuf);
  scanline_buffer = NULL;
  /* read in each successive scanline */
  while(num_scanlines > 0) {
    if (fread(rgbe,sizeof(rgbe),1,fp) < 1) {
      free(scanline_buffer);
      return rgbe_error(rgbe_read_error,NULL, errbuf);
    }
    if ((rgbe[0] != 2)||(rgbe[1] != 2)||(rgbe[2] & 0x80)) {
      /* this file is not run length encoded */
      rgbe2float(&data[0],&data[1],&data[2],rgbe);
      data += RGBE_DATA_SIZE;
      free(scanline_buffer);
      return RGBE_ReadPixels(fp, data, scanline_width*num_scanlines-1, errbuf);
    }
    if ((((int)rgbe[2])<<8 | rgbe[3]) != scanline_width) {
      free(scanline_buffer);
      return rgbe_error(rgbe_format_error,"wrong scanline width", errbuf);
    }
    if (scanline_buffer == NULL)
      scanline_buffer = (unsigned char *)
	malloc(sizeof(unsigned char)*4*scanline_width);
    if (scanline_buffer == NULL) 
      return rgbe_error(rgbe_memory_error,"unable to allocate buffer space", errbuf);
    
    ptr = &scanline_buffer[0];
    /* read each of the four channels for the scanline into the buffer */
    for(i=0;i<4;i++) {
      ptr_end = &scanline_buffer[(i+1)*scanline_width];
      while(ptr < ptr_end) {
	if (fread(buf,sizeof(buf[0])*2,1,fp) < 1) {
	  free(scanline_buffer);
	  return rgbe_error(rgbe_read_error,NULL, errbuf);
	}
	if (buf[0] > 128) {
	  /* a run of the same value */
	  count = buf[0]-128;
	  if ((count == 0)||(count > ptr_end - ptr)) {
	    free(scanline_buffer);
	    return rgbe_error(rgbe_format_error,"bad scanline data", errbuf);
	  }
	  while(count-- > 0)
	    *ptr++ = buf[1];
	}
	else {
	  /* a non-run */
	  count = buf[0];
	  if ((count == 0)||(count > ptr_end - ptr)) {
	    free(scanline_buffer);
	    return rgbe_error(rgbe_format_error,"bad scanline data", errbuf);
	  }
	  *ptr++ = buf[1];
	  if (--count > 0) {
	    if (fread(ptr,sizeof(*ptr)*count,1,fp) < 1) {
	      free(scanline_buffer);
	      return rgbe_error(rgbe_read_error,NULL, errbuf);
	    }
	    ptr += count;
	  }
	}
      }
    }
    /* now convert data from buffer into floats */
    for(i=0;i<scanline_width;i++) {
      rgbe[0] = scanline_buffer[i];
      rgbe[1] = scanline_buffer[i+scanline_width];
      rgbe[2] = scanline_buffer[i+2*scanline_width];
      rgbe[3] = scanline_buffer[i+3*scanline_width];
      rgbe2float(&data[RGBE_DATA_RED],&data[RGBE_DATA_GREEN],
		 &data[RGBE_DATA_BLUE],rgbe);
      data += RGBE_DATA_SIZE;
    }
    num_scanlines--;
  }
  free(scanline_buffer);
  return RGBE_RETURN_SUCCESS;
}

OIIO_PLUGIN_NAMESPACE_END

// clang-format on
