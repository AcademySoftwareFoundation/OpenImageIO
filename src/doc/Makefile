
# by default, just make the document
all: openimageio.pdf


# document making rule: use pdflatex
openimageio.pdf: *.tex *.aux
	pdflatex openimageio


# special command 'make index' to regenerate the index
index: openimageio_index

openimageio_index:
	pdflatex openimageio
	pdflatex openimageio
	makeindex openimageio
	pdflatex openimageio

