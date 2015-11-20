#!/bin/bash

if [ ! -e ../oiio-images ] ; then
    git clone https://github.com/OpenImageIO/oiio-images.git ../oiio-images
fi

if [ ! -e ../libtiffpic ] ; then
    wget ftp://ftp.remotesensing.org/pub/libtiff/pics-3.8.0.tar.gz
    tar xf pics-3.8.0.tar.gz -C ..
fi

#echo "listing .."
#ls ..

