PDFLATEX := pdflatex -halt-on-error -interaction=errorstopmode 

# by default, just make the document
all: openimageio.pdf


# document making rule: use pdflatex
openimageio.pdf: *.tex *.aux
	${PDFLATEX} openimageio.tex


# special command 'make index' to regenerate the index
index: openimageio_index

openimageio_index:
	${PDFLATEX} openimageio.tex
	${PDFLATEX} openimageio.tex
	makeindex openimageio
	${PDFLATEX} openimageio.tex

