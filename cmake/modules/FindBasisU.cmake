#
# Copyright 2020 Pixar
#
# Licensed under the Apache License, Version 2.0 (the "Apache License")
# with the following modification; you may not use this file except in
# compliance with the Apache License and the following modification to it:
# Section 6. Trademarks. is deleted and replaced with:
#
# 6. Trademarks. This License does not grant permission to use the trade
#    names, trademarks, service marks, or product names of the Licensor
#    and its affiliates, except as required to comply with Section 4(c) of
#    the License and to reproduce the content of the NOTICE file.
#
# You may obtain a copy of the Apache License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the Apache License with the above modification is
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the Apache License for the specific
# language governing permissions and limitations under the Apache License.
#
#=============================================================================
#
# The module defines the following variables:
#   BASISU_INCLUDE_DIR - path to basisu header directory
#   BASISU_LIBRARY     - path to basisu library file
#       BASISU_FOUND   - true if basisu was found
#
# Example usage:
#   find_package(BASISU)
#   if(BASISU_FOUND)
#     message("BasisU found: ${BASISU_LIBRARY}")
#   endif()
#
#=============================================================================

if (WIN32)
    set (EMBREE_LIB_NAME basisu.lib)
else()
    set (BASISU_LIB_NAME libbasisu.a)
endif()

find_library(BASISU_LIBRARY
        "${BASISU_LIB_NAME}"
    HINTS
        "${BASISU_LOCATION}/lib"
        "$ENV{BASISU_LOCATION}/lib"
    DOC
        "BasisU library path"
)

find_path(BASISU_INCLUDE_DIR
    transcoder/basisu.h
PATH_SUFFIXES
    basisu/include/
HINTS
    "${BASISU_LOCATION}"
    "$ENV{BASISU_LOCATION}"
DOC
    "BasisU headers path"
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(BasisU
    REQUIRED_VARS
        BASISU_INCLUDE_DIR
        BASISU_LIBRARY
)
