name = "OpenImageIO"

version = "2.1.3"

authors = [
    "Contributors to OpenImageIO",
]


description = \
"""
OpenImageIO (OIIO) is a library for reading, writing, and processing images 
in a wide variety of file formats, using a format-agnostic API. OIIO emphasizes 
formats and functionality used in professional, large-scale visual effects and 
feature film animation, and it is used uniquitously by large VFX studios, 
as well as incorporated into many commercial products.
"""

tools = [
    'iconvert',
    'idiff',
    'igrep',
    'iinfo',
    'maketx',
    'oiiotool',
]

variants = [
    ['platform-linux', 'python-2'],
]

hashed_variants = True

def commands():
    #https://github.com/nerdvegas/rez/wiki/Building-Packages#passing-arguments
    if building:
        env.CMAKE_MODULE_PATH.append("{root}/share/cmake/Modules/")
        env.CPATH.append("{root}/include")
        env.OPENIMAGEIO_ROOT_DIR = "{root}"

    env.PYTHONPATH.append('{root}/lib/python2.7/site-packages') # need to figure out a way to build in a versionless directory
    env.PATH.append('{root}/bin')
