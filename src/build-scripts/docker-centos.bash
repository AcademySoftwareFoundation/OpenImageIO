# docker run -ti --rm -v $(pwd):/jc quay.io/pypa/manylinux2014_x86_64:latest bash
set -ex

if [[ "$OSTYPE" != "msys" ]]; then
    yum install wget less ccache -y
fi

python -m pip install conan --user

if [[ "$OSTYPE" != "msys" ]]; then
    if [[ ! -d /opt/cmake ]]; then
        currentPath=$(pwd)
        wget https://github.com/Kitware/CMake/releases/download/v3.23.2/cmake-3.23.2-linux-x86_64.tar.gz
        mkdir -p /opt/cmake
        cd /opt
        tar -xzf "${currentPath}/cmake-3.23.2-linux-x86_64.tar.gz"
        rm "${currentPath}/cmake-3.23.2-linux-x86_64.tar.gz"
        mv cmake-3.23.2-linux-x86_64/* cmake/
        rm -rf cmake-3.23.2-linux-x86_64/
        cd "${currentPath}"
    fi

    cp $(which ccache) /usr/local/bin/ || true
    ln -sf $(which ccache) /usr/local/bin/gcc
    ln -sf $(which ccache) /usr/local/bin/g++
    ln -sf $(which ccache) /usr/local/bin/cc
    ln -sf $(which ccache) /usr/local/bin/c++
    ccache --max-files 0 --max-size 0
fi

export PATH=/opt/cmake/bin:/usr/local/bin:~/.local/bin:$PATH
if [[ "$OSTYPE" == 'msys' ]]; then
    export PATH=$PATH:~/AppData/Roaming/Python/Python310/Scripts
fi

export CMAKE_CXX_STANDARD=17

export USE_OPENVDB=0
export USE_QT5=0
export USE_OPENGL=0
export USE_NUKE=0
export USE_R3DSDK=0
export USE_JPEGTURBO=0

export USE_FFMPEG=1
export USE_PNG=1
export USE_OPENCV=0 # Always disable
export USE_GIF=1
export USE_PTEX=1
export USE_WEBP=1
export USE_OPENCOLORIO=1
export USE_OPENEXR=1
export USE_LIBHEIF=1
export USE_TIFF=1
export USE_LIBRAW=1
export USE_OPENJPEG=1
export USE_FREETYPE=1
export USE_CONAN=1

export OPENCOLORIO_VERSION=v2.1.2
export PUGIXML_VERSION=v1.11.4
export FMT_VERSION=9.0.0
export OPENEXR_VERSION=v3.1.5
export PYBIND11_VERSION=v2.9.2

export OPENCOLORIO_BUILD_SHARED_LIBS=OFF
export PUGIXML_BUILD_SHARED_LIBS=OFF
export OPENEXR_BUILD_SHARED_LIBS=OFF

export MY_CMAKE_FLAGS="${MY_CMAKE_FLAGS} -DBUILD_SHARED_LIBS=OFF -DLINKSTATIC=ON -DOIIO_BUILD_TESTS=OFF -DOPENCOLORIO_NO_CONFIG=ON -DOIIO_BUILD_TOOLS=ON"
export PYTHON_VERSION=3.10

export OPENIMAGEIO_OPTIONS="openexr:core=1"

source src/build-scripts/ci-startup.bash

export CMAKE_BUILD_PARALLEL_LEVEL=16

unset TERM

mkdir -p build

pushd build
if [[ $(conan profile list | grep default) == '' ]]; then
    conan profile new default --detect
fi

if [[ "$OSTYPE" == linux-* ]]; then
    conan profile update settings.compiler=gcc default
    conan profile update settings.compiler.version=10 default
    conan profile update settings.compiler.libcxx=libstdc++ default
elif [[ "$OSTYPE" == "msys" ]]; then
    conan profile update settings.compiler='Visual Studio' default
    conan profile update settings.compiler.runtime=MT default
    conan profile update settings.compiler.version=16 default
    conan profile update settings.arch=x86_64 default

    export CONAN_CMAKE_FILES=$(pwd)
fi

conanArgs=''

if [[ "$USE_FFMPEG" == '1' ]]; then
    cp ../conanfile.txt .
    conanArgs='--build=openjpeg --build=libx264 --build=ffmpeg --build=libx265 --build expat -c tools.system.package_manager:mode=install -c tools.system.package_manager:tool=yum'
    if [[ "$OSTYPE" == "msys" ]]; then
        cat ../conanfile.txt | grep -v with_vulkan | grep -v with_pulse > conanfile.txt
        conanArgs=''
    fi

else
    cat ../conanfile.txt | grep -v 'ffmpeg/' > conanfile.txt
fi

PATH=/opt/python/cp310-cp310/bin:$PATH CONAN_SYSREQUIRES_SUDO=0 CONAN_SYSREQUIRES_MODE=enabled conan install . --build #$conanArgs

popd

source src/build-scripts/gh-installdeps.bash

# source src/build-scripts/ci-build.bash

/opt/python/cp310-cp310/bin/python setup.py bdist_wheel
