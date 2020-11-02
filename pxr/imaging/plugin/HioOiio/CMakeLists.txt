set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE HioOiio)

if (NOT ${PXR_BUILD_GPU_SUPPORT})
    message(STATUS
            "Skipping ${PXR_PACKAGE} because PXR_BUILD_GPU_SUPPORT is OFF")
    return()
endif()

pxr_plugin(HioOiio
    LIBRARIES
        ar
        arch
        gf
        hio
        tf
        ${OIIO_LIBRARIES}
        ${OPENEXR_LIBRARY}

    INCLUDE_DIRS
        ${OIIO_INCLUDE_DIRS}
        ${OPENEXR_INCLUDE_DIRS}

    CPPFILES
        oiioImage.cpp

    RESOURCE_FILES
        plugInfo.json
)
