set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE mtlf)

if (NOT ${PXR_ENABLE_METAL_SUPPORT})
    message(STATUS
        "Skipping ${PXR_PACKAGE} because PXR_ENABLE_METAL_SUPPORT is OFF")
    return()
endif()

if (OPENSUBDIV_HAS_METAL_COMPUTE)
    add_definitions(-DOPENSUBDIV_HAS_METAL_COMPUTE)
endif()

pxr_library(mtlf
    LIBRARIES
        arch
        garch
        glf
        gf
        glf
        hgiMetal
        js
        plug
        tf
        trace
        sdf
        ${Boost_PYTHON_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${OIIO_LIBRARIES}
        ${X11_LIBRARIES}
        ${OPENEXR_LIBRARY}

    INCLUDE_DIRS
        ${OIIO_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS}
        ${GLEW_INCLUDE_DIR}
        ${OPENEXR_INCLUDE_DIRS}

    PUBLIC_CLASSES
        baseTexture
        bindingMap
        contextCaps
        diagnostic
        drawTarget
        info
        OSDMetalContext
        mtlDevice
        package
        resourceFactory
        simpleLightingContext
        simpleShadowArray
        uniformBlock
        utils

    PRIVATE_CLASSES
        debugCodes

    PUBLIC_HEADERS
        api.h

    PYTHON_CPPFILES
        moduleDeps.cpp

    PYMODULE_CPPFILES
        module.cpp
        wrapDiagnostic.cpp

    PYMODULE_FILES
        __init__.py

    RESOURCE_FILES
        plugInfo.json
        shaders/pcfShader.glslfx
        shaders/simpleLighting.glslfx
        shaders/defaults.metal
        shaders/interopFrag.glsl
        shaders/interopVtx.glsl
)
