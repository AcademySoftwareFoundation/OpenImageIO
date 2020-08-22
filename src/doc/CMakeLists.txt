# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

project(documentation)

set (public_docs
     "${OpenImageIO_SOURCE_DIR}/LICENSE.md"
     "${OpenImageIO_SOURCE_DIR}/THIRD-PARTY.md"
     "${OpenImageIO_SOURCE_DIR}/CHANGES.md"
     CHANGES-0.x.md
     CHANGES-1.x.md
)

if (INSTALL_DOCS)
    install (FILES ${public_docs}
             DESTINATION ${CMAKE_INSTALL_DOCDIR}
             COMPONENT documentation)
endif()

# generate man pages using txt2man and a tiny python script to munge the
# result of "$tool --help"
find_program(TXT2MAN txt2man)
if (NOT PYTHONINTERP_FOUND)
find_package(PythonInterp)
endif ()
if (UNIX AND TXT2MAN AND PYTHONINTERP_FOUND)
    message (STATUS "Unix man page documentation will be generated")
    set (cli_tools oiiotool iinfo maketx idiff igrep iconvert)

    if (TARGET iv)
        list (APPEND cli_tools iv)
    endif()

    foreach (tool ${cli_tools})
        set (outfile "${documentation_BINARY_DIR}/${tool}.1")
        list (APPEND manpage_files ${outfile})
        add_custom_command (OUTPUT ${outfile}
            COMMAND ${tool} --help |
            ${PYTHON_EXECUTABLE} "${documentation_SOURCE_DIR}/help2man_preformat.py" |
            ${TXT2MAN} -v OpenImageIO -s 1 -t ${tool} > ${outfile}
            DEPENDS ${tool} help2man_preformat.py)
    endforeach()

    # force man page build before install
    add_custom_target (man_pages ALL DEPENDS ${manpage_files})

    if (INSTALL_DOCS)
        install (FILES ${manpage_files}
                 DESTINATION ${CMAKE_INSTALL_MANDIR}
                 COMPONENT documentation)
    endif()
endif()
