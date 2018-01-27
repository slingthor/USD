set(PXR_PREFIX pxr/usdImaging)
set(PXR_PACKAGE usdImagingMetal)

# Note: both python include directives are included for compatibility.
pxr_library(usdImagingMetal
    LIBRARIES
        gf
        tf
        plug
        tracelite
        vt
        work
        garch
        hd
        hdx
        mtlf
        pxOsd
        sdf
        usd
        usdGeom
        usdShade
        usdHydra
        usdImaging
        ar
        ${Boost_PYTHON_LIBRARY}
        ${PYTHON_LIBRARY}
        ${TBB_tbb_LIBRARY}

    INCLUDE_DIRS
        ${PYTHON_INCLUDE_PATH}
        ${PYTHON_INCLUDE_DIRS}
        ${TBB_INCLUDE_DIRS}

    PUBLIC_CLASSES
        engine
        metal
        hdEngine

    PUBLIC_HEADERS
        api.h
        version.h

    PRIVATE_HEADERS
        unitTestMetalDrawing.h

    PYTHON_CPPFILES
        moduleDeps.cpp

    PYMODULE_CPPFILES
        module.cpp
        wrapMetal.cpp

    PYMODULE_FILES
        __init__.py
)

if (X11_FOUND OR APPLE)
    pxr_build_test(testUsdImagingMetalBasicDrawing
        LIBRARIES
            hd
            garch
            mtlf
            usd
            usdUtils
            usdImaging
            usdImagingMetal
            sdf
        CPPFILES
            unitTestMetalDrawing.cpp
            testenv/testUsdImagingMetalBasicDrawing.cpp
    )
endif()
