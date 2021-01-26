set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE glf)

set(optionalPublicClasses "")
if (X11_FOUND)
    list(APPEND optionalPublicClasses testGLContext)
endif()

set(optionalLibs "")
set(optionalIncludeDirs "")

if (NOT ${PXR_ENABLE_GL_SUPPORT})
    message(STATUS
        "Building minimal ${PXR_PACKAGE} because PXR_ENABLE_GL_SUPPORT is OFF")

    set(OPENGL_LIBRARY  "")
if (NOT APPLEIOS)
    set(OPENGL_LIBRARY              ${OPENGL_gl_LIBRARY}
                                    ${OPENGL_glu_LIBRARY})
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

        PUBLIC_CLASSES
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
            ${X11_LIBRARIES}
            ${optionalLibs}

        INCLUDE_DIRS
            ${Boost_INCLUDE_DIRS}
            ${optionalIncludeDirs}

        PUBLIC_CLASSES
            baseTexture
            bindingMap
            contextCaps
            diagnostic
            drawTarget
            glContext
            glRawContext
            info
            resourceFactory
            simpleLightingContext
            simpleShadowArray
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

        PYMODULE_FILES
            __init__.py

        RESOURCE_FILES
            plugInfo.json
            shaders/pcfShader.glslfx
            shaders/simpleLighting.glslfx

        DOXYGEN_FILES
            overview.dox
    )
endif()
