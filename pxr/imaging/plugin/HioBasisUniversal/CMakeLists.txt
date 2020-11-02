set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE HioBasisUniversal)

if (NOT ${PXR_BUILD_GPU_SUPPORT})
    message(STATUS
            "Skipping ${PXR_PACKAGE} because PXR_BUILD_GPU_SUPPORT is OFF")
    return()
endif()

pxr_plugin(HioBasisUniversal
    LIBRARIES
        ar
        arch
        gf
        hio
        tf
        ${BASISU_LIBRARY}

    INCLUDE_DIRS
        ${BASISU_INCLUDE_DIR}

    PUBLIC_CLASSES
        basisUniversalImageManager

    CPPFILES
        basisUniversalImage.cpp

    RESOURCE_FILES
        plugInfo.json
)
