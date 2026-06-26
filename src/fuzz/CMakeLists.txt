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

add_executable (fuzz_image fuzz_image.cpp)

target_link_libraries (fuzz_image PRIVATE OpenImageIO)

# -fsanitize=fuzzer-no-link instruments code for coverage tracing without
# linking the fuzzer runtime; the runtime comes from OIIO_FUZZING_ENGINE at
# link time. address+undefined catch memory errors and UB respectively.
target_compile_options (fuzz_image PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-omit-frame-pointer
)

target_link_options (fuzz_image PRIVATE
    ${OIIO_FUZZING_ENGINE}
    -fsanitize=address,undefined
)

# all_fuzz_targets is the conventional build alias used by OSS-Fuzz build.sh
# and local "cmake --build . --target all_fuzz_targets".
# add_custom_target (all_fuzz_targets DEPENDS fuzz_image)
install_targets (fuzz_image)
