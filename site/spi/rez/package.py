# -*- coding: utf-8 -*-
# Documentation on packages and available attributes can be found at:
# https://github.com/nerdvegas/rez/wiki/Package-Definition-Guide

name = 'OpenImageIO'

version = '2.2.1.1'
# NOTE: must keep this in sync with the master CMakeLists.txt
# TODO: can we auto-substitute?
# @PROJECT_VERSION_MAJOR@.@PROJECT_VERSION_MINOR@.@PROJECT_VERSION_PATCH@.@PROJECT_VERSION_TWEAK@'

description = \
    """
    Rez package of OpenImageIO
    """

authors = ['Larry Gritz <lg@imageworks.com>']

# Add dependencies to other Rez packages here
#requires = [ 'platform-@SP_PLATFORM@', 'arch-@SP_PROC@', 'os-@SP_REZ_OS@',
#             'OpenEXR-@OPENEXR_VERSION_MAJOR@.@OPENEXR_VERSION_MINOR@'
#           ]


build_command = 'bash {root}/build.sh'

requires = [
    #  'OpenEXR-2.4.0'
    # , 'OpenColorIO-1.1'
]

build_requires = [
    'LibRaw-0.20.0-dev1',
    'Field3D-0.413',
    'tbb',
    'openvdb-0.6020001',
]

variants = [
    # 0: gcc 6.3/C++14 compat, python 2.7, boost 1.70
    # This is the variant needed by Maya 2020 & Houdini 18 & Roman
      [ 'gcc-6.3', 'python-2.7', 'boost-1.70' ]  # OpenEXR-2.4

    # 1: gcc 6.3/C++14 compat, python 3.7, boost 1.70, OpenEXR 2.4
    # VFX Platform 2020-ish
    , [ 'gcc-6.3', 'python-3.7', 'boost-1.70' ]  # OpenEXR-2.4

    # 2: Legacy SPI / Maya 2019
    , [ 'gcc-4.8', 'python-2.7', 'boost-1.55' ]  # OpenEXR-2.2

    # 3: Special for Jon Ware/Substance: legacy SPI, but python 3.6,
    , [ 'gcc-4.8', 'python-3.6', 'boost-1.55' ]  # OpenEXR-2.2

    # 4: Legacy SPI with sp-namespaced boost
    # , [ 'gcc-4.8', 'python-2.7', 'boost-1.55sp' ]  # OpenEXR-2.2

    # 5: gcc 6.3/C++14 compat, python 3.7, intermediate boost 1.66
    # Does anybody need this?
    # , [ 'gcc-6.3', 'python-3.7', 'boost-1.66' ]  # OpenEXR-2.2

    # 6: gcc 6.3/C++14 compat, old python & boost
    # Does anybody need this?
    # , [ 'gcc-6.3', 'python-2.7', 'boost-1.55' ]  # OpenEXR-2.2
]


def commands():
    # Add import statements here, not at the top of the file!

    # Uncomment if you have executables under bin/
    env.PATH.append("{root}/bin")
    env.LD_LIBRARY_PATH.append("{root}/lib")

    # Uncomment if you have Python code under python/
    env.PYTHONPATH.append("{root}/python")

    # For SpImport.Packages, append the python directory
    #env.SPIMPORT_PACKAGES.append("{root}/python")

    if building:
        env.CMAKE_MODULE_PATH.append('{root}')
    #    Add env vars or custom code you want executed only at build time
