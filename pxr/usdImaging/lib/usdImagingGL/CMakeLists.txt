set(PXR_PREFIX pxr/usdImaging)
set(PXR_PACKAGE usdImagingGL)

if ((NOT ${PXR_ENABLE_GL_SUPPORT}) AND (NOT ${PXR_ENABLE_METAL_SUPPORT}))
    message(STATUS
        "Skipping ${PXR_PACKAGE} because PXR_ENABLE_GL_SUPPORT and PXR_ENABLE_METAL_SUPPORT is OFF")
    return()
endif()

if (${PXR_ENABLE_METAL_SUPPORT})
    set(METAL_LIBRARY               mtlf)
    set(METAL_PUBLIC_CLASSES        Metal/hdEngineMetal)
endif()

if (${PXR_ENABLE_GL_SUPPORT})
    set(OPENGL_LIBRARY              glf
                                    ${OPENGL_gl_LIBRARY}
                                    ${OPENGL_glu_LIBRARY})
    set(OPENGL_PUBLIC_CLASSES       GL/hdEngineGL
                                    GL/refEngine)
else()
    set(OPENGL_INCLUDE_DIR          "")
endif()

# Note: both python include directives are included for compatibility.
pxr_library(usdImagingGL
    LIBRARIES
        gf
        tf
        plug
        trace
        vt
        work
        garch
        hd
        pxOsd
        sdf
        sdr
        usd
        usdGeom
        usdShade
        usdImaging
        ar
        ${Boost_PYTHON_LIBRARY}
        ${OPENGL_LIBRARY}
        ${METAL_LIBRARY}
        ${PYTHON_LIBRARY}
        ${GLEW_LIBRARY}
        ${TBB_tbb_LIBRARY}

    INCLUDE_DIRS
        ${PYTHON_INCLUDE_PATH}
        ${PYTHON_INCLUDE_DIRS}
        ${OPENGL_INCLUDE_DIR}
        ${GLEW_INCLUDE_DIR}
        ${TBB_INCLUDE_DIRS}

    PUBLIC_CLASSES
        engine
        gl
        renderParams
        ${OPENGL_PUBLIC_CLASSES}
        ${METAL_PUBLIC_CLASSES}

    PUBLIC_HEADERS
        api.h
        version.h
        rendererSettings.h

    PRIVATE_HEADERS
        unitTestGLDrawing.h

    PYTHON_CPPFILES
        moduleDeps.cpp

    PYMODULE_CPPFILES
        module.cpp
        wrapGL.cpp

    PYMODULE_FILES
        __init__.py

    RESOURCE_FILES
        plugInfo.json
)

if (X11_FOUND OR APPLE)
    pxr_build_test(testUsdImagingGLBasicDrawing
        LIBRARIES
            hd
            garch
            glf
            usd
            usdUtils
            usdImaging
            usdImagingGL
            sdf
            ${OPENGL_LIBRARY}
            ${METAL_LIBRARY}
        CPPFILES
            unitTestGLDrawing.cpp
            testenv/testUsdImagingGLBasicDrawing.cpp
    )
endif()
