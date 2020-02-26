#!/bin/bash

# Terminate the script on error
set -e


#env
#echo " "
#env | grep -i rez | sort


GCCVER=${REZ_GCC_MAJOR_VERSION}.${REZ_GCC_MINOR_VERSION}
GCCXY=gcc${REZ_GCC_MAJOR_VERSION}${REZ_GCC_MINOR_VERSION}
GCCXYSP=${GCCXY}
if [[ "${GCCXY}" == "gcc48" ]] ; then
    GCCXYSP=${GCCXY}m64
fi
BOOSTXY=boost${REZ_BOOST_MAJOR_VERSION}${REZ_BOOST_MINOR_VERSION}

Boost_ROOT=

CMAKE_ARGS=" \
    -G Ninja \
    -DCMAKE_MAKE_PROGRAM=ninja \
    -DOIIO_SPIREZ=1 \
    -DSP_OS=rhel7 \
    -DCMAKE_DEBUG_POSTFIX=_d \
    -DUSE_SIMD=sse4.1,aes \
    -DEXTRA_CPP_ARGS:STRING=-DOIIO_SPI=1 \
    -DOIIO_SITE:STRING=spi \
    -DSPI_TESTS=1 \
    -DOIIO_NAMESPACE_INCLUDE_PATCH=1 \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DPYTHON_SITE_DIR=${REZ_BUILD_INSTALL_PATH}/python \
    -DCMAKE_C_COMPILER=/shots/spi/home/software/packages/llvm/9.0.0/gcc-${GCCVER}/bin/clang \
    -DCMAKE_CXX_COMPILER=/shots/spi/home/software/packages/llvm/9.0.0/gcc-${GCCVER}/bin/clang++ \
    -DCMAKE_C_FLAGS=--gcc-toolchain=/usr \
    -DCMAKE_CXX_FLAGS=--gcc-toolchain=/usr \
    -DTOOLCHAIN_FLAGS=--gcc-toolchain=/usr \
    -DPYTHON_VERSION=${REZ_PYTHON_VERSION} \
    -DOpenColorIO_ROOT=/shots/spi/home/lib/SpComp2/OpenColorIO/rhel7-gcc48m64/v2 \
    -DxTBB_ROOT=/net/apps/rhel7/intel/tbb \
    -DxTBB_LIBRARY=/net/apps/rhel7/intel/tbb/lib/intel64/gcc4.7 \
    -DHDF5_CUSTOM=1 \
    -DHDF5_LIBRARIES=/usr/lib64/libhdf5.so \
    -DNuke_ROOT=/net/apps/rhel7/foundry/nuke11.2v3 \
    -DLibheif_ROOT=/shots/spi/home/lib/arnold/rhel7/libheif-1.3.2 \
    -DFFMPEG_INCLUDE_DIR=/usr/include/ffmpegroot/ffmpeg34 \
    -DFFMPEG_LIBRARIES='/usr/lib64/ffmpegroot/ffmpeg345/libavcodec.so;/usr/lib64/ffmpegroot/ffmpeg345/libavformat.so;/usr/lib64/ffmpegroot/ffmpeg345/libavutil.so;/usr/lib64/ffmpegroot/ffmpeg345/libswscale.so;' \
    -DUSE_OPENCV=0 \
    -DVISIBILITY_MAP_FILE:STRING=${REZ_BUILD_SOURCE_PATH}/../hidesymbols.map \
    -DTIFF_INCLUDE_DIR:STRING=/usr/include \
    -DTIFF_LIBRARIES:STRING=/net/soft_scratch/users/lg/tiff-4.0.3/rhel7/lib/libtiff.a \
    -DEXTRA_DSO_LINK_ARGS:STRING='-Wl,--exclude-libs,libtiff.a' \
    -DCMAKE_INSTALL_RPATH=${REZ_BUILD_INSTALL_PATH}/lib \
    -DOIIO_REZ_INSTALL_PATH=${REZ_BUILD_INSTALL_PATH} \
    -DCMAKE_BUILD_WITH_INSTALL_RPATH=1 \
    -DTBB_ROOT=${REZ_TBB_ROOT} \
    -DOpenVDB_ROOT=/shots/spi/home/lib/SpComp2/openvdb/rhel7-${GCCXYSP}-${BOOSTXY}/v6020001 \
    -DField3D_ROOT=/shots/spi/home/lib/SpComp2/Field3D/rhel7-${GCCXYSP}-${BOOSTXY}/v413 \
    "

# Unneeded:
#CMAKE_ARGS+="-DOpenVDB_ROOT=/shots/spi/home/lib/SpComp2/openvdb/rhel7-${GCCXY}-${BOOSTXY}/v6020001 "
#    -DField3D_ROOT=/shots/spi/home/lib/SpComp2/Field3D/rhel7-${GCCXY}-${BOOSTXY}/v412 



#BOGUS+=\
#    -g3 \
#    -DSP_PROC=x86_64 \
#    -DSP_PLATFORM=linux \
#    -DSP_REZ_OS=CentOS-7 \
#    -DSPI_COMPILER_PLATFORM=gcc-6.3 \
#    -DPYTHONINTERP_FOUND=1 \
#    -DOpenEXR_ROOT=/shots/spi/home/software/packages/OpenEXR/2.2.0/gcc-${REZ_GCC_VERSION}/python-3.7 \
#    -DOIIO_REZ_INSTALL_PATH=/shots/spi/home/software/packages/OpenImageIO/2.2.1.0/gcc-6.3/python-3.7/boost-1.66 \
#    -DCMAKE_INSTALL_RPATH=/shots/spi/home/software/packages/OpenImageIO/2.2.1.0/gcc-6.3/python-3.7/boost-1.66/lib \
#    -DCMAKE_INSTALL_LIBDIR="${REZ_BUILD_INSTALL_PATH}/lib" \



if [[ "${GCCVER}" == "4.8" ]] ; then
#    CMAKE_ARGS+="-DField3D_ROOT=/shots/spi/home/lib/SpComp2/Field3D/rhel7-${GCCXYSP}-${BOOSTXY}/v413 "
#    CMAKE_ARGS+="-DOpenVDB_ROOT=/shots/spi/home/lib/SpComp2/openvdb/rhel7-${GCCXYSP}-${BOOSTXY}/v6020001 "
    CMAKE_ARGS+="-DLIBRAW_INCLUDEDIR_HINT=/usr/include/libraw-0.18.11 "
    CMAKE_ARGS+="-DLIBRAW_LIBDIR_HINT=/usr/lib64/libraw-0.18.11 "
fi
if [[ "${GCCVER}" == "6.3" ]] ; then
    CMAKE_ARGS+="-DLibRaw_ROOT=${REZ_LIBRAW_ROOT} "
    #/shots/spi/home/software/packages/LibRaw/0.20.0-dev1/gcc-6.3 "
fi


if [[ "${REZ_BOOST_VERSION}" == "1.55" ]] ; then
    CMAKE_ARGS+=" \
        -DBOOST_CUSTOM=1 \
        -DBoost_VERSION=${REZ_BOOST_VERSION} \
        -DBoost_INCLUDE_DIRS=/usr/include/boost_${REZ_BOOST_VERSION} \
        -DBoost_LIBRARY_DIRS=/usr/lib64/boost_${REZ_BOOST_VERSION} \
        -DBoost_LIBRARIES:STRING='/usr/lib64/boost_${REZ_BOOST_VERSION}/libboost_filesystem-gcc48-mt-1_55.so;/usr/lib64/boost_${REZ_BOOST_VERSION}/libboost_regex-gcc48-mt-1_55.so;/usr/lib64/boost_${REZ_BOOST_VERSION}/libboost_system-gcc48-mt-1_55.so;/usr/lib64/boost_${REZ_BOOST_VERSION}/libboost_thread-gcc48-mt-1_55.so' \
        "
elif [[ "${REZ_BOOST_VERSION}" == "1.55sp" ]] ; then
    # This may be unused. Excise soon?
    CMAKE_ARGS+=" \
        -DBOOST_CUSTOM=1 \
        -DBoost_VERSION=${REZ_BOOST_VERSION} \
        -DBoost_INCLUDE_DIRS=/usr/include/boost_${REZ_BOOST_VERSION} \
        -DBoost_LIBRARY_DIRS=/usr/lib64/boost_${REZ_BOOST_VERSION} \
        -DBoost_LIBRARIES:STRING='/usr/lib64/boost_${REZ_BOOST_VERSION}/libspboost_filesystem-gcc48-mt-1_55.so;/usr/lib64/boost_${REZ_BOOST_VERSION}/libspboost_regex-gcc48-mt-1_55.so;/usr/lib64/boost_${REZ_BOOST_VERSION}/libspboost_system-gcc48-mt-1_55.so;/usr/lib64/boost_${REZ_BOOST_VERSION}/libspboost_thread-gcc48-mt-1_55.so' \
        "
else
    CMAKE_ARGS+=" \
        -DBOOST_INCLUDEDIR=/usr/include/boostroot/boost${REZ_BOOST_VERSION}.0 \
        -DBOOST_LIBRARYDIR=/usr/lib64/boostroot/boost${REZ_BOOST_VERSION}.0 \
    "
fi


echo ""
echo $CMAKE_ARGS
echo ""

# Other settings specific to rez variants.
# The Rez variant order MUST match what's in package.py !!!
case "$REZ_BUILD_VARIANT_INDEX" in
0)
    # 0: gcc 6.3/C++14 compat, python 2.7, boost 1.70
    # This is the variant needed by Maya 2020 & Houdini 18 & Roman
    OPENEXR_VERSION=2.4.0
    ;;
1)
    # 1: gcc 6.3/C++14 compat, python 3.7, boost 1.70
    # VFX Platform 2020-ish
    OPENEXR_VERSION=2.4.0
    ;;
2)
    # 2: Legacy SPI: gcc 4.8, boost 1.55, python 2.7
    OPENEXR_VERSION=2.2.0
    ;;
3)
    # 3: Special for Jon Ware/Substance: legacy SPI, but python 3.6,
    OPENEXR_VERSION=2.2.0
    ;;
*)
    echo "Bad REZ_BUILD_VARIANT_INDEX = ${REZ_BUILD_VARIANT_INDEX}"
        exit 1
esac

if [[ "${OPENEXR_VERSION}" == "2.2.0" ]] ; then
    CMAKE_ARGS+=-DOpenEXR_ROOT=/shots/spi/home/software/packages/OpenEXR/${OPENEXR_VERSION}/gcc-${GCCVER}/python-${REZ_PYTHON_VERSION}
else
    CMAKE_ARGS+=-DOpenEXR_ROOT=/shots/spi/home/software/packages/OpenEXR/${OPENEXR_VERSION}/gcc-${GCCVER}
fi


#echo "\n\n\n"
#echo "CMAKE_ARGS=${CMAKE_ARGS}"
#echo "\n\n\n"

# We build the debug libs first
#cmake $CMAKE_ARGS "$@" --config Debug -DVERBOSE=1 \
#        -DCMAKE_INSTALL_PREFIX="${REZ_BUILD_INSTALL_PATH}" \
#        "${REZ_BUILD_SOURCE_PATH}/../../.."

cmake $CMAKE_ARGS "$@" -DVERBOSE=1 \
        -DCMAKE_INSTALL_PREFIX="${REZ_BUILD_INSTALL_PATH}" \
        "${REZ_BUILD_SOURCE_PATH}/../../.."

if [[ $REZ_BUILD_INSTALL -eq "1" ]]; then
    cmake --build . --target install
else
    cmake --build .
fi
