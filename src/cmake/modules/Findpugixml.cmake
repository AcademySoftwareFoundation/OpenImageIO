# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Find the pugixml XML parsing library.
#
# Sets the usual variables expected for find_package scripts:
#
# PUGIXML_INCLUDES - header location
# PUGIXML_LIBRARIES - library to link against
# PUGIXML_FOUND - true if pugixml was found.

# First try the config files and hope they're good enough
find_package (pugixml CONFIG)

if (TARGET pugixml::pugixml)
    # New pugixml (>= 1.11) has nice config files we can rely on and makes a
    # pugixml::pugixml target.
    message (VERBOSE "Found CONFIG for pugixml (>=1.11)")

    set (PUGIXML_FOUND true)
    set (pugixml_FOUND true)
    get_target_property(PUGIXML_INCLUDES pugixml::pugixml INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(PUGIXML_LIBRARIES pugixml::pugixml INTERFACE_LINK_LIBRARIES)

# elseif (TARGET pugixml)
#     # But older pugixml (<= 1.10) has mediocre config files that are not
#     # reliable in practice. They also only define a "pugixml" target but
#     # not "pugixml::pugixml". So just fall through to the "no config" case
#     # below, and figure everything out by hand.

else ()
    # If no config file is found for PugiXML, do it the old fashioned way.
    find_path (PUGIXML_INCLUDE_DIR
               NAMES pugixml.hpp
               PATH_SUFFIXES pugixml-1.8 pugixml-1.9 pugixml-1.10)
    set (pugixml_required_vars PUGIXML_INCLUDE_DIR)
    find_library (PUGIXML_LIBRARY
                  NAMES pugixml
                  PATH_SUFFIXES pugixml-1.8 pugixml-1.9 pugixml-1.10)

    if (PUGIXML_INCLUDE_DIR)
        file (STRINGS "${PUGIXML_INCLUDE_DIR}/pugixml.hpp" TMP REGEX "define PUGIXML_VERSION .*$")
        string (REGEX MATCHALL "[0-9]+" PUGIXML_CODED_VERSION ${TMP})
        if (PUGIXML_CODED_VERSION VERSION_GREATER_EQUAL 1000)
            math (EXPR PUGIXML_VERSION_MAJOR "${PUGIXML_CODED_VERSION} / 1000")
            math (EXPR PUGIXML_VERSION_MINOR "(${PUGIXML_CODED_VERSION} % 1000) / 10")
        else ()
            math (EXPR PUGIXML_VERSION_MAJOR "${PUGIXML_CODED_VERSION} / 100")
            math (EXPR PUGIXML_VERSION_MINOR "(${PUGIXML_CODED_VERSION} % 100) / 10")
        endif ()
        set (PUGIXML_VERSION ${PUGIXML_VERSION_MAJOR}.${PUGIXML_VERSION_MINOR})
    endif ()

    # Support the REQUIRED and QUIET arguments, and set PUGIXML_FOUND if found.
    include (FindPackageHandleStandardArgs)
    find_package_handle_standard_args (pugixml DEFAULT_MSG 
                                       PUGIXML_LIBRARY PUGIXML_INCLUDE_DIR)

    if (PUGIXML_FOUND OR pugixml_FOUND)
        set (PUGIXML_INCLUDES ${PUGIXML_INCLUDE_DIR})
        set (PUGIXML_LIBRARIES ${PUGIXML_LIBRARY})

        if (NOT TARGET pugixml::pugixml)
            add_library(pugixml::pugixml UNKNOWN IMPORTED)
            set_target_properties(pugixml::pugixml PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${PUGIXML_INCLUDES}")
            if (PUGIXML_LIBRARIES)
                set_property(TARGET pugixml::pugixml APPEND PROPERTY
                    IMPORTED_LOCATION "${PUGIXML_LIBRARIES}")
            endif ()
        endif ()
    endif ()

    mark_as_advanced (PUGIXML_LIBRARY PUGIXML_INCLUDE_DIR)

endif()
