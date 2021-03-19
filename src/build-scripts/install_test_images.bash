#!/usr/bin/env bash

if [[ ! -e ../oiio-images ]] ; then
    git clone https://github.com/OpenImageIO/oiio-images.git ../oiio-images
fi

if [[ ! -e ../libtiffpic ]] ; then
    curl -o pics-3.8.0.tar.gz https://download.osgeo.org/libtiff/pics-3.8.0.tar.gz
    tar xf pics-3.8.0.tar.gz -C ..
fi

if [[ ! -e ../openexr-images ]] ; then
    git clone https://github.com/openexr/openexr-images.git ../openexr-images
fi

if [[ ! -e ../bmpsuite ]] ; then
    mkdir ../bmpsuite
    curl -o bmpsuite.zip http://entropymine.com/jason/bmpsuite/bmpsuite.zip
    unzip bmpsuite.zip -d ../bmpsuite
fi
