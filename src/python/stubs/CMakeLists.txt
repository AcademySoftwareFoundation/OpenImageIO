
# Setup the pystub target, which is not built by default.

set (_stub_file "${CMAKE_SOURCE_DIR}/src/python/stubs/OpenImageIO/__init__.pyi")

# Note: the python version must be kept in sync with `[[tool.cibuildwheel.overrides]]` in pyproject.toml.
# The stubs are generated within a container so the version of python does not need to match
# the version of python that OpenImageIO is being built against.
# Note: the version of cibuildwheel should be kept in sync with .github/workflows/wheel.yml
add_custom_command (COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/src/python/stubs/generate_stubs_local.py
                        --repo-root ${CMAKE_SOURCE_DIR} --python-version="3.11" --cibuildwheel-version="2.21.1"
                        --output-dir "${CMAKE_BINARY_DIR}/wheelhouse"
                    OUTPUT "${CMAKE_BINARY_DIR}/wheelhouse/OpenImageIO/__init__.pyi"
                    DEPENDS "${CMAKE_SOURCE_DIR}/src/python/stubs/generate_stubs.py"
                    DEPENDS "${CMAKE_SOURCE_DIR}/src/python/stubs/generate_stubs_local.py"
                    COMMENT "pystubs: Generating python stubs"
                    )

add_custom_command (COMMAND ${CMAKE_COMMAND} -E copy
                        "${CMAKE_BINARY_DIR}/wheelhouse/OpenImageIO/__init__.pyi"
                        ${_stub_file}
                    OUTPUT ${_stub_file}
                    DEPENDS "${CMAKE_BINARY_DIR}/wheelhouse/OpenImageIO/__init__.pyi"
                    COMMENT "pystubs: Copying generated stubs to source"
                    )

add_custom_target (pystubs DEPENDS ${_stub_file})
