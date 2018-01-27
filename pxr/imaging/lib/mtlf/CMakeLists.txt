set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE mtlf)

set(optionalPublicClasses "")

set(optionalLibs "")
set(optionalIncludeDirs "")
if (PXR_ENABLE_PTEX_SUPPORT)
    list(APPEND optionalPublicClasses ptexMipmapTextureLoader)
    list(APPEND optionalLibs ${PTEX_LIBRARY})
    list(APPEND optionalIncludeDirs ${PTEX_INCLUDE_DIR})
endif()

pxr_library(mtlf
    LIBRARIES
        arch
        garch
        gf
        js
        plug
        tf
        tracelite
        sdf
        ${Boost_PYTHON_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${OPENGL_gl_LIBRARY}
        ${OPENGL_glu_LIBRARY}
        ${OIIO_LIBRARIES}
        ${GLEW_LIBRARY}
        ${X11_LIBRARIES}
        ${OPENEXR_LIBRARY}
        ${optionalLibs}

    INCLUDE_DIRS
        ${OIIO_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS}
        ${GLEW_INCLUDE_DIR}
        ${OPENEXR_INCLUDE_DIRS}
        ${optionalIncludeDirs}

    PUBLIC_CLASSES
        arrayTexture
        baseTexture
        bindingMap
        diagnostic
        drawTarget
        glslfx
        info
        mtlDevice
        package
        ptexTexture
        simpleLightingContext
        simpleShadowArray
        uniformBlock
        utils
        ${optionalPublicClasses}

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
        shaders/ptexTexture.glslfx
        shaders/pcfShader.glslfx
        shaders/simpleLighting.glslfx
        shaders/simpleShadowMapShader.glslfx
        shaders/defaults.metal
        shaders/interopFrag.glsl
        shaders/interopVtx.glsl
)
