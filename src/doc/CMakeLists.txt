project(documentation)

set (public_docs
     openimageio.pdf
     ${OpenImageIO_SOURCE_DIR}/../LICENSE
     ${OpenImageIO_SOURCE_DIR}/../CHANGES
)

if (INSTALL_DOCS)
    install (FILES ${public_docs} DESTINATION ${DOC_INSTALL_DIR}
             COMPONENT documentation)
endif ()

# generate man pages using txt2man and a tiny python script to munge the
# result of "$tool --help"
find_program(TXT2MAN txt2man)
find_package(PythonInterp)
if (UNIX AND TXT2MAN AND PYTHONINTERP_FOUND)
    message (STATUS "Unix man page documentation will be generated")
    set (cli_tools oiiotool iinfo maketx idiff iv igrep iprocess iconvert)

    foreach (tool ${cli_tools})
        set (outfile "${documentation_BINARY_DIR}/${tool}.1")
        list (APPEND manpage_files ${outfile})
        add_custom_command (OUTPUT ${outfile}
            COMMAND ${tool} --help |
            ${PYTHON_EXECUTABLE} ${documentation_SOURCE_DIR}/help2man_preformat.py |
            ${TXT2MAN} -v OpenImageIO -s 1 -t ${tool} > ${outfile}
            DEPENDS ${tool} help2man_preformat.py)
    endforeach()

    # force man page build before install
    add_custom_target (man_pages ALL DEPENDS ${manpage_files})

    if (INSTALL_DOCS)
        install (FILES ${manpage_files}
                 DESTINATION ${MAN_INSTALL_DIR} COMPONENT documentation)
    endif ()
endif()
