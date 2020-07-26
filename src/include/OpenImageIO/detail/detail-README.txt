This directory is for headers that need to be part of the OpenImageIO
installation, but that contain material that is not officially part of the
documented OpenImageIO public API.

Client software of the OpenImageIO library or APIs are advised not to
directly include headers in the detail/ subdirectory, nor to directly call
anything in those headers or in the pvt or detail namespaces.

Therefore, anything in this directory may be changed arbitrarily as part of
a minor version release (but not part of a patch version release, since
changes here may well change the ABI/linkage).
