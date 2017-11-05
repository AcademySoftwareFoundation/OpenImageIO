# Attempt to find the desired version, but fall back to other
# additional versions.
find_package (PythonInterp ${PYTHON_VERSION} REQUIRED)

# The version that was found may not be the default or user
# defined one.
set (PYTHON_VERSION_FOUND ${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR})

if (NOT ${PYTHON_VERSION} EQUAL ${PYTHON_VERSION_FOUND} )
    message (WARNING "The requested version ${PYTHON_VERSION} was not found.") 
    message (WARNING "Using ${PYTHON_VERSION_FOUND} instead.")
endif ()

find_package (PythonLibs ${PYTHON_VERSION_FOUND} REQUIRED)

if (NOT DEFINED PYTHON_SITE_DIR)
    set (PYTHON_SITE_DIR "${CMAKE_INSTALL_LIBDIR}/python${PYTHON_VERSION_FOUND}/site-packages")
endif ()


set (target_name PyOpenImageIO)

set (python_srcs py_imageinput.cpp py_imageoutput.cpp
     py_imagecache.cpp py_imagespec.cpp py_roi.cpp
     py_imagebuf.cpp py_imagebufalgo.cpp
     py_typedesc.cpp py_paramvalue.cpp py_deepdata.cpp
     py_oiio.cpp)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    # Seems to be a problem on some systems, with pybind11 and python headers
    set_property (SOURCE ${python_srcs} APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-macro-redefined ")
endif ()

if (VERBOSE)
    message (STATUS "Python found ${PYTHONLIBS_FOUND} ")
    message (STATUS "Python include dirs ${PYTHON_INCLUDE_PATH}")
    message (STATUS "Python libraries    ${PYTHON_LIBRARIES}")
    message (STATUS "Python site packages dir ${PYTHON_SITE_DIR}")
    message (STATUS "Python to include 'lib' prefix: ${PYLIB_LIB_PREFIX}")
    message (STATUS "Python to include SO version: ${PYLIB_INCLUDE_SONAME}")
    message (STATUS "Python version ${PYTHON_VERSION_STRING}")
    message (STATUS "Python version major: ${PYTHON_VERSION_MAJOR} minor: ${PYTHON_VERSION_MINOR}")
    message (STATUS "pybind11 headers ${PYBIND11_INCLUDE_DIR}")
endif ()

include_directories (${PYTHON_INCLUDE_PATH} ${PYBIND11_INCLUDE_DIR})

add_library (${target_name} MODULE ${python_srcs})
if (APPLE)
   target_link_libraries (${target_name} OpenImageIO ${Boost_LIBRARIES} ${CMAKE_DL_LIBS})
   set_target_properties (${target_name} PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
else ()
   target_link_libraries (${target_name} OpenImageIO ${SANITIZE_LIBRARIES} ${Boost_LIBRARIES} ${PYTHON_LIBRARIES} ${CMAKE_DL_LIBS})
endif ()

    # Exclude the 'lib' prefix from the name
if(NOT PYLIB_LIB_PREFIX)
    add_definitions("-DOIIO_PYMODULE_NAME=OpenImageIO")
    set_target_properties (${target_name} PROPERTIES
                             OUTPUT_NAME OpenImageIO
                             PREFIX "")
else()
    add_definitions("-DOIIO_PYMODULE_NAME=PyOpenImageIO")
    set_target_properties (${target_name} PROPERTIES
                             OUTPUT_NAME PyOpenImageIO
                             PREFIX lib)
endif ()

if(PYLIB_INCLUDE_SONAME)
    if (VERBOSE)
        message(STATUS "Setting PyOIIO SOVERSION to: ${SOVERSION}")
    endif ()
    set_target_properties(${target_name} PROPERTIES
        VERSION ${OIIO_VERSION_MAJOR}.${OIIO_VERSION_MINOR}
        SOVERSION ${SOVERSION}
    )
endif()

if (WIN32)
    set_target_properties (${target_name} PROPERTIES
                           DEBUG_POSTFIX "_d"
                           SUFFIX ".pyd")
endif()

install (TARGETS ${target_name}
         RUNTIME DESTINATION ${PYTHON_SITE_DIR} COMPONENT user
         LIBRARY DESTINATION ${PYTHON_SITE_DIR} COMPONENT user)
