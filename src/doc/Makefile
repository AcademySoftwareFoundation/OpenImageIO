.PHONY: figures

include ../make/detectplatform.mk
BUILDDIR := ../../build/${platform}
ifeq (${OIIOTOOL},)
  ifneq (${shell ls ${BUILDDIR}/bin/oiiotool},)
    OIIOTOOL := "${BUILDDIR}/bin/oiiotool"
  else
    OIIOTOOL := "../../build/bin/oiiotool"
  endif
endif


PDFLATEX := pdflatex -file-line-error --halt-on-error -interaction=errorstopmode

# by default, just make the document
all: openimageio.pdf


# document making rule: use pdflatex
openimageio.pdf: *.tex *.aux figures.turd
	${PDFLATEX} openimageio.tex


# special command 'make index' to regenerate the index
index: openimageio_index

openimageio_index: figures
	${PDFLATEX} openimageio.tex
	${PDFLATEX} openimageio.tex
	makeindex openimageio
	${PDFLATEX} openimageio.tex

figures:
	(cmake -E make_directory figures ; \
	 cd figures ; \
	 OIIOTOOL="../${OIIOTOOL}" bash ../makefigures.bash ; \
	)

figures.turd: makefigures.bash
	${MAKE} figures
	touch figures.turd

cleanfigures:
	cmake -E remove_directory figures


help:
	@echo "help"
	@echo "BUILDDIR=${BUILDDIR}"
	@echo "Sphinx targets:"
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(SPHINXBUILDDIR)" $(SPHINXOPTS) $(O)



doxygen:
	echo "make doxygen TBD"
	doxygen Doxyfile
	touch doxygen.turd

doxygen.turd: doxygen

doxygenclean:
	cmake -E remove_directory ../../build/doxygen
	cmake -E remove doxygen.turd
	cmake -E remove_directory ../../build/sphinx


# You can set these variables from the command line.
SPHINXOPTS    =
SPHINXBUILD   = sphinx-build
SOURCEDIR     = .
SPHINXBUILDDIR      = ../../build/sphinx

# Put it first so that "make" without argument is like "make help".
#help:

.PHONY: help Makefile

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
sphinx : Makefile conf.py
	# @$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(SPHINXBUILDDIR)" $(SPHINXOPTS) $(O)
	$(SPHINXBUILD) -b html "$(SOURCEDIR)" "$(SPHINXBUILDDIR)" $(SPHINXOPTS)

sphinxpdf : Makefile conf.py
	# @$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(SPHINXBUILDDIR)" $(SPHINXOPTS) $(O)
	$(SPHINXBUILD) -b latex "$(SOURCEDIR)" "$(SPHINXBUILDDIR)/latex" $(SPHINXOPTS)
	$(MAKE) -C $(SPHINXBUILDDIR)/latex all-pdf
# 	$(SPHINXBUILD) -b latexpdf "$(SOURCEDIR)" "$(SPHINXBUILDDIR)/latex" $(SPHINXOPTS)
