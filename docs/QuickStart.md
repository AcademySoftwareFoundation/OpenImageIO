OpenImageIO Usage Quick Start
=============================

> [!NOTE]
> For information about how to *install* or *build* OpenImageIO, please see
> the [INSTALL guide](../INSTALL.md). This "Usage Quick Start" guide is
> intended to help you get started using OpenImageIO once you have it
> installed.

To illustrate how to do a simple common image operation -- reading a TIFF file
and writing it back as an OpenEXR file, with all necessary conversions -- we
will show how this can be accomplished through several different OpenImageIO
APIs and programs.

In all cases, we will assume we have a file called `input.tif` and we wish to
convert it to `output.exr`.

### Using `oiiotool` command-line utility

```bash
$ oiiotool input.tif -o output.exr
```

### High-level ImageBuf API

In C++:

```cpp
#include <OpenImageIO/imagebuf.h>

void main()
{
    OIIO::ImageBuf image("input.tif");
    image.write("output.exr");
}
```

In Python:

```python
import OpenImageIO as oiio

image = oiio.ImageBuf("input.tif")
image.write("output.exr")
```

### Low level ImageInput/ImageOutput API

In C++:

```cpp
#include <OpenImageIO/imageio.h>
#include <cstddef>
#include <memory>

void main()
{
    auto input = oiio.ImageInput.open("input.tif")
    OIIO::ImageSpec spec = input.spec()
    size_t nbytes = spec.image_bytes();
    std::unique_ptr<std::byte[]> pixels = new std::byte[nbytes];
    input->read_image(0, 0, 0, spec.nchanels, OIIO::TypeUnknown, pixels.get());
    input->close();

    auto output = oiio.ImageOutput.create("output.exr");
    output->open("output.exr", spec);
    output->write_image(TypeUnknown, pixels.get());
    output->close();
}
```

In Python:

```python
import OpenImageIO as oiio

input = oiio.ImageInput.open("input.tif")
spec = input.spec()
pixels = input.read_image()
# Note: read_image will return a NumPy multi-dimensional array holding
# all the pixel values of the image.

output = oiio.ImageOutput.create("output.exr")
output.open("output.exr", spec)
output.write_image(pixels)
# Note: write_image expects a NumPy multi-dimensional array holding
# all the pixel values of the image.
```

### Next Steps

This is just a simple example to give you a flavor of the different major
interfaces. For more advanced usage, you may want to explore the
[documentation](https://docs.openimageio.org).
