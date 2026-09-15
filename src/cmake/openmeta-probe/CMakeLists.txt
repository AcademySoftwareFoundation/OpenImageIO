# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

cmake_minimum_required (VERSION 3.23)
project (OpenMetaProbe LANGUAGES CXX)
find_package (OpenMeta 0.5.0 CONFIG REQUIRED)
add_executable (openmeta_probe main.cpp)
set_target_properties (openmeta_probe PROPERTIES
                       CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_link_libraries (openmeta_probe PRIVATE OpenMeta::openmeta)
