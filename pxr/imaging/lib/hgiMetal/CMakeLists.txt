set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE hgiMetal)

if (NOT ${PXR_ENABLE_METAL_SUPPORT})
    message(STATUS
        "Skipping ${PXR_PACKAGE} because PXR_ENABLE_METAL_SUPPORT is OFF")
    return()
endif()

pxr_library(hgiMetal
    LIBRARIES
        arch
        hgi
        tf
        trace

    PRIVATE_CLASSES
        conversions
        diagnostic

    PUBLIC_CLASSES
        blitEncoder
        graphicsEncoder
        hgi
        immediateCommandBuffer
        texture

    PUBLIC_HEADERS
        api.h

)
