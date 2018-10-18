set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE garch)

if (NOT ${PXR_ENABLE_GL_SUPPORT})
    message(STATUS
        "Skipping ${PXR_PACKAGE} because PXR_ENABLE_GL_SUPPORT is OFF")
    return()
endif()

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

if (OPENSUBDIV_HAS_GLSL_COMPUTE)
    add_definitions(-DOPENSUBDIV_HAS_GLSL_COMPUTE)
endif()
if (OPENSUBDIV_HAS_METAL_COMPUTE)
    add_definitions(-DOPENSUBDIV_HAS_METAL_COMPUTE)
endif()

set (optionalPublicClasses "")
set (optionalCppFiles "")
set (plugInfo plugInfo_NoOIIO.json)
if (PXR_ENABLE_PTEX_SUPPORT)
    list(APPEND optionalPublicClasses ptexMipmapTextureLoader)
    list(APPEND optionalLibs ${PTEX_LIBRARY})
    list(APPEND optionalIncludeDirs ${PTEX_INCLUDE_DIR})
endif()
if (PXR_BUILD_OPENIMAGEIO_PLUGIN)
    set (plugInfo plugInfo.json)
    list(APPEND optionalCppFiles oiioImage.cpp)
    list(APPEND optionalLibs ${OIIO_LIBRARIES})
    list(APPEND optionalIncludeDirs ${OIIO_INCLUDE_DIRS})
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
        ${APPKIT_LIBRARY}
        ${METAL_LIBRARY}
        ${COREVIDEO_LIBRARY}
        ${optionalLibs}

    INCLUDE_DIRS
        ${Boost_INCLUDE_DIRS}
        ${optionalIncludeDirs}

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
        ptexTexture
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
        ${optionalPublicClasses}

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
        stb/stb_image.h
        stb/stb_image_resize.h
        stb/stb_image_write.h

    PYTHON_CPPFILES
        moduleDeps.cpp

    CPPFILES
        ${GARCH_GLPLATFORMCONTEXT}.${GARCH_SOURCE_EXTENSION}
        ${GARCH_GLPLATFORMDEBUGWINDOW}.${GARCH_SOURCE_EXTENSION}
        dictionary.cpp
        ${optionalCppFiles}
        stbImage.cpp

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
        ${plugInfo}:plugInfo.json
)
