set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE hgiMetal)

if (NOT ${PXR_ENABLE_METAL_SUPPORT})
    message(STATUS
        "Skipping ${PXR_PACKAGE} because PXR_ENABLE_METAL_SUPPORT is OFF")
    return()
endif()

set(optionalLibs "")
if (APPLEIOS)
    list(APPEND optionalLibs      ${UIKIT_LIBRARY})
else()
    list(APPEND optionalLibs      ${APPKIT_LIBRARY})
endif()

pxr_library(hgiMetal
    LIBRARIES
        arch
        hgi
        tf
        trace
        ${METAL_LIBRARY}
        ${optionalLibs}

    PRIVATE_CLASSES
        conversions
        diagnostic

    PUBLIC_CLASSES
        blitEncoder
        buffer
        graphicsEncoder
        hgi
        immediateCommandBuffer
        shaderFunction
        shaderProgram
        texture

    PUBLIC_HEADERS
        api.h

    RESOURCE_FILES
        plugInfo.json
)
