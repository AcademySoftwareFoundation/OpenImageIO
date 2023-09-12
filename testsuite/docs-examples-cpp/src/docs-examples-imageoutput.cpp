// BEGIN-imageoutput-simple
#include <OpenImageIO/imageio.h>
using namespace OIIO;

void simple_write()
{
    const char* filename = "simple.tif";
    const int xres = 320, yres = 240, channels = 3;
    unsigned char pixels[xres * yres * channels] = { 0 };

    std::unique_ptr<ImageOutput> out = ImageOutput::create(filename);
    if (!out)
        return;  // error
    ImageSpec spec(xres, yres, channels, TypeDesc::UINT8);
    out->open(filename, spec);
    out->write_image(TypeDesc::UINT8, pixels);
    out->close();
}
// END-imageoutput-simple



void scanlines_write()
{
    const char* filename = "scanlines.tif";
    const int xres = 320, yres = 240, channels = 3;

    std::unique_ptr<ImageOutput> out = ImageOutput::create(filename);
    if (!out)
        return;  // error
    ImageSpec spec(xres, yres, channels, TypeDesc::UINT8);

// BEGIN-imageoutput-scanlines
    unsigned char scanline[xres * channels] = { 0 };
    out->open (filename, spec);
    int z = 0;   // Always zero for 2D images
    for (int y = 0;  y < yres;  ++y) {
        // ... generate data in scanline[0..xres*channels-1] ...
        out->write_scanline (y, z, TypeDesc::UINT8, scanline);
    }
    out->close();
// END-imageoutput-scanlines
}



int main(int /*argc*/, char** /*argv*/)
{
    simple_write();
    scanlines_write();
    return 0;
}
