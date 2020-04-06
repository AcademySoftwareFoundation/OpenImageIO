=====================
OpenImageIO |version|
=====================

.. only:: not latex

    ------------------------
    Programmer Documentation
    ------------------------

    Editor: Larry Gritz, lg@openimageio.org


    OpenImageIO (OIIO) is a library for reading, writing, and processing
    images in a wide variety of file formats, using a format-agnostic API.
    OIIO emphasizes formats and functionality used in professional,
    large-scale visual effects and feature film animation, and it is used
    ubiquitously by large VFX studios, as well as incorporated into many
    commercial products.

    Quick Links:
    `Web site <https://openimageio.org>`_ /
    `GitHub repository <https://github.com/OpenImageIO/oiio>`_ /
    `Developer mail list <http://lists.openimageio.org/listinfo.cgi/oiio-dev-openimageio.org>`_ /
    `BSD License <https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md>`_


------------

.. default-role:: code


.. only:: not latex

    Contents
    --------

.. toctree::
   :maxdepth: 1

   copyr
   oiiointro

.. toctree::
   :caption: The OpenImageIO Library APIs
   :maxdepth: 2

   imageioapi
   imageoutput
   imageinput
   writingplugins
   builtinplugins
   imagecache
   texturesys
   imagebuf
   imagebufalgo
   pythonbindings

.. toctree::
   :caption: Image Utilities
   :maxdepth: 2

   oiiotool
   iinfo
   iconvert
   igrep
   idiff
   maketx

.. toctree::
   :caption: Appendices
   :maxdepth: 2

   stdmetadata
   glossary




.. comment
    Helpful links when writing documentation:

    Doxygen docs:
        http://www.doxygen.nl/manual/index.html
    Sphinx docs:
        http://www.sphinx-doc.org/en/master/contents.html
        http://www.sphinx-doc.org/en/master/usage/restructuredtext/domains.html#cpp-domain
    Breathe docs:
        https://github.com/michaeljones/breathe
        https://breathe.readthedocs.io/en/latest/directives.html
    RST:


