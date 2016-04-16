CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT (openexr)

SET(OPENEXR_VERSION_MAJOR "2")
SET(OPENEXR_VERSION_MINOR "2")
SET(OPENEXR_VERSION_PATCH "0")

SET(OPENEXR_VERSION ${OPENEXR_VERSION_MAJOR}.${OPENEXR_VERSION_MINOR}.${OPENEXR_VERSION_PATCH})
SET(OPENEXR_VERSION_API ${OPENEXR_VERSION_MAJOR}_${OPENEXR_VERSION_MINOR})


# enable the tests
ENABLE_TESTING()


# distro building 
SET(CPACK_PACKAGE_VERSION_MAJOR "${OPENEXR_VERSION_MAJOR}")
SET(CPACK_PACKAGE_VERSION_MINOR "${OPENEXR_VERSION_MINOR}")
SET(CPACK_PACKAGE_VERSION_PATCH "${OPENEXR_VERSION_PATCH}")
SET(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
    "${CMAKE_PROJECT_NAME}-${OPENEXR_VERSION}"
  )
set(CPACK_SOURCE_IGNORE_FILES
  "/.git*;/.cvs*;${CPACK_SOURCE_IGNORE_FILES}")
INCLUDE ( CPack )



# Allow the developer to select if Dynamic or Static libraries are built
OPTION (BUILD_SHARED_LIBS    "Build Shared Libraries" ON)
OPTION (USE_ZLIB_WINAPI      "Use ZLib Win API"       OFF)
OPTION (NAMESPACE_VERSIONING "Use Namespace Versioning" ON)
OPTION (BUILD_UTILS "Build the binary/utility programs" ON)
OPTION (BUILD_TESTS "Build the tests" ON)

# Setup osx rpathing
SET (CMAKE_MACOSX_RPATH 1)
SET (BUILD_WITH_INSTALL_RPATH 1)

ADD_DEFINITIONS ( -DHAVE_CONFIG_H -DILM_IMF_TEST_IMAGEDIR="${CMAKE_SOURCE_DIR}/IlmImfTest/" )

INCLUDE_DIRECTORIES (
  ${CMAKE_CURRENT_BINARY_DIR}/config
  IlmImf
  IlmImfUtil
  exrmaketiled
  exrenvmap
  exrmakepreview
  exrmultiview
  IlmImfFuzzTest
)

FIND_PACKAGE(ZLIB REQUIRED)
INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIR})

IF (NOT WIN32)
  SET ( PTHREAD_LIB pthread )
ENDIF()

INCLUDE_DIRECTORIES ( ${ILMBASE_PACKAGE_PREFIX}/include/OpenEXR )
LINK_DIRECTORIES ( ${ILMBASE_PACKAGE_PREFIX}/lib )
MESSAGE ( "ILMBASE_PACKAGE_PREFIX = " ${ILMBASE_PACKAGE_PREFIX})

SET (LIB_TYPE STATIC)
IF (BUILD_SHARED_LIBS)
  # User wants to build Dynamic Libraries, so change the LIB_TYPE variable to CMake keyword 'SHARED'
  SET (LIB_TYPE SHARED)
    IF (WIN32)
    ADD_DEFINITIONS(-DOPENEXR_DLL)
  ENDIF ()
ENDIF ()

IF (USE_ZLIB_WINAPI)
  ADD_DEFINITIONS(-DZLIB_WINAPI)
ENDIF ()

# Test for GCC-style inline asm support with AVX instructions
INCLUDE (CheckCXXSourceCompiles)
CHECK_CXX_SOURCE_COMPILES (
	"
	int main()
	{
	 #if defined(__GNUC__) && defined(__SSE2__) 
		 int n   = 0;
		 int eax = 0;
		 int edx = 0;
		 __asm__(
			 \"xgetbv     ;\"
			 \"vzeroupper  \"
			 : \"=a\"(eax), \"=d\"(edx) : \"c\"(n) : );
	 #else
		 #error No GCC style inline asm supported for AVX instructions
	 #endif
	}
	" HAVE_GCC_INLINE_ASM_AVX)

# Check if sysconf(_SC_NPROCESSORS_ONLN) can be used for CPU count
CHECK_CXX_SOURCE_COMPILES (
    "
    #include <unistd.h>
    int main()
    {
        sysconf(_SC_NPROCESSORS_ONLN);
    }
    " HAVE_SYSCONF_NPROCESSORS_ONLN)


##########################
# OpenEXRConfig.h generation
##########################
IF (WIN32)
  FILE ( WRITE ${CMAKE_CURRENT_BINARY_DIR}/config/OpenEXRConfig.h "#define OPENEXR_IMF_HAVE_COMPLETE_IOMANIP 1\n" )
ELSEIF (APPLE)
  FILE ( WRITE ${CMAKE_CURRENT_BINARY_DIR}/config/OpenEXRConfig.h "#define OPENEXR_IMF_HAVE_DARWIN 1\n" )
  FILE ( APPEND ${CMAKE_CURRENT_BINARY_DIR}/config/OpenEXRConfig.h "#define OPENEXR_IMF_HAVE_COMPLETE_IOMANIP 1\n" )
  FILE ( APPEND ${CMAKE_CURRENT_BINARY_DIR}/config/OpenEXRConfig.h "#include <string.h>\n" )
ELSE ()
  # Linux
  FILE ( WRITE ${CMAKE_CURRENT_BINARY_DIR}/config/OpenEXRConfig.h "#define OPENEXR_IMF_HAVE_LINUX_PROCFS 1\n" )
  FILE ( APPEND ${CMAKE_CURRENT_BINARY_DIR}/config/OpenEXRConfig.h "#define OPENEXR_IMF_HAVE_COMPLETE_IOMANIP 1\n" )
  FILE ( APPEND ${CMAKE_CURRENT_BINARY_DIR}/config/OpenEXRConfig.h "#define OPENEXR_IMF_HAVE_LARGE_STACK 1\n" )
ENDIF()
  
IF (NAMESPACE_VERSIONING)
    FILE ( APPEND ${CMAKE_CURRENT_BINARY_DIR}/config/OpenEXRConfig.h "#define OPENEXR_IMF_INTERNAL_NAMESPACE_CUSTOM 1\n")
    FILE ( APPEND ${CMAKE_CURRENT_BINARY_DIR}/config/OpenEXRConfig.h "#define OPENEXR_IMF_NAMESPACE Imf\n" )
    FILE ( APPEND ${CMAKE_CURRENT_BINARY_DIR}/config/OpenEXRConfig.h "#define OPENEXR_IMF_INTERNAL_NAMESPACE Imf_${OPENEXR_VERSION_API}\n\n" )
ELSE ()
    FILE ( APPEND ${CMAKE_CURRENT_BINARY_DIR}/config/OpenEXRConfig.h "#define OPENEXR_IMF_INTERNAL_NAMESPACE_CUSTOM 0\n")
    FILE ( APPEND ${CMAKE_CURRENT_BINARY_DIR}/config/OpenEXRConfig.h "#define OPENEXR_IMF_NAMESPACE Imf\n" )
    FILE ( APPEND ${CMAKE_CURRENT_BINARY_DIR}/config/OpenEXRConfig.h "#define OPENEXR_IMF_INTERNAL_NAMESPACE Imf\n\n" )
ENDIF ()

FILE ( APPEND ${CMAKE_CURRENT_BINARY_DIR}/config/OpenEXRConfig.h "#define OPENEXR_VERSION_STRING \"${OPENEXR_VERSION}\"\n" )
FILE ( APPEND ${CMAKE_CURRENT_BINARY_DIR}/config/OpenEXRConfig.h "#define OPENEXR_PACKAGE_STRING \"OpenEXR ${OPENEXR_VERSION}\"\n" )


FILE ( APPEND ${CMAKE_CURRENT_BINARY_DIR}/config/OpenEXRConfig.h "
#define OPENEXR_VERSION_MAJOR ${OPENEXR_VERSION_MAJOR}
#define OPENEXR_VERSION_MINOR ${OPENEXR_VERSION_MINOR}
#define OPENEXR_VERSION_PATCH ${OPENEXR_VERSION_PATCH}
")


  FILE ( APPEND ${CMAKE_CURRENT_BINARY_DIR}/config/OpenEXRConfig.h "
// Version as a single hex number, e.g. 0x01000300 == 1.0.3
#define OPENEXR_VERSION_HEX ((OPENEXR_VERSION_MAJOR << 24) | \\
                             (OPENEXR_VERSION_MINOR << 16) | \\
                             (OPENEXR_VERSION_PATCH <<  8))\n
")

IF (HAVE_GCC_INLINE_ASM_AVX)
  FILE ( APPEND ${CMAKE_CURRENT_BINARY_DIR}/config/OpenEXRConfig.h "#define OPENEXR_IMF_HAVE_GCC_INLINE_ASM_AVX 1\n" )
ENDIF()
  
IF (HAVE_SYSCONF_NPROCESSORS_ONLN)
  FILE ( APPEND ${CMAKE_CURRENT_BINARY_DIR}/config/OpenEXRConfig.h "#define OPENEXR_IMF_HAVE_SYSCONF_NPROCESSORS_ONLN 1\n" )
ENDIF()
  
SET (OPENEXR_LIBSUFFIX "")
SET (ILMBASE_LIBSUFFIX "")
IF (NAMESPACE_VERSIONING)
  SET ( OPENEXR_LIBSUFFIX "-${OPENEXR_VERSION_API}" )
  # assume same NAMESPACE_VERSION setting for IlmBase for now
  SET ( ILMBASE_LIBSUFFIX "-${OPENEXR_VERSION_API}" )
ENDIF ()

##########################
# IlmImf library
##########################
ADD_SUBDIRECTORY ( IlmImf )

SET_TARGET_PROPERTIES ( IlmImf
  PROPERTIES
  VERSION 22.0.0
  SOVERSION 22
  OUTPUT_NAME "IlmImf${OPENEXR_LIBSUFFIX}"
  )
  
##########################
# IlmImfUtil library
##########################
ADD_SUBDIRECTORY ( IlmImfUtil )

SET_TARGET_PROPERTIES ( IlmImfUtil
  PROPERTIES
  VERSION 22.0.0
  SOVERSION 22
  OUTPUT_NAME "IlmImfUtil${OPENEXR_LIBSUFFIX}"
  )
  
##########################
# Example Code
##########################
ADD_SUBDIRECTORY ( IlmImfExamples )

  
##########################
# Tests
##########################
if (BUILD_TESTS)
  ADD_SUBDIRECTORY ( IlmImfTest )
  ADD_SUBDIRECTORY ( IlmImfUtilTest )
  ADD_SUBDIRECTORY ( IlmImfFuzzTest )
endif ()


##########################
# Binaries / Utilities
##########################
if (BUILD_UTILS)
  ADD_SUBDIRECTORY ( exrheader )
  ADD_SUBDIRECTORY ( exrmaketiled )
  ADD_SUBDIRECTORY ( exrstdattr )
  ADD_SUBDIRECTORY ( exrmakepreview )
  ADD_SUBDIRECTORY ( exrenvmap )
  ADD_SUBDIRECTORY ( exrmultiview )
  ADD_SUBDIRECTORY ( exrmultipart )
endif ()


##########################
# Installation
##########################

INSTALL ( FILES
  ${CMAKE_CURRENT_BINARY_DIR}/config/OpenEXRConfig.h
  DESTINATION
  ${CMAKE_INSTALL_PREFIX}/include/OpenEXR
  )

# Documentation
INSTALL ( FILES
  doc/TechnicalIntroduction.pdf
  doc/ReadingAndWritingImageFiles.pdf
  doc/OpenEXRFileLayout.pdf
  doc/MultiViewOpenEXR.pdf
  doc/InterpretingDeepPixels.pdf
  doc/TheoryDeepPixels.pdf
  DESTINATION
  ${CMAKE_INSTALL_PREFIX}/share/doc/OpenEXR-${OPENEXR_VERSION}
  )

# Examples
INSTALL ( FILES
  IlmImfExamples/main.cpp
  IlmImfExamples/drawImage.cpp
  IlmImfExamples/rgbaInterfaceExamples.cpp
  IlmImfExamples/rgbaInterfaceTiledExamples.cpp
  IlmImfExamples/generalInterfaceExamples.cpp
  IlmImfExamples/lowLevelIoExamples.cpp
  IlmImfExamples/previewImageExamples.cpp
  IlmImfExamples/generalInterfaceTiledExamples.cpp
  IlmImfExamples/generalInterfaceTiledExamples.h
  IlmImfExamples/drawImage.h
  IlmImfExamples/rgbaInterfaceExamples.h
  IlmImfExamples/generalInterfaceExamples.h
  IlmImfExamples/rgbaInterfaceTiledExamples.h
  IlmImfExamples/lowLevelIoExamples.h
  IlmImfExamples/previewImageExamples.h
  IlmImfExamples/namespaceAlias.h
  DESTINATION
  ${CMAKE_INSTALL_PREFIX}/share/doc/OpenEXR-${OPENEXR_VERSION}/examples
  )
