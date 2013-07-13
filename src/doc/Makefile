PDFLATEX := pdflatex -halt-on-error -interaction=errorstopmode 

# by default, just make the document
all: openimageio.pdf


# document making rule: use pdflatex
openimageio.pdf: *.tex *.aux
	${PDFLATEX} openimageio


# special command 'make index' to regenerate the index
index: openimageio_index

openimageio_index:
	${PDFLATEX} openimageio
	${PDFLATEX} openimageio
	makeindex openimageio
	${PDFLATEX} openimageio

