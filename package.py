name = "oiio"

version = "3.5.82_spiArn"

authors = [
    "lg"
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
    ['platform-linux'],
]

def commands():
    env.PYTHONPATH.append('{root}/lib/python2.7/site-packages')
    env.PATH.append('{root}/bin')
