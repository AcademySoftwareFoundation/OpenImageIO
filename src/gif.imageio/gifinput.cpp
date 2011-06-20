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
    GifRecordType * Type;
    std::string m_filename;           ///< Stash the filename
    FILE *m_file;                     ///< Open image handle
    int m_subimage;                   ///< What subimage are we looking at?
    int m_next_scanline;
    int m_bit_depth; 
    //GifPixelType *Line;                 ///< PNG bit depth
    int m_interlace_type;
    bool m_raw; 
    
    /// Reset everything to initial state
    ///
    void init () {
        GifFile =NULL;
        Type = NULL;
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
    printf("OPEN");
    m_filename = name;
    m_subimage = 0;
    
    GifFile = DGifOpenFileName(name.c_str());

   if(DGifGetRecordType(GifFile, Type)!=1)
   {
    PrintGifError();
    //DGifCloseFile(GifFile);
    return false;
   }
   
   if(DGifGetImageDesc(GifFile)!=1)
   {
    PrintGifError();
    // DGifCloseFile(GifFile);
    return false;
   }  
   m_interlace_type = GifFile->Image.Interlace;
   m_bit_depth = GifFile->Image.ColorMap->BitsPerPixel;
   newspec = ImageSpec (GifFile->Image.Width, GifFile->Image.Height, 
                       GifFile->Image.ColorMap->ColorCount,
                       m_bit_depth == 16 ? TypeDesc::UINT16 : TypeDesc::UINT8);
                      
   newspec.default_channel_names ();
   newspec.attribute ("oiio:ColorSpace", "sRGB");
   m_next_scanline = 0;
   printf("opened ");
    return true;
}
bool 
GIFInput::close ()
{
  if(DGifCloseFile(GifFile)==1)
   return true;
  PrintGifError();
  return false; 
}  

bool
GIFInput::open (const std::string &name, ImageSpec &newspec,
                const ImageSpec &config)
{
    printf("opened2 ");
    const ImageIOParameter *p = config.find_attribute ("_gif:raw",
                                                       TypeDesc::TypeInt);
    m_raw = p && *(int *)p->data();
    return open (name, newspec);
}


bool
GIFInput::read_native_scanline (int y, int z, void *data)
{
    
    if (y < 0 || y >= GifFile->Image.Height)   // out of range scanline
        return false;
    if (m_next_scanline > y) {
        // User is trying to read an earlier scanline than the one we're
        // up to.  Easy fix: close the file and re-open.
        ImageSpec dummyspec;
        int subimage = current_subimage();
        if (! close ()  ||
            ! open (m_filename, dummyspec)  ||
            ! seek_subimage (subimage, 0, dummyspec))
            return false;    // Somehow, the re-open failed
        assert (m_next_scanline == 0 && current_subimage() == subimage);
    }
    for (  ;  m_next_scanline <= y;  ++m_next_scanline) {
        // Keep reading until we've read the scanline we really need
        if (DGifGetLine(GifFile,(GifPixelType *)data,z) != 1) {
            PrintGifError();
            return false;
        }
    }
   
       
       return true;
  }
}
}





