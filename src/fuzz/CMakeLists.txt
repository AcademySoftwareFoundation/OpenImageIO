# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# This file is only reached when OIIO_BUILD_FUZZ_TARGETS=ON (guarded in the
# root CMakeLists.txt).

if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message (FATAL_ERROR
        "OIIO_BUILD_FUZZ_TARGETS=ON requires clang. "
        "Set CMAKE_CXX_COMPILER=clang++ (detected: ${CMAKE_CXX_COMPILER_ID}).")
endif ()

# Resolve the fuzzing engine.
# - Local dev: defaults to -fsanitize=fuzzer (links the libFuzzer runtime).
# - OSS-Fuzz: sets LIB_FUZZING_ENGINE to a .a path for the chosen engine.
if (DEFINED ENV{LIB_FUZZING_ENGINE})
    set (OIIO_FUZZING_ENGINE "$ENV{LIB_FUZZING_ENGINE}")
else ()
    set (OIIO_FUZZING_ENGINE "-fsanitize=fuzzer")
endif ()

# Executable is named oiio_fuzz_image (not just fuzz_image) so that if it is
# ever accidentally built and installed, the name makes clear which package it
# belongs to. The source file keeps its shorter fuzz_image.cpp name.
add_executable (oiio_fuzz_image fuzz_image.cpp)

target_link_libraries (oiio_fuzz_image PRIVATE OpenImageIO)

# -fsanitize=fuzzer-no-link instruments code for coverage tracing without
# linking the fuzzer runtime; the runtime comes from OIIO_FUZZING_ENGINE at
# link time. address+undefined catch memory errors and UB respectively.
target_compile_options (oiio_fuzz_image PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-omit-frame-pointer
)

target_link_options (oiio_fuzz_image PRIVATE
    ${OIIO_FUZZING_ENGINE}
    -fsanitize=address,undefined
)

# all_fuzz_targets is the conventional build alias used by OSS-Fuzz build.sh
# and local "cmake --build . --target all_fuzz_targets".
# add_custom_target (all_fuzz_targets DEPENDS oiio_fuzz_image)
install_targets (oiio_fuzz_image)

# Install the libFuzzer options file alongside the binary. OSS-Fuzz/ClusterFuzz
# keys the options file to the binary basename, so it is named to match:
# oiio_fuzz_image.options.
install (FILES oiio_fuzz_image.options DESTINATION ${CMAKE_INSTALL_BINDIR})
