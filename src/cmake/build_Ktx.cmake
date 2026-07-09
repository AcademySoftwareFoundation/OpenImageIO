# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

set_cache (Ktx_BUILD_VERSION main "Ktx version for local builds")
set_cache (Ktx_GIT_REPOSITORY "https://github.com/KhronosGroup/KTX-Software.git"
              "git repository from where to fetch libktx")
set_cache (Ktx_GIT_TAG "${Ktx_BUILD_VERSION}" "Git branch or tag")
set_cache (Ktx_GIT_COMMIT "2ca7d54109f4c23298a969f22b68769e94138de5"
              "commit hash to verify tag/branch against")
set_cache (Ktx_BUILD_SHARED_LIBS OFF ${LOCAL_BUILD_SHARED_LIBS_DEFAULT}
           DOC "Should a local Ktx build, if necessary, build shared libraries" ADVANCED)

string (MAKE_C_IDENTIFIER ${Ktx_BUILD_VERSION} Ktx_VERSION_IDENT)

# Override C/C++ compiler (useful when running CI with unsupported Intel's compiler ICX)
set_cache (KTX_CMAKE_C_COMPILER ${CMAKE_C_COMPILER} "libktx build C compiler override" ADVANCED)
set_cache (KTX_CMAKE_CXX_COMPILER ${CMAKE_CXX_COMPILER} "libktx build C++ compiler override" ADVANCED)

#
# The only two tested libktx versions are:
#   - v5.0.0-rc1 (newest supported)
#   - v4.3.2     (newest version with minimum CMake required <= 3.18)
# libktx v4.3.2 requires a different setup than newer libktx versions
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
  build_dependency_with_cmake(Ktx
      VERSION         ${Ktx_BUILD_VERSION}
      GIT_REPOSITORY  ${Ktx_GIT_REPOSITORY}
      GIT_TAG         ${Ktx_GIT_TAG}
      GIT_COMMIT      ${Ktx_GIT_COMMIT}
      CMAKE_ARGS
          -D KTX_FEATURE_STATIC_LIBRARY=${Ktx_BUILD_STATIC_LIBS}  # no BUILD_SHARED_LIBS in older libktx versions ...
          -D CMAKE_POSITION_INDEPENDENT_CODE=ON
          -D KTX_FEATURE_TOOLS=OFF
          -D KTX_FEATURE_TESTS=OFF
          -D KTX_FEATURE_KTX1=ON  # TODO: test with OFF
          -D KTX_FEATURE_KTX2=ON 
          -D KTX_FEATURE_VK_UPLOAD=OFF
          -D KTX_FEATURE_GL_UPLOAD=OFF
          -D CMAKE_C_COMPILER=${KTX_CMAKE_C_COMPILER}
          -D CMAKE_CXX_COMPILER=${KTX_CMAKE_CXX_COMPILER}
      )
else() # v5.0.0-rc1 or a branch with similar CMake setup
  build_dependency_with_cmake(Ktx
      VERSION         ${Ktx_BUILD_VERSION}
      GIT_REPOSITORY  ${Ktx_GIT_REPOSITORY}
      GIT_TAG         ${Ktx_GIT_TAG}
      GIT_COMMIT      ${Ktx_GIT_COMMIT}
      SOURCE_SUBDIR   lib  # To only build Ktx, cmake has to point to: KTX-Software/lib
      CMAKE_ARGS
          -D BUILD_SHARED_LIBS=${Ktx_BUILD_SHARED_LIBS}
          -D CMAKE_INSTALL_LIBDIR=lib
          -D CMAKE_POSITION_INDEPENDENT_CODE=ON
          -D LIBKTX_VERSION_READ_ONLY=OFF
          -D LIBKTX_VERSION_FULL=ON
          -D LIBKTX_FEATURE_KTX1=ON  # Setting this to OFF causes linker issues
          -D LIBKTX_FEATURE_KTX2=ON
          -D LIBKTX_FEATURE_VK_UPLOAD=OFF
          -D LIBKTX_FEATURE_GL_UPLOAD=OFF
          -D LIBKTX_FEATURE_ETC_UNPACK=OFF # This has some weird licensing and I don't feel comfortable including it ...
          -D CMAKE_C_COMPILER=${KTX_CMAKE_C_COMPILER}
          -D CMAKE_CXX_COMPILER=${KTX_CMAKE_CXX_COMPILER}
          # as per KTX-Software:
          # > Intel Macs have support for SSE, but if you're building universal
          # > binaries, you have to disable SSE or the build will fail.
      )
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
