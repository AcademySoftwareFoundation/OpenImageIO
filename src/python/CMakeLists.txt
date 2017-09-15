#if the CMAKE_CURRENT_BINARY_DIR is python3, then build the python3 module,
#otherwise the python2 module
string (REGEX MATCH "python3\$" _py3_subdir ${CMAKE_CURRENT_BINARY_DIR})

if (_py3_subdir)
    set (BUILD_PY3 ON)
else ()
    set (BUILD_PY3 OFF)
endif ()

if (CMAKE_GENTOO_BUILD OR CMAKE_BUILD_TYPE STREQUAL Gentoo)
    # Gentoo's patches to CMake make sure that we will always find exactly the version of Python required by Gentoo
    if (BUILD_PY3)
        find_package (PythonInterp ${PYTHON3_VERSION} REQUIRED)
        find_package (PythonLibs ${PYTHON3_VERSION} REQUIRED)
    endif ()
    find_package (Boost 1.53 REQUIRED COMPONENTS python)
elseif (NOT BOOST_CUSTOM AND NOT BUILD_PY3)
    #Unset those, otherwise find_package(PythonLibs) will pick up old stuff
    #if it has been run before
    unset(Python_ADDITIONAL_VERSIONS)
    unset(PYTHON_LIBRARY)
    unset(PYTHON_LIBRARY CACHE)
    unset(PYTHON_INCLUDE_DIR)
    unset(PYTHON_INCLUDE_DIR CACHE)
    unset(PYTHON_INCLUDE_PATH)
    unset(PYTHON_INCLUDE_PATH CACHE)
    find_package (PythonLibs ${PYTHON_VERSION} REQUIRED)
    find_package (Boost 1.53 REQUIRED COMPONENTS python)
elseif (BOOST_CUSTOM AND NOT BUILD_PY3)
    find_package (PythonLibs ${PYTHON_VERSION} REQUIRED)
else ()
    #BOOST_CUSTOM is ignored for python3

    #Unset those, otherwise find_package(PythonLibs) will pick up old stuff
    #if it has been run before
    unset(PYTHON_LIBRARY)
    unset(PYTHON_LIBRARY CACHE)
    unset(PYTHON_INCLUDE_DIR)
    unset(PYTHON_INCLUDE_DIR CACHE)
    unset(PYTHON_INCLUDE_PATH)
    unset(PYTHON_INCLUDE_PATH CACHE)

    #cmake 2.8 does not look for python 3.4
    set(Python_ADDITIONAL_VERSIONS 3.4)
    find_package (PythonInterp ${PYTHON3_VERSION} REQUIRED)
    find_package (PythonLibs ${PYTHON3_VERSION} REQUIRED)

    #Finding the python3 component for boost is a little tricky, since it has
    #different names on different systems. Try the most common ones
    #(boost_python3, boost_python-py34, …).
    foreach (_boost_py3_lib python3 python-py${PYTHON_VERSION_MAJOR}${PYTHON_VERSION_MINOR})
        find_package (Boost 1.53 QUIET COMPONENTS ${_boost_py3_lib})
        string (TOUPPER ${_boost_py3_lib} boost_py3_lib_name)
        if (Boost_${boost_py3_lib_name}_FOUND)
            #Not the most beautiful thing to do, but that gets them included in
            #the target_link_libraries(…) call farther down
            set (Boost_PYTHON_LIBRARIES ${Boost_${boost_py3_lib_name}_LIBRARIES})
            break ()
        endif ()
    endforeach ()
endif ()

if (APPLE)
#    set (PYTHON_LIBRARIES /opt/local/lib)
endif ()

# Disable some warnings for Clang, it's a little too picky with boost
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_definitions ("-Wno-array-bounds")
endif ()

if (BUILD_PY3)
    set (target_name Py3OpenImageIO)
else ()
    set (target_name PyOpenImageIO)
endif ()

if (BOOST_CUSTOM OR Boost_FOUND AND PYTHONLIBS_FOUND)

    set (python_srcs py_imageinput.cpp py_imageoutput.cpp
         py_imagecache.cpp py_imagespec.cpp py_roi.cpp
         py_imagebuf.cpp py_imagebufalgo.cpp
         py_typedesc.cpp py_paramvalue.cpp py_deepdata.cpp
         py_oiio.cpp)

    string (REGEX MATCH "([0-9]+)\.([0-9]+)\.[0-9]+" _version_match ${PYTHONLIBS_VERSION_STRING})
    set (PYTHONLIBS_VERSION_MAJOR ${CMAKE_MATCH_1})
    set (PYTHONLIBS_VERSION_MINOR ${CMAKE_MATCH_2})
    if (VERBOSE)
        message (STATUS "Python found ${PYTHONLIBS_FOUND} ")
        message (STATUS "Python include dirs ${PYTHON_INCLUDE_PATH}")
        message (STATUS "Python libraries    ${PYTHON_LIBRARIES}")
        message (STATUS "Python to include 'lib' prefix: ${PYLIB_LIB_PREFIX}")
        message (STATUS "Python to include SO version: ${PYLIB_INCLUDE_SONAME}")
        message (STATUS "PYTHONLIBS_VERSION_STRING = ${PYTHONLIBS_VERSION_STRING}")
        message (STATUS "Python version major: ${PYTHONLIBS_VERSION_MAJOR} minor: ${PYTHONLIBS_VERSION_MINOR}")
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
    
    if (BUILD_PY3)
        install (TARGETS ${target_name}
                 RUNTIME DESTINATION ${PYLIB3_INSTALL_DIR} COMPONENT user
                 LIBRARY DESTINATION ${PYLIB3_INSTALL_DIR} COMPONENT user)
    else ()
        install (TARGETS ${target_name}
                 RUNTIME DESTINATION ${PYLIB_INSTALL_DIR} COMPONENT user
                 LIBRARY DESTINATION ${PYLIB_INSTALL_DIR} COMPONENT user)
    endif ()
elseif (BUILD_PY3)
    if (NOT PYTHONLIBS_FOUND)
        message (STATUS "Python3 libraries not found")
    endif ()
    if (NOT Boost_FOUND)
        message (STATUS "Boost python3 component not found")
    endif ()
    set(USE_PYTHON3 OFF)
endif ()
