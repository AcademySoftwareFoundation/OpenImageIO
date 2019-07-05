#!/usr/bin/env python3

# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# http://www.sphinx-doc.org/en/master/config

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
# import os
# import sys
# sys.path.insert(0, os.path.abspath('.'))

import sys
import os
import shlex
import subprocess


# -- Project information -----------------------------------------------------

# The master toctree document.
master_doc = 'index'

default_role = 'code'

project = 'OpenImageIO'
copyright = '2008-present, Contributors to OpenImageIO'
author = 'Larry Gritz'

# The version info for the project you're documenting, acts as replacement for
# |version| and |release|, also used in various other places throughout the
# built documents.

# LG addition: we search for it in the CMakeLists.txt so we don't need to
# keep modifying this file:
version = '0.0'
release = '0.0.0'
import re
version_regex = re.compile(r'project .* VERSION ((\d+\.\d+)\.\d+)')
f = open('../../CMakeLists.txt')
for l in f:
    aa=re.search(version_regex, l)
    if aa is not None:
       release = aa.group(1)
       version = aa.group(2)
       break
f.close()



# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
#'sphinx.ext.pngmath', 
#'sphinx.ext.todo', 
'breathe' ]

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = []


# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
#html_theme = 'alabaster'
#html_theme = 'astropy-sphinx-theme'
html_theme = 'sphinx_rtd_theme'

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['_static']


# Breathe options
breathe_projects = { "oiio": "../../build/doxygen/xml" }
breathe_default_project = "oiio"
breathe_domain_by_extension = {'h': 'cpp'}
breathe_default_members = ()
primary_domain = 'cpp'
highlight_language = 'cpp'


read_the_docs_build = os.environ.get('READTHEDOCS', None) == 'True'

if read_the_docs_build:
    subprocess.call('mkdir -p ../../build/doxygen', shell=True)
    subprocess.call('echo "Calling Doxygen"', shell=True)
    subprocess.call('doxygen Doxyfile', shell=True)

