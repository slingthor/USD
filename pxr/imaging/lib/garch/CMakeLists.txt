set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE garch)

if(APPLE)
    set(GARCH_GLPLATFORMCONTEXT       glPlatformContextDarwin)
    set(GARCH_GLPLATFORMDEBUGWINDOW   glPlatformDebugWindowDarwin)
    set(GARCH_SOURCE_EXTENSION        mm)
    FIND_LIBRARY(APPKIT_LIBRARY AppKit)
    FIND_LIBRARY(METAL_LIBRARY Metal)
    FIND_LIBRARY(COREVIDEO_LIBRARY CoreVideo)
elseif(X11_FOUND)
    set(GARCH_GLPLATFORMCONTEXT       glPlatformContextGLX)
    set(GARCH_GLPLATFORMDEBUGWINDOW   glPlatformDebugWindowGLX)
    set(GARCH_SOURCE_EXTENSION        cpp)
elseif(WIN32)
    set(GARCH_GLPLATFORMCONTEXT       glPlatformContextWindows)
    set(GARCH_GLPLATFORMDEBUGWINDOW   glPlatformDebugWindowWindows)
    set(GARCH_SOURCE_EXTENSION        cpp)
endif()

pxr_library(garch
    LIBRARIES
        arch
        js
        plug
        sdf
        tf
        vt
        ${Boost_PYTHON_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${OPENGL_gl_LIBRARY}
        ${OIIO_LIBRARIES}
        ${APPKIT_LIBRARY}
        ${METAL_LIBRARY}
        ${COREVIDEO_LIBRARY}

    INCLUDE_DIRS
        ${Boost_INCLUDE_DIRS}
        ${OIIO_INCLUDE_DIRS}

    PUBLIC_CLASSES
        arrayTexture
        bindingMap
        baseTexture
        baseTextureData
        contextCaps
        drawTarget
        image
        imageRegistry
        glDebugWindow
        glPlatformDebugContext
        glslfx
        glslfxConfig
        resourceFactory
        texture
        textureHandle
        textureRegistry
        simpleLight
        simpleLightingContext
        simpleMaterial
        simpleShadowArray
        uniformBlock
        utils
        uvTexture
        uvTextureData
        uvTextureStorage
        uvTextureStorageData

    PRIVATE_CLASSES
        debugCodes

    PUBLIC_HEADERS
        api.h
        gl.h
        glPlatformContext.h
        ${GARCH_GLPLATFORMCONTEXT}.h

    PRIVATE_HEADERS
        rankedTypeMap.h
        ${GARCH_GLPLATFORMDEBUGWINDOW}.h

    PYTHON_CPPFILES
        moduleDeps.cpp

    CPPFILES
        ${GARCH_GLPLATFORMCONTEXT}.${GARCH_SOURCE_EXTENSION}
        ${GARCH_GLPLATFORMDEBUGWINDOW}.${GARCH_SOURCE_EXTENSION}
        dictionary.cpp
        oiioImage.cpp

    PYMODULE_CPPFILES
        module.cpp
        wrapBaseTexture.cpp
        wrapDrawTarget.cpp
        wrapPlatformDebugContext.cpp
        wrapSimpleLight.cpp
        wrapSimpleMaterial.cpp
        wrapTexture.cpp
        wrapTextureRegistry.cpp
        wrapUVTexture.cpp
        wrapUVTextureStorage.cpp

    PYMODULE_FILES
        __init__.py

    RESOURCE_FILES
        plugInfo.json
)
