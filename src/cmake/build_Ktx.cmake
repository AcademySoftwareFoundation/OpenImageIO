# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

set_cache (Ktx_BUILD_VERSION main "Ktx version for local builds")
set       (Ktx_GIT_REPOSITORY "https://github.com/KhronosGroup/KTX-Software.git")
set_cache (Ktx_GIT_TAG "${Ktx_BUILD_VERSION}" "Git branch or tag")
set_cache (Ktx_GIT_COMMIT "2ca7d54109f4c23298a969f22b68769e94138de5"
              "commit hash to verify tag/branch against")
set_cache (Ktx_BUILD_SHARED_LIBS OFF ${LOCAL_BUILD_SHARED_LIBS_DEFAULT}
              DOC "Should a local Ktx build, if necessary, build shared libraries" ADVANCED)

# TODO: if libktx is built as a shared library, astcenc have to be linked-against.
#       I don't know how to 'cleanly' do this in OIIO CMake (yet).
#       Even though all CIs pass, do not merge before addressing this!

string (MAKE_C_IDENTIFIER ${Ktx_BUILD_VERSION} Ktx_VERSION_IDENT)

# Override C/C++ compiler (useful when running CI with unsupported Intel's compiler ICX)
set_cache (KTX_CMAKE_C_COMPILER ${CMAKE_C_COMPILER} "libktx build C compiler override" ADVANCED)
set_cache (KTX_CMAKE_CXX_COMPILER ${CMAKE_CXX_COMPILER} "libktx build C++ compiler override" ADVANCED)

# On x86_64, libktx defaults to using AVX2 for ASTC. For Intel-based MacOS, the
# default AVX2 requires x86_64h which may not be available (e.g., Intel-based
# MacOS Github Actions CIs). For ARM64 libktx defaults to using Neon. For
# 'unknown' CPUs, SIMD is disabled (see KTX-Software/lib/CMakeLists.txt for details).
#
# Possible values:
#   - "": default, let libktx decide
#   - ASTCENC_ISA_NATIVE: native SIMD
#   - ASTCENC_ISA_NONE: disable SIMD
#   - ASTCENC_ISA_SVE_256: slowest on Arm
#   - ASTCENC_ISA_SVE_128: 2nd fastest on Arm
#   - ASTCENC_ISA_NEON: fasted on Arm
#   - ASTCENC_ISA_AVX2: fastest on x86_64 (may not be supported on Intel-based MacOS runners)
#   - ASTCENC_ISA_SSE41: 2nd fastest on x86_64 (guaranteed to be supported)
#   - ASTCENC_ISA_SSE2: slowest on x86_64 (guaranteed to be supported)
#
set_cache (Ktx_ASTCENC_ISA "" "ASTC specific SIMD instruction set. See astc-encoder/CMakeLists.txt for more details" ADVANCED)

set(CMAKE_ARGS_LIST
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DCMAKE_C_COMPILER=${KTX_CMAKE_C_COMPILER}
    -DCMAKE_CXX_COMPILER=${KTX_CMAKE_CXX_COMPILER}
)

if(Ktx_ASTCENC_ISA)
    list(APPEND CMAKE_ARGS_LIST -D${Ktx_ASTCENC_ISA}=ON)
endif()

#
# The only three tested libktx versions are:
#   - v0.0.0     (main:HEAD which is versionless)
#   - v5.0.0-rc1 (newest supported)
#   - v4.3.2     (newest version with minimum CMake required <= 3.18)
# 
# libktx v4.3.2 is needed for older systems (see CI runners with 'oldest' in
# name) on which CMake >= 3.22 cannot be installed (this is the minimum version
# required by libktx > 4.3.2).
#
# libktx v4.3.2 requires a different setup than newer libktx versions.
# 
# for detailed build instructions, see:
# https://github.com/KhronosGroup/KTX-Software/blob/main/BUILDING.md
#
# KTX-Software not only provides Ktx but also a set of CLI tools and load
# test applications that we do not need. We only need the libktx component
# without any GPU texture loading functionalities.
#
if (Ktx_BUILD_VERSION STREQUAL "v4.3.2")
  # Because you can't negate variables in CMake ...
  if(Ktx_BUILD_SHARED_LIBS)
      set(Ktx_BUILD_STATIC_LIBS OFF)
  else()
      set(Ktx_BUILD_STATIC_LIBS ON)
  endif()
  list(APPEND CMAKE_ARGS_LIST
      -DKTX_FEATURE_STATIC_LIBRARY=${Ktx_BUILD_STATIC_LIBS}  # no BUILD_SHARED_LIBS in older libktx versions ...
      -DKTX_FEATURE_TOOLS=OFF
      -DKTX_FEATURE_TESTS=OFF
      -DKTX_FEATURE_KTX1=ON
      -DKTX_FEATURE_KTX2=ON
      -DKTX_FEATURE_KTX2=ON
      -DKTX_FEATURE_GL_UPLOAD=OFF
  )
  build_dependency_with_cmake(Ktx
      VERSION         ${Ktx_BUILD_VERSION}
      GIT_REPOSITORY  ${Ktx_GIT_REPOSITORY}
      GIT_TAG         ${Ktx_GIT_TAG}
      GIT_COMMIT      ${Ktx_GIT_COMMIT}
      CMAKE_ARGS      ${CMAKE_ARGS_LIST})
else() # v5.0.0-rc1 or a branch with similar CMake setup
  list(APPEND CMAKE_ARGS_LIST
      -DBUILD_SHARED_LIBS=${Ktx_BUILD_SHARED_LIBS}
      -DCMAKE_INSTALL_LIBDIR=lib
      -DLIBKTX_VERSION_READ_ONLY=OFF
      -DLIBKTX_VERSION_FULL=ON
      -DLIBKTX_FEATURE_KTX1=ON  # Setting this to OFF causes linker issues
      -DLIBKTX_FEATURE_KTX2=ON
      -DLIBKTX_FEATURE_VK_UPLOAD=OFF
      -DLIBKTX_FEATURE_GL_UPLOAD=OFF
      -DLIBKTX_FEATURE_ETC_UNPACK=OFF  # This has some weird licensing and I don't feel comfortable including it ...
  )
  build_dependency_with_cmake(Ktx
      VERSION         ${Ktx_BUILD_VERSION}
      GIT_REPOSITORY  ${Ktx_GIT_REPOSITORY}
      GIT_TAG         ${Ktx_GIT_TAG}
      GIT_COMMIT      ${Ktx_GIT_COMMIT}
      SOURCE_SUBDIR   lib  # To only build Ktx, cmake has to point to: KTX-Software/lib
      CMAKE_ARGS      ${CMAKE_ARGS_LIST})
endif()

# Set some things up that we'll need for a subsequent find_package to work
set (Ktx_ROOT ${Ktx_LOCAL_INSTALL_DIR})
set (Ktx_DIR ${Ktx_LOCAL_INSTALL_DIR}/lib/cmake/ktx)

# Signal to caller that we need to find again at the installed location
# set (Ktx_REFIND TRUE)
# set (Ktx_REFIND_ARGS CONFIG)

find_package (Ktx CONFIG REQUIRED
             HINTS 
                    ${Ktx_LOCAL_INSTALL_DIR}/lib/cmake/ktx/
                    ${Ktx_LOCAL_INSTALL_DIR}
             )

if (Ktx_BUILD_SHARED_LIBS)
  # install_local_dependency_libs (pkgname libname)
    install_local_dependency_libs (Ktx ktx) # notice libname is lowercase
endif ()
