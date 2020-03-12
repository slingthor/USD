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

# Compile cpp files as obj-c++ to allow inline use of Metal API
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -x objective-c++")

pxr_library(hgiMetal
    LIBRARIES
        arch
        hgi
        tf
        trace
        ${METAL_LIBRARY}
        ${APPUIKIT_LIBRARY}

    PRIVATE_CLASSES
        conversions
        diagnostic 

    PUBLIC_CLASSES
        blitEncoder
        buffer
        graphicsEncoder
        hgi
        immediateCommandBuffer
        pipeline
        resourceBindings
        shaderFunction
        shaderProgram
        texture

    PUBLIC_HEADERS
        api.h

    RESOURCE_FILES
        plugInfo.json
)
