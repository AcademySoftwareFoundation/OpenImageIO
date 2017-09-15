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

if (NOT BOOST_CUSTOM)
    # Finding the python component for boost is a little tricky, since it has
    # different names on different systems. Try the most common ones.
    foreach (_py_lib python-${PYTHON_VERSION_FOUND} python
             python${PYTHON_VERSION_MAJOR}
             python-py${PYTHON_VERSION_MAJOR}${PYTHON_VERSION_MINOR})
        find_package (Boost QUIET COMPONENTS ${_py_lib})
        string (TOUPPER ${_py_lib} _py_lib_name)
        if (Boost_${_py_lib_name}_FOUND)
            # Not the most beautiful thing to do, but that gets them included in
            # the target_link_libraries(…) call farther down
            set (Boost_PYTHON_LIBRARIES ${Boost_${_py_lib_name}_LIBRARIES})
            break ()
        endif ()
    endforeach ()
endif ()

if (APPLE)
#    set (PYTHON_LIBRARIES /opt/local/lib)
endif ()

if (NOT DEFINED PYTHON_SITE_DIR)
    set (PYTHON_SITE_DIR "${CMAKE_INSTALL_LIBDIR}/python${PYTHON_VERSION_FOUND}/site-packages")
endif ()

# Disable some warnings for Clang, it's a little too picky with boost
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_definitions ("-Wno-array-bounds")
endif ()

set (target_name PyOpenImageIO)

# Test if automatically found or manually set with BOOST_CUSTOM
if (DEFINED Boost_PYTHON_LIBRARIES)

    set (python_srcs py_imageinput.cpp py_imageoutput.cpp
         py_imagecache.cpp py_imagespec.cpp py_roi.cpp
         py_imagebuf.cpp py_imagebufalgo.cpp
         py_typedesc.cpp py_paramvalue.cpp py_deepdata.cpp
         py_oiio.cpp)

    if (VERBOSE)
        message (STATUS "Python found ${PYTHONLIBS_FOUND} ")
        message (STATUS "Python include dirs ${PYTHON_INCLUDE_PATH}")
        message (STATUS "Python libraries    ${PYTHON_LIBRARIES}")
        message (STATUS "Python site packages dir ${PYTHON_SITE_DIR}")
        message (STATUS "Python to include 'lib' prefix: ${PYLIB_LIB_PREFIX}")
        message (STATUS "Python to include SO version: ${PYLIB_INCLUDE_SONAME}")
        message (STATUS "Python version ${PYTHON_VERSION_STRING}")
        message (STATUS "Python version major: ${PYTHON_VERSION_MAJOR} minor: ${PYTHON_VERSION_MINOR}")
        message (STATUS "Boost python libraries ${Boost_PYTHON_LIBRARIES}")
    endif ()

    include_directories (${PYTHON_INCLUDE_PATH} ${Boost_INCLUDE_DIRS})
    add_library (${target_name} MODULE ${python_srcs})
    if (APPLE)
        target_link_libraries (${target_name} OpenImageIO ${Boost_LIBRARIES} ${Boost_PYTHON_LIBRARIES} ${CMAKE_DL_LIBS})
        set_target_properties (${target_name} PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
    else ()
        target_link_libraries (${target_name} OpenImageIO ${SANITIZE_LIBRARIES} ${Boost_LIBRARIES} ${Boost_PYTHON_LIBRARIES} ${PYTHON_LIBRARIES} ${CMAKE_DL_LIBS})
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
else ()
    # If Boost python components were not found, turn off all python support.
    message (STATUS "Boost python support not found!")
    if (APPLE AND USE_PYTHON)
        message (STATUS "   If your Boost is from Macports, you need the +python26 variant to get Python support.")
     endif ()
     if (BOOST_CUSTOM)
        message (STATUS "   Please set the variable Boost_PYTHON_LIBRARIES to the location of the boost python libraries.")
     endif ()
     message (FATAL_ERROR "Python module cannot be built. Either disable python support or check your boost installation.")
endif ()
