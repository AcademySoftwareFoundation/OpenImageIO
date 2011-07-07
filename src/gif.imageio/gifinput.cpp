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
        
    
    }
     
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
    int ExtCode , DelayTime, Terminator;
    char* Comment;
    char *BlockSize, *PackedField, *ColorIndex, *AuthentCode , *Identifier;
    int c ,f=0;
    
    
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
   
   Comment = (char*) malloc(1);
    strcpy(Comment, "\0");
     std:: cout << "Initial image count" << GifFile->ImageCount << "\n";
   do {
         if(DGifGetRecordType(GifFile, &Type)== GIF_ERROR)
         {
          std::cerr << "Record type err" << GIF_ERROR << Type <<"\n";
          PrintGifError();
         //DGifCloseFile(GifFile);
         //return false;
         }
        std::cout << "/n record type complete" << Type << "\n";
   
        switch (Type) {
	    case IMAGE_DESC_RECORD_TYPE:
              if(DGifGetImageDesc(GifFile)!=1)
              {
                PrintGifError();
                // DGifCloseFile(GifFile);
                std::cerr << "Image desc err";
                return false;
              }
              f=1;
              m_spec = ImageSpec (GifFile->Image.Width, GifFile->Image.Height,
                       3,(m_bit_depth+1) == 16 ? TypeDesc::UINT16 : TypeDesc::UINT8);
                      
              std::cout << "image desc complete"<< GifFile->ImageCount;
              break;
	    case EXTENSION_RECORD_TYPE:
		/* Skip any extension blocks in file except comments: */
		if (DGifGetExtension(GifFile, &ExtCode, &Extension) == GIF_ERROR) {
		    PrintGifError();
		    exit(EXIT_FAILURE);
		}
		while (Extension != NULL) {
		    std::cout << "extension code" << ExtCode <<"\n";
		    switch(ExtCode)
		     {
		     case COMMENT_EXT_FUNC_CODE:
		        
		        /*Extension[Extension[0]+1] = '\000';   
			Comment = (char*) realloc(Comment, strlen(Comment) + Extension[0] + 1);
			strcat(Comment, (char*)Extension+1);
			*/
			break;
		     case GRAPHICS_EXT_FUNC_CODE: 
		        /*strcat(BlockSize, (char*)Extension+1);
		        strcat(PackedField, (char*)Extension+2); 
		        DelayTime = (int)Extension[3];
		        strcat(ColorIndex, (char*)Extension+4);
		        */break;
		     case PLAINTEXT_EXT_FUNC_CODE: 
		        
		        break;
		     case APPLICATION_EXT_FUNC_CODE:
		        /*for(c=0;c<=7;c++)
		         strcat((Identifier+c),(char*)Extension[c+1]);
		        for(c=0;c<=2;c++)
		         strcat((AuthentCode+c),(char*)Extension[9+c]);
		         Terminator =0;
		         */
		        
		         break;
		    }
		    if (DGifGetExtensionNext(GifFile, &Extension) == GIF_ERROR) {
			PrintGifError();
			std:: cerr << "Extension block failure";
		    }
		    std::cout <<"\n" << Type << Extension;
		}
		break;
	    case TERMINATE_RECORD_TYPE:
		break;
	    default:		    /* Should be traps by DGifGetRecordType. */
		break;
	}
	
   }
   while ((Type!=TERMINATE_RECORD_TYPE && GifFile->ImageCount==0));
           
   //m_interlace_type = GifFile->Image.Interlace;
   //m_bit_depth = GifFile->Image.ColorMap->BitsPerPixel;
   std::cout <<"\n image count" <<GifFile->ImageCount;
   
   m_spec.default_channel_names ();
   m_spec.attribute ("oiio:ColorSpace", "sRGB");
   m_next_scanline = 0;
   newspec= m_spec;
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
    std::cout << "\nInterlace value" << GifFile->Image.Interlace;
    if (GifFile->Image.Interlace) {
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
			    //GifQprintf("\b\b\b\b%-4d", Count++);
			    if (DGifGetLine(GifFile, &ScreenBuffer[j][Col],
				Width) == GIF_ERROR) {
				PrintGifError();
				//exit(EXIT_FAILURE);
				return false;
			    }
			}
		}
		else {
		if (m_next_scanline > y) {
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
			if (DGifGetLine(GifFile, (GifPixelType *)data,
				z) == GIF_ERROR) {
			    PrintGifError();
			    //exit(EXIT_FAILURE);
			    return false;
			}
		    }
		}
    
     
       
       return true;
  }

}
}


