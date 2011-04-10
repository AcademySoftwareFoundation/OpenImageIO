#include "typedesc.h"
#include "imageio.h"

/* TODO */

OIIO_PLUGIN_NAMESPACE_BEGIN

OIIO_PLUGIN_EXPORTS_BEGIN
DLLEXPORT ImageOutput *xpm_output_imageio_create () { return NULL; }
DLLEXPORT const char *xpm_output_extensions[] = { "xpm", NULL };
OIIO_PLUGIN_EXPORTS_END

OIIO_PLUGIN_NAMESPACE_END