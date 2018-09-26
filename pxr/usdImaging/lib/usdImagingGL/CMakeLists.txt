set(PXR_PREFIX pxr/usdImaging)
set(PXR_PACKAGE usdImagingGL)

if (NOT ${PXR_ENABLE_GL_SUPPORT})
    message(STATUS
        "Skipping ${PXR_PACKAGE} because PXR_ENABLE_GL_SUPPORT is OFF")
    return()
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
        glf
        hd
        pxOsd
        sdf
        usd
        usdGeom
        usdShade
        usdImaging
        ar
        ${Boost_PYTHON_LIBRARY}
        ${PYTHON_LIBRARY}
        ${OPENGL_gl_LIBRARY}
        ${OPENGL_glu_LIBRARY}
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
        hdEngine
        refEngine

    PUBLIC_HEADERS
        api.h
        version.h

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
        CPPFILES
            unitTestGLDrawing.cpp
            testenv/testUsdImagingGLBasicDrawing.cpp
    )
endif()
