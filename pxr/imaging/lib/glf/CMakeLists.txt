set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE glf)

set(optionalPublicClasses "")
if (X11_FOUND)
    list(APPEND optionalPublicClasses testGLContext)
endif()

set(optionalLibs "")
set(optionalIncludeDirs "")
if (PXR_ENABLE_PTEX_SUPPORT)
    list(APPEND optionalLibs ${PTEX_LIBRARY})
    list(APPEND optionalIncludeDirs ${PTEX_INCLUDE_DIR})
endif()

if (OPENSUBDIV_HAS_GLSL_COMPUTE)
    add_definitions(-DOPENSUBDIV_HAS_GLSL_COMPUTE)
endif()

if (NOT ${PXR_ENABLE_GL_SUPPORT})
    message(STATUS
        "Building minimal ${PXR_PACKAGE} because PXR_ENABLE_GL_SUPPORT is OFF")

    set(OPENGL_LIBRARY  "")
if (NOT APPLEIOS)
    set(OPENGL_LIBRARY              ${OPENGL_gl_LIBRARY}
                                    ${OPENGL_glu_LIBRARY}
                                    ${GLEW_LIBRARY})
endif()

    pxr_library(glf
        LIBRARIES
            ar
            arch
            garch
            gf
            hf
            js
            plug
            tf
            trace
            sdf
            ${OPENGL_LIBRARY}

        INCLUDE_DIRS
            ${Boost_INCLUDE_DIRS}
            ${GLEW_INCLUDE_DIR}

        PUBLIC_CLASSES
            glew
            glContext
            glRawContext
            info

        PRIVATE_CLASSES
            glContextRegistry

        PUBLIC_HEADERS
            api.h
            diagnostic.h

        PYTHON_CPPFILES
            moduleDeps.cpp

        PYMODULE_CPPFILES
            module.cpp
            wrapGlew.cpp

        PYMODULE_FILES
            __init__.py
)
else()
    pxr_library(glf
        LIBRARIES
            ar
            arch
            garch
            gf
            hf
            js
            plug
            tf
            trace
            sdf
            ${Boost_PYTHON_LIBRARY}
            ${OPENGL_gl_LIBRARY}
            ${OPENGL_glu_LIBRARY}
            ${GLEW_LIBRARY}
            ${X11_LIBRARIES}
            ${OPENEXR_LIBRARY}
            ${optionalLibs}

        INCLUDE_DIRS
            ${Boost_INCLUDE_DIRS}
            ${GLEW_INCLUDE_DIR}
            ${OPENEXR_INCLUDE_DIRS}
            ${optionalIncludeDirs}

        PUBLIC_CLASSES
            arrayTexture
            baseTexture
            bindingMap
            contextCaps
            diagnostic
            drawTarget
            glew
            glContext
            glRawContext
            info
            package
            ptexTexture
            resourceFactory
            simpleLightingContext
            simpleShadowArray
            udimTexture
            uniformBlock
            utils
            ${optionalPublicClasses}

        PRIVATE_CLASSES
            debugCodes
            glContextRegistry

        PUBLIC_HEADERS
            api.h

        PYTHON_CPPFILES
            moduleDeps.cpp

        PYMODULE_CPPFILES
            module.cpp
            wrapDiagnostic.cpp
            wrapGlew.cpp

        PYMODULE_FILES
            __init__.py

        RESOURCE_FILES
            plugInfo.json
            shaders/pcfShader.glslfx
            shaders/simpleLighting.glslfx
            shaders/simpleShadowMapShader.glslfx
    )
endif()
