set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE glfBasisUniversal)

if (NOT ${PXR_BUILD_GPU_SUPPORT})
    message(STATUS
            "Skipping ${PXR_PACKAGE} because PXR_BUILD_GPU_SUPPORT is OFF")
    return()
endif()

set(BASISU_SRC_DIR "${CMAKE_SOURCE_DIR}/third_party/basis_universal")
file (GLOB basisu_inc_files ${BASISU_SRC_DIR}/*)
pxr_plugin(glfBasisUniversal
    LIBRARIES
        ar
        arch
        gf
        glf
        tf

    INCLUDE_DIRS
        ${BASISU_SRC_DIR}
        ${BASISU_SRC_DIR}/transcoder

    PUBLIC_CLASSES
        basisUniversalImageManager

    CPPFILES
        basisUniversalImage.cpp
        ${BASISU_SRC_DIR}/transcoder/basisu_transcoder.cpp
        ${BASISU_SRC_DIR}/transcoder/basisu_transcoder.h
        ${basisu_inc_files}

    RESOURCE_FILES
        plugInfo.json
)
