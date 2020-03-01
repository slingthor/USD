set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE hgiMetal)

if (NOT ${PXR_ENABLE_METAL_SUPPORT})
    message(STATUS
        "Skipping ${PXR_PACKAGE} because PXR_ENABLE_METAL_SUPPORT is OFF")
    return()
endif()

FIND_LIBRARY(METAL_LIBRARY Metal)
FIND_LIBRARY(APPKIT_LIBRARY AppKit)

# Compile cpp files as obj-c++ to allow inline use of Metal API
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -x objective-c++")

pxr_library(hgiMetal
    LIBRARIES
        arch
        hgi
        tf
        trace
        ${METAL_LIBRARY}
        ${APPKIT_LIBRARY}

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
