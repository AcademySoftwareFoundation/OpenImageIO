#!/usr/bin/env bash
#

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


set -ex

# Make extra space on the runners
df -h .
time rm -rf /usr/local/lib/android /host/root/usr/local/lib/android &
sleep 3
# rather than block, delete in background, but give it a few secs to start
# clearing things out before moving on.
# Other candidates, if we need it: /usr/share/dotnet /usr/local/.ghcup


#
# Install system packages when those are acceptable for dependencies.
#
if [[ "$ASWF_ORG" != ""  ]] ; then
    # Using ASWF container

    #ls /etc/yum.repos.d

    # This will show how much space is taken by each installed package, sorted
    # by size in KB.
    # rpm -qa --queryformat '%10{size} - %-25{name} \t %{version}\n' | sort -n

    # I would like this to free space by removing packages we don't need.
    # BUT IT DOESN'T, because uninstalling a package just ends is visibility
    # to the runtime, it doesn't remove it from the static container image
    # that's taking up the disk space. So this is pointless. But leaving it
    # here to remind myself not to waste time trying it again.
    # time sudo yum remove -y nsight-compute-2022.3.0 libcublas-devel-11-8 libcublas-11-8 libcusparse-devel-11-8 libnpp-devel-11-8 libnpp-11-8 libcurand-devel-11-8 libcurand-11-8 || true
    # time sudo yum remove -y nsight-compute-2024.3.1 libcublas-devel-12-6 libcublas-12-6 libcusparse-devel-12-6 libnpp-devel-12-6 libnpp-12-6 libcurand-devel-12-6 libcurand-12-6 || true

    # time sudo dnf upgrade --refresh || true
    if [[ "${DO_RPMFUSION_REPO:-0}" != "0" ]] ; then
        time sudo dnf install --nogpgcheck https://mirrors.rpmfusion.org/free/el/rpmfusion-free-release-$(rpm -E %rhel).noarch.rpm -y || true
    fi

    if [[ "$ASWF_VFXPLATFORM_VERSION" == "2022" ]] ; then
        # CentOS 7 based containers need the now-nonexistent centos repo to be
        # excluded or all the subsequent yum install commands will fail.
        yum-config-manager --disable centos-sclo-rh || true
        sed -i 's,^mirrorlist=,#,; s,^#baseurl=http://mirror\.centos\.org/centos/$releasever,baseurl=https://vault.centos.org/7.9.2009,' /etc/yum.repos.d/CentOS-Base.repo
    fi

    # time time sudo yum install -y giflib giflib-devel || true
    if [[ "${USE_OPENCV}" != "0" ]] ; then
        time sudo yum install -y opencv opencv-devel || true
    fi
    if [[ "${USE_FFMPEG}" != "0" ]] ; then
        time sudo dnf install -y ffmpeg ffmpeg-devel || true
    fi
    if [[ "${USE_FREETYPE:-1}" != "0" ]] ; then
        time sudo yum install -y freetype freetype-devel || true
    fi
    if [[ "${USE_LIBRAW:-0}" != "0" ]] ; then
        time sudo yum install -y LibRaw LibRaw-devel || true
    fi
    if [[ "${EXTRA_DEP_PACKAGES}" != "" ]] ; then
        time sudo yum install -y ${EXTRA_DEP_PACKAGES} || true
    fi
    if [[ "${PIP_INSTALLS}" != "" ]] ; then
        time pip3 install ${PIP_INSTALLS} || true
    fi

    if [[ "$CXX" == "icpc" || "$CC" == "icc" || "$USE_ICC" != "" ]] ; then
        # Lock down icc to 2022.1 because newer versions hosted on the Intel
        # repo require a glibc too new for the ASWF CentOS7-based containers
        # we run CI on.
        sudo cp src/build-scripts/oneAPI.repo /etc/yum.repos.d
        sudo /usr/bin/yum install -y intel-oneapi-compiler-dpcpp-cpp-and-cpp-classic-2022.1.0.x86_64
        set +e; source /opt/intel/oneapi/setvars.sh --config oneapi_2022.1.0.cfg; set -e
    elif [[ "$CXX" == "icpc" || "$CC" == "icc" || "$USE_ICC" != "" || "$CXX" == "icpx" || "$CC" == "icx" || "$USE_ICX" != "" ]] ; then
        sudo cp src/build-scripts/oneAPI.repo /etc/yum.repos.d
        sudo yum install -y intel-oneapi-compiler-dpcpp-cpp-and-cpp-classic
        # If we needed to lock down to a particular version, we could:
        # sudo yum install -y intel-oneapi-compiler-dpcpp-cpp-and-cpp-classic-2023.1.0.x86_64
        set +e; source /opt/intel/oneapi/setvars.sh; set -e
        echo "Verifying installation of Intel(r) oneAPI DPC++/C++ Compiler:"
        icpx --version
    fi

else
    # Using native Ubuntu runner

    if [[ "${SKIP_APT_GET_UPDATE}" != "1" ]] ; then
        time sudo apt-get update
    fi

    if [[ "${SKIP_SYSTEM_DEPS_INSTALL}" != "1" ]] ; then
        time sudo apt-get -q install -y --fix-missing \
            git cmake ninja-build ccache g++ \
            libtiff-dev libgif-dev libpng-dev libjpeg-dev \
            libraw-dev libwebp-dev \
            libavcodec-dev libavformat-dev libswscale-dev libavutil-dev \
            dcmtk libopenvdb-dev \
            libfreetype6-dev \
            libopencolorio-dev \
            libtbb-dev \
            libdeflate-dev bzip2
        # Iffy ones get the "|| true" treatment so failure is ok
        time sudo apt-get -q install -y --fix-missing \
            libjxl-dev || true
    fi
    if [[ "${USE_OPENCV}" != "0" ]] && [[ "${INSTALL_OPENCV}" != "0" ]] ; then
        sudo apt-get -q install -y --fix-missing libopencv-dev || true
    fi
    if [[ "${QT_VERSION:-5}" == "5" ]] ; then
        time sudo apt-get -q install -y \
            qt5-default || /bin/true
    elif [[ "${QT_VERSION}" == "6" ]] ; then
        time sudo apt-get -q install -y qt6-base-dev || /bin/true
    fi
    if [[ "${EXTRA_DEP_PACKAGES}" != "" ]] ; then
        time sudo apt-get -q install -y ${EXTRA_DEP_PACKAGES}
    fi

    if [[ "${USE_PYTHON}" != "0" ]] ; then
        time sudo apt-get -q install -y python3-numpy
    fi
    if [[ "${PIP_INSTALLS}" != "" ]] ; then
        time pip3 install ${PIP_INSTALLS}
    fi

    if [[ "$USE_LIBHEIF" != "0" ]] ; then
       sudo add-apt-repository ppa:strukturag/libde265 || true
       sudo add-apt-repository ppa:strukturag/libheif || true
       time sudo apt-get -q install -y libheif-plugin-aomdec \
            libheif-plugin-aomenc libheif-plugin-libde265 \
            libheif-plugin-x265 libheif-dev || true
    fi

    if [[ "${USE_FFMPEG}" != "0" ]] ; then
        time sudo apt-get -q install -y ffmpeg || true
    fi

    export CMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu:$CMAKE_PREFIX_PATH

    if [[ "$CXX" == "icpc" || "$CC" == "icc" || "$USE_ICC" != "" || "$USE_ICX" != "" ]] ; then
        time sudo apt-get -q install -y wget
        wget https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS-2023.PUB
        sudo apt-key add GPG-PUB-KEY-INTEL-SW-PRODUCTS-2023.PUB
        echo "deb https://apt.repos.intel.com/oneapi all main" | sudo tee /etc/apt/sources.list.d/oneAPI.list
        time sudo apt-get install -y intel-oneapi-compiler-dpcpp-cpp-and-cpp-classic
        set +e; source /opt/intel/oneapi/setvars.sh; set -e
    fi

fi

if [[ "$CMAKE_VERSION" != "" ]] ; then
    source src/build-scripts/build_cmake.bash
fi
cmake --version


#
# If we're using clang to compile on native Ubuntu, we need to install it.
# If on an ASWF CentOS docker container, it already is installed.
#
if [[ "$LLVM_VERSION" != "" ]] ; then
    source src/build-scripts/build_llvm.bash
fi



#
# Packages we need to build from scratch.
#

if [[ "$PYBIND11_VERSION" != "0" ]] ; then
    source src/build-scripts/build_pybind11.bash
fi

if [[ "$OPENEXR_VERSION" != "" ]] ; then
    source src/build-scripts/build_openexr.bash
fi

if [[ "$WEBP_VERSION" != "" ]] ; then
    source src/build-scripts/build_webp.bash
fi

if [[ "$LIBTIFF_VERSION" != "" ]] ; then
    source src/build-scripts/build_libtiff.bash
fi

if [[ "$LIBRAW_VERSION" != "" ]] ; then
    source src/build-scripts/build_libraw.bash
fi

if [[ "$OPENJPEG_VERSION" != "" ]] ; then
    source src/build-scripts/build_OpenJPEG.bash
fi

if [[ "$PUGIXML_VERSION" != "" ]] ; then
    source src/build-scripts/build_pugixml.bash
    export MY_CMAKE_FLAGS+=" -DUSE_EXTERNAL_PUGIXML=1 "
fi

if [[ "$OPENCOLORIO_VERSION" != "" ]] ; then
    source src/build-scripts/build_opencolorio.bash
fi

if [[ "$PTEX_VERSION" != "" ]] ; then
    source src/build-scripts/build_Ptex.bash
fi

if [[ "$ABI_CHECK" != "" ]] ; then
    source src/build-scripts/build_abi_tools.bash
fi

if [[ "$LIBJPEGTURBO_VERSION" != "" ]] ; then
    source src/build-scripts/build_libjpeg-turbo.bash
fi

if [[ "$FREETYPE_VERSION" != "" ]] ; then
    source src/build-scripts/build_Freetype.bash
fi

if [[ "$LIBPNG_VERSION" != "" ]] ; then
    source src/build-scripts/build_libpng.bash
fi

if [[ "$USE_ICC" != "" ]] ; then
    # We used gcc for the prior dependency builds, but use icc for OIIO itself
    echo "which icpc:" $(which icpc)
    export CXX=icpc
    export CC=icc
fi

df -h .
df -h /host/root || true

# Save the env for use by other stages
src/build-scripts/save-env.bash
