.PHONY: figures

include ../make/detectplatform.mk
ifeq (${OIIOTOOL},)
  ifneq (${shell ls ./../../build/${platform}/src/oiiotool/oiiotool},)
    OIIOTOOL := "../../../build/${platform}/src/oiiotool/oiiotool"
  else
    OIIOTOOL := "../../../build/src/oiiotool/oiiotool"
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
	 OIIOTOOL="${OIIOTOOL}" bash ../makefigures.bash ; \
	)

figures.turd: makefigures.bash
	${MAKE} figures
	touch figures.turd

cleanfigures:
	cmake -E remove_directory figures
