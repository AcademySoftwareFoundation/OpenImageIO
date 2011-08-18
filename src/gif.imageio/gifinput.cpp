#include <cstdio>
#include <cstdlib>
#include <cassert>

#include "gif_lib.h"
#include "imageio.h"


OIIO_PLUGIN_NAMESPACE_BEGIN


class GIFInput : public ImageInput {
public:
    GIFInput () { init(); }
    virtual ~GIFInput () { close(); }
    virtual const char * format_name (void) const { return "gif"; }
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool open (const std::string &name, ImageSpec &newspec,
                      const ImageSpec &config);
    virtual bool close ();
    virtual int current_subimage (void) const { return m_subimage; }
    virtual bool read_native_scanline (int y, int z, void *data);
    

private:
    GifFileType *GifFile;
    GifRecordType Type;
    GifByteType  *Extension;
    int Filehandle;
    void *Private ;
    std::string m_filename; 
    ///< Stash the filename
    FILE *m_file; ///< Open image handle
    int m_subimage; ///< What subimage are we looking at?
    int m_next_scanline;
    int m_bit_depth;
    //GifPixelType *Line; ///< PNG bit depth
    int m_interlace_type;
    bool m_raw;
    GifRowType *ScreenBuffer;
    
    
    
    /// Reset everything to initial state
    ///
    void init () {
        GifFile =NULL;
        m_subimage = -1;
        m_file = NULL;
        m_next_scanline = 0;
        m_raw=false;
        ScreenBuffer =NULL;
    
    }
    void DumpRow2RGB(GifPixelType *RowBuffer,
               int ScreenWidth);
     
};



// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

DLLEXPORT ImageInput *gif_input_imageio_create () { return new GIFInput; }

DLLEXPORT int gif_imageio_version = OIIO_PLUGIN_VERSION;

DLLEXPORT const char * gif_input_extensions[] = {
    "gif", NULL
};

OIIO_PLUGIN_EXPORTS_END

bool
GIFInput::open (const std::string &name, ImageSpec &newspec)
{
    int ExtCode;
    
    
    std::cout << "Open file\n";
    m_filename = name;
    m_subimage = 0;
          
    std::cout << m_filename;
    
    GifFile = DGifOpenFileName(m_filename.c_str());
   if(GifFile == NULL)
   {
     std::cerr << "Error in opening file";
     return false;
   } 
   std::cout << GifFile << "\n";
   std::cout << sizeof(GifFileType);
   std::cout << "\n File opened\n";
   
   
     std:: cout << "Initial image count" << GifFile->ImageCount << "\n";
    
   for(; ;)
    {
         if(DGifGetRecordType(GifFile, &Type)== GIF_ERROR)
         {
          std::cerr << "Record type err" << GIF_ERROR << Type <<"\n";
          PrintGifError();
         //DGifCloseFile(GifFile);
         //return false;
         }
        std::cout << "/n record type complete" << Type << "\n";
         
    
	   if(Type == EXTENSION_RECORD_TYPE)
	   {
		/* Skip any extension blocks in file except comments: */
		if (DGifGetExtension(GifFile, &ExtCode, &Extension) == GIF_ERROR) {
		    PrintGifError();
		    exit(EXIT_FAILURE);
		}
		std::cout << "checking extension \n"; 
		while (Extension != NULL) {
		    std::cout << "extension code" << ExtCode <<"\n";
		    if (DGifGetExtensionNext(GifFile, &Extension) == GIF_ERROR) {
			PrintGifError();
			std:: cerr << "Extension block failure";
		       }
		      
		}
		
	   }	
	   else if(Type == TERMINATE_RECORD_TYPE)
		break;
           else if(Type == IMAGE_DESC_RECORD_TYPE)
                break;		
	   else		    /* Should be traps by DGifGetRecordType. */
		continue;
	}
	
   
   
   if(Type== IMAGE_DESC_RECORD_TYPE)
   {
              if(DGifGetImageDesc(GifFile)!=1)
              {
                PrintGifError();
                // DGifCloseFile(GifFile);
                std::cerr << "Image desc err";
                return false;
              }
              //m_bit_depth = GifFile->Image.ColorMap->BitsPerPixel;
              m_spec = ImageSpec (GifFile->Image.Width, GifFile->Image.Height,
                       3,TypeDesc::UINT8);
                      
              std::cout << "image desc complete"<< GifFile->ImageCount;
              
    }
    
    
   //m_interlace_type = GifFile->Image.Interlace;
   //m_bit_depth = GifFile->Image.ColorMap->BitsPerPixel;
   std::cout <<"\n image count" <<GifFile->ImageCount;
   std::cout << "\nInterlace value" << GifFile->Image.Interlace;
   std::cout << "\nResolution" << GifFile->Image.Width << GifFile->Image.Height;
   //std::cout << "\n bit depth"<<m_bit_depth;
   m_spec.default_channel_names ();
   m_spec.attribute ("oiio:ColorSpace", "sRGB");
   m_next_scanline = 0;
   newspec= spec();
   std::cout << "open function complete";
    return true;
}
bool
GIFInput::close ()
{
  if(DGifCloseFile(GifFile)==1){
  std::cout << "Closing\n";
   return true;
  }
  std::cerr <<"Closing error";
   PrintGifError();
  return false;
}

bool
GIFInput::open (const std::string &name, ImageSpec &newspec,
                const ImageSpec &config)
{
    std::cout << "open 2\n";
    const ImageIOParameter *p = config.find_attribute ("_gif:raw",
                                                       TypeDesc::TypeInt);
    m_raw = p && *(int *)p->data();
    std::cout << "open2 exit\n";
    return open (name, newspec);
}


bool
GIFInput::read_native_scanline (int y, int z, void *data)
{
    int InterlacedOffset[] = { 0, 4, 2, 1 };/* The way Interlaced image should. */
    int InterlacedJumps[] = { 8, 8, 4, 2 };    /* be read - offsets and jumps... */
      
    int	i,j, Row, Col, Width, Height, Count;
    std::cout << "read\n";
    if (y < 0 || y >= m_spec.height){
    std::cerr << "out of range\n"; // out of range scanline
        return false;
    }
    std::cout << "value of y"<<y;
    std::cout << "\nInterlace value" << GifFile->Image.Interlace;
    if (GifFile->Image.Interlace) {
    if(ScreenBuffer == NULL){
                    if ((ScreenBuffer = (GifRowType *)
	malloc(GifFile->SHeight * sizeof(GifRowType *))) == NULL)
	    std::cerr << "Failed to allocate memory required, aborted.";  
                     Row = GifFile->Image.Top; /* Image Position relative to Screen. */
		    Col = GifFile->Image.Left;
		    Width = GifFile->Image.Width;
		    Height = GifFile->Image.Height;
		    /* Need to perform 4 passes on the images: */
		    for (Count = i = 0; i < 4; i++)
			for (j = Row + InterlacedOffset[i]; j < Row + Height;
						 j += InterlacedJumps[i]) {
			    //GifQprintf("std::cout << "\nInterlace value" << GifFile->Image.Interlace;\b\b\b\b%-4d", Count++);
			    if (DGifGetLine(GifFile, &ScreenBuffer[j][Col],
				Width) == GIF_ERROR) {
				PrintGifError();
				//exit(EXIT_FAILURE);
				return false;
			    }
			}}
			size_t size = spec().scanline_bytes();
                        memcpy (data, &ScreenBuffer[0] + y * size, size);
		}
		else {
		if (m_next_scanline > y) 
		{
                      // User is trying to read an earlier scanline than the one we're
                     // up to. Easy fix: close the file and re-open.
                    std::cout << "dummy spec\n";
                   ImageSpec dummyspec;
                   int subimage = current_subimage();
                   if (! close () ||
                   ! open (m_filename, dummyspec) ||
                   ! seek_subimage (subimage, 0, dummyspec))
                   return false; // Somehow, the re-open failed
                   assert (m_next_scanline == 0 && current_subimage() == subimage);
                }
		   for (; m_next_scanline <= y; ++m_next_scanline) {
		       
			//GifQprintf("\b\b\b\b%-4d", i);
			if (DGifGetLine(GifFile, (GifPixelType *)data,z) == GIF_ERROR) {
			    PrintGifError();
			    //exit(EXIT_FAILURE);
			    return false;
			}
			std::cout<< "Pixel read";					
		    }
		}
    
       //DumpRow2RGB((GifPixelType *)data,GifFile->SWidth);
       
       return true;
  }
/* void GIFInput::DumpRow2RGB(GifPixelType *RowBuffer,
               int ScreenWidth)
{
    int i, j;
    GifPixelType GifPixel;
    static GifColorType *ColorMapEntry;
    unsigned char* BufferP;
    ColorMapObject *ColorMap;
    
    ColorMap = (GifFile->Image.ColorMap
		? GifFile->Image.ColorMap
		: GifFile->SColorMap);
    if (ColorMap == NULL) {
        fprintf(stderr, "Gif Image does not have a colormap\n");
        exit(EXIT_FAILURE);
    }

    std::cout << "Color map present";

        if ((BufferP = (unsigned char *) malloc(ScreenWidth * 3)) == NULL)
            std::cerr << "Failed to allocate memory";
            //GIF_EXIT("Failed to allocate memory required, aborted.");
         
            GifPixel = *RowBuffer;
            RowBuffer = BufferP;
            //GifQprintf("\b\b\b\b%-4d", ScreenHeight - i);
            for (j = 0 ; j < ScreenWidth; j++) {
                ColorMapEntry = &ColorMap->Colors[GifPixel+j];
                *BufferP++ = ColorMapEntry->Red;
                *BufferP++ = ColorMapEntry->Green;
                *BufferP++ = ColorMapEntry->Blue;
            }
            /*if (fwrite(Buffer, ScreenWidth * 3, 1, f[0]) != 1)
                std::cerr << "Write failed";
              */  //GIF_EXIT("Write to file(s) failed.");
        std::cout<< "Success";

        //free((char *) BufferP);
        
    
 }
 */
}
}


