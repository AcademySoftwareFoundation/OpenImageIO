#!/bin/bash

# The Linux VM used by Travis has OpenEXR 1.x. We really want 2.x.

if [ ! -e ${HOME}/openexr-install ] ; then
    mkdir ${HOME}/openexr-install
fi

EXRVERSION=2.2.0

# Clone OpenEXR project (including IlmBase) from GitHub and build
if [ ! -e ../openexr ] ; then
    git clone -b v${EXRVERSION} https://github.com/openexr/openexr.git ../openexr
    pushd ../openexr/IlmBase
    ./bootstrap && ./configure --prefix=${HOME}/openexr-install && make && make install
    cd ../OpenEXR
    ./bootstrap ; ./configure --prefix=${HOME}/openexr-install --with-ilmbase-prefix=${HOME}/openexr-install --disable-ilmbasetest && make && make install
    popd
fi

# Alternate approach: download the tarballs and build them.
# if [ ! -e ../ilmbase-${EXRVERSION} ] ; then
#     wget http://download.savannah.nongnu.org/releases/openexr/ilmbase-${EXRVERSION}.tar.gz
#     tar xf ilmbase-${EXRVERSION}.tar.gz -C ..
#     ( cd ../ilmbase-${EXRVERSION} ; ./bootstrap ; ./configure --prefix=${HOME}/openexr-install && make && make install )
# fi
# if [ ! -e ../openexr-${EXRVERSION} ] ; then
#     wget http://download.savannah.nongnu.org/releases/openexr/openexr-${EXRVERSION}.tar.gz
#     tar xf openexr-${EXRVERSION}.tar.gz -C ..
#     ( cd ../openexr-${EXRVERSION} ; ./bootstrap ; ./configure --prefix=${HOME}/openexr-install --with-ilmbase-prefix=${HOME}/openexr-install --disable-ilmbasetest && make && make install )
# fi

ls -R ${HOME}/openexr-install

#echo "listing .."
#ls ..

