name = "oiio"

version = "3.5.83"

authors = [
    "lg",
]


description = \
"""
The primary target audience for OIIO is VFX studios and developers of tools such 
as renderers, compositors, viewers, and other image-related software you'd find 
in a production pipeline.
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
