set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE hgiMetal)

if (NOT ${PXR_BUILD_STORM_PLUGINS} OR NOT ${PXR_ENABLE_METAL_SUPPORT})
    message(STATUS
        "Skipping ${PXR_PACKAGE} because PXR_BUILD_STORM_PLUGINS or PXR_ENABLE_METAL_SUPPORT is OFF")
    return()
endif()

FIND_LIBRARY(METAL_LIBRARY Metal)
if (APPLEIOS)
FIND_LIBRARY(APPUIKIT_LIBRARY UIKit)
else()
FIND_LIBRARY(APPUIKIT_LIBRARY AppKit)
endif()

pxr_library(hgiMetal
    LIBRARIES
        arch
        hgi
        tf
        trace
        ${METAL_LIBRARY}
        ${APPUIKIT_LIBRARY}

    PUBLIC_HEADERS
        api.h
        capabilities.h
        diagnostic.h
        hgi.h
        texture.h

    PRIVATE_HEADERS
        blitCmds.h
        buffer.h
        conversions.h
        graphicsCmds.h
        pipeline.h
        resourceBindings.h
        shaderFunction.h
        shaderProgram.h

    CPPFILES
        blitCmds.mm
        buffer.mm
        capabilities.mm
        conversions.mm
        diagnostic.mm
        graphicsCmds.mm
        hgi.mm
        pipeline.mm
        resourceBindings.mm
        shaderFunction.mm
        shaderProgram.mm
        texture.mm

    RESOURCE_FILES
        plugInfo.json
)
