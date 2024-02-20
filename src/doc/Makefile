# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

.PHONY: figures

#include ../make/detectplatform.mk
BUILDDIR := ../../build
ifeq (${OIIOTOOL},)
  ifneq (${shell ls ${BUILDDIR}/bin/oiiotool},)
    OIIOTOOL := "${BUILDDIR}/bin/oiiotool"
  else
    OIIOTOOL := "../../build/bin/oiiotool"
  endif
endif


help:
	@echo "help"
	@echo "BUILDDIR=${BUILDDIR}"
	@echo "Interesting targets:"
	@echo "  sphinx         Build html docs in BUILDDIR/sphinx"
	@echo "  sphinxpdf      Build pdf docs in BUILDDIR/sphinx"
	@echo "  doxygen        Doxygen pre-pass needed before sphinx"
	@echo "  doxygenclean   Reset doxygen pass"
	@echo "  figures        Rebuild all figures (DON'T DO THIS!)"
	@echo "  cleanfigures   Clear all figures (DEFINITELY DON'T DO THIS!)"
#	@echo "Sphinx targets:"
#	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(SPHINXBUILDDIR)" $(SPHINXOPTS) $(O)


.PHONY: help Makefile


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
