#!/usr/bin/env bash

if [[ ! -e ../oiio-images ]] ; then
    git clone https://github.com/OpenImageIO/oiio-images.git -b RB-2.4 --depth 1 ../oiio-images
    # NOTE: on release branches, this should include `-b RB-x.y`` so that
    # updates to the master oiio-images don't break tests in release branches.
fi

# if [[ ! -e ../libtiffpic ]] ; then
#     curl -o pics-3.8.0.tar.gz https://download.osgeo.org/libtiff/pics-3.8.0.tar.gz
#     tar xf pics-3.8.0.tar.gz -C ..
# fi

if [[ ! -e ../openexr-images ]] ; then
    git clone https://github.com/openexr/openexr-images.git --depth 1 ../openexr-images
fi

# if [[ ! -e ../bmpsuite ]] ; then
#     mkdir ../bmpsuite
#     curl -o bmpsuite.zip http://entropymine.com/jason/bmpsuite/bmpsuite.zip
#     unzip bmpsuite.zip -d ../bmpsuite
# fi
