#ifndef _H_RGBE
#define _H_RGBE
/* THIS CODE CARRIES NO GUARANTEE OF USABILITY OR FITNESS FOR ANY PURPOSE.
 * WHILE THE AUTHORS HAVE TRIED TO ENSURE THE PROGRAM WORKS CORRECTLY,
 * IT IS STRICTLY USE AT YOUR OWN RISK.  */

/* utility for reading and writing Ward's rgbe image format.
   See rgbe.txt file for more details.
*/

#include <cstdio>

#include <OpenImageIO/imageio.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

typedef struct {
  int valid;            /* indicate which fields are valid */
  char programtype[16]; /* listed at beginning of file to identify it 
                         * after "#?".  defaults to "RGBE" */ 
  float gamma;          /* image has already been gamma corrected with 
                         * given gamma.  defaults to 1.0 (no correction) */
  float exposure;       /* a value of 1.0 in an image corresponds to
			 * <exposure> watts/steradian/m^2. 
			 * defaults to 1.0 */
  int orientation;      /* Orientation of the image.  Use the same coded
                         * values as the TIFF and JPEG/JFIF/EXIF specs.
                         * defaults to 1 (-Y +X)
                         * (added by Larry Gritz, 7/2008)
                         */
} rgbe_header_info;

/* flags indicating which fields in an rgbe_header_info are valid */
#define RGBE_VALID_PROGRAMTYPE 0x01
#define RGBE_VALID_GAMMA       0x02
#define RGBE_VALID_EXPOSURE    0x04
#define RGBE_VALID_ORIENTATION 0x08

/* return codes for rgbe routines */
#define RGBE_RETURN_SUCCESS 0
#define RGBE_RETURN_FAILURE -1

/* read or write headers */
/* you may set rgbe_header_info to null if you want to */
int RGBE_WriteHeader(FILE *fp, int width, int height, rgbe_header_info *info,
                     char *errbuf=NULL);
int RGBE_ReadHeader(FILE *fp, int *width, int *height, rgbe_header_info *info,
                    char *errbuf=NULL);

/* read or write pixels */
/* can read or write pixels in chunks of any size including single pixels*/
int RGBE_WritePixels(FILE *fp, float *data, int numpixels,
                     char *errbuf=NULL);
int RGBE_ReadPixels(FILE *fp, float *data, int numpixels,
                    char *errbuf=NULL);

/* read or write run length encoded files */
/* must be called to read or write whole scanlines */
int RGBE_WritePixels_RLE(FILE *fp, float *data, int scanline_width,
			 int num_scanlines, char *errbuf=NULL);
int RGBE_ReadPixels_RLE(FILE *fp, float *data, int scanline_width,
			int num_scanlines, char *errbuf=NULL);

OIIO_PLUGIN_NAMESPACE_END

#endif /* _H_RGBE */



