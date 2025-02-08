<!-- SPDX-License-Identifier: CC-BY-4.0 -->
<!-- Copyright Contributors to the OpenImageIO Project. -->

# Building the OpenImageIO documentation

The main OpenImageIO documentation can be read on [OpenImageIO
Documentation](https://docs.openimageio.org).

But sometimes -- such as for developers who are editing the documentation --
it is helpful to be able to build and preview it locally. These are the steps
required to build the documentation.

The main docs as they appear on ReadTheDocs consist of two parts:

- Individual class and function documentation that are in the comments
  in the various public header files of OpenImageIO.
- All the explanatory text found in `src/doc/*.rst`, which is processed
  by Sphinx to generate the final documentation, and which draw on the
  header documentation that was extracted by Doxygen.


## Building what you need to build the docs

To be able to fully build the documentation, there are some packages you
will need installed. 

### Python

You will need Python. 


### Doxygen

We don't use Doxygen for our final docs, but we do use it to generate some
intermediate files used by the Sphinx documentation (via the Breathe plugin),
so you'll need it installed.

You can read full build-from-source instructions for Doxygen
[here](https://www.doxygen.nl/manual/install.html), but that's unnecessary
for most users. There are several common ways to install via package
managers:

- Debian/Ubuntu/etc that use apt: `sudo apt install doxygen`
- RHEL/CentOS/Rocky/Alma/etc/ that use yum: `sudo yum install doxygen`
- Homebrew on macOS:  `brew install doxygen`
- MacPorts on macOS:  `sudo port install doxygen`


### Sphinx and its plugins

Our main documentation uses Sphinx, and several extensions. The easiest way to
install it is via Python PIP:

- [Sphinx](https://www.sphinx-doc.org/) documentation generator: `pip install -U sphinx`
- [Breathe](https://breathe.readthedocs.io) plugin for Sphinx for combining with Doxygen: `pip install breathe`
- [Sphinx-tabs](https://sphinx-tabs.readthedocs.io) plugin: `pip install -U sphinx-tabs`
- [Furo](https://github.com/pradyunsg/furo) theme: `pip install furo`



## Actually building the docs

The building of the documentation is separate from the main OpenImageIO
build system.

1. First, build OpenImageIO itself. You should probably know how to do this
   before you're writing documentation for other users. Note that the build
   itself will generate a lot of temporary files (like object files) from the
   compilation in the `build` subdirectory, and that will also be used as scratch space for building the documentation.

2. Do the rest of these steps in src/doc:
   
       cd src/doc

3. Build the Doxygen intermediate files

       # (still in the src/doc directory)
       make doxygen

   Don't worry about warnings. Only worry if you see errors.

   You only need to redo this step if you do one of the following things:
   - Edit any of the `.h` file from which documentation is extracted. In fact,
     more specifically, you need to redo this step if you CHANGE any of the
     doxygen-based documentation in the comments of the headers.
   - Start a fresh build, change which branch you're working on, or in any
     way delete your `build` directory. Remember, as pointed out above, that
     this is where the documentation is built, so if it disappears, you have
     to start over again.

4. Build the Sphinx documentation

       # (still in the src/doc directory)
       make sphinx

   You should see output that may spew some warnings (don't worry about
   that) and should end with something that looks like this:

       ...
       dumping search index in English (code: en)... done
       dumping object inventory... done
       build succeeded, 63 warnings.

   You need to redo this Sphinx step if you change any of the text
   documentation in `src/doc/*.rst` is changed. Note that if you are only
   changing the `.rst` files, there is no need to go all the way back to step
   3 to regenerate the Doxygen files.

5. Where are my docs?
   
   The HTML pages are in `../../build/sphinx`

   Point your browser at `index.html` in that directory to view the docs.




