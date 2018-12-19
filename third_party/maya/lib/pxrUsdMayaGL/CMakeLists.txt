set(PXR_PACKAGE pxrUsdMayaGL)

if ((NOT ${PXR_ENABLE_GL_SUPPORT}) AND (NOT ${PXR_ENABLE_METAL_SUPPORT}))
    message(STATUS
        "Skipping ${PXR_PACKAGE} because PXR_ENABLE_GL_SUPPORT and PXR_ENABLE_METAL_SUPPORT is OFF")
    return()
endif()

if (APPLE)
    set(METAL_LIBRARY               mtlf)
    set(METAL_PUBLIC_CLASSES        Metal/batchRendererMetal)
endif()

pxr_shared_library(${PXR_PACKAGE}
    LIBRARIES
        arch
        gf
        glf
        hd
        hdSt
        hdx
        px_vp20
        sdf
        tf
        usd
        usdGeom
        usdImaging
        usdImagingGL
        usdMaya
        vt
        ${Boost_LIBRARIES}
        ${MAYA_LIBRARIES}
        ${METAL_LIBRARY}

    INCLUDE_DIRS
        ${Boost_INCLUDE_DIRS}
        ${MAYA_INCLUDE_DIRS}

    PUBLIC_HEADERS
        api.h
        renderParams.h

    PUBLIC_CLASSES
        batchRenderer
        debugCodes
        hdImagingShapeDrawOverride
        hdImagingShapeUI
        hdRenderer
        proxyDrawOverride
        proxyShapeUI
        sceneDelegate
        shapeAdapter
        softSelectHelper
        usdProxyShapeAdapter
        userData
        GL/batchRendererGL
        ${METAL_PUBLIC_CLASSES}

    PRIVATE_CLASSES
        instancerImager
        instancerShapeAdapter

    CPPFILES
        proxyShapeDelegate

    DISABLE_PRECOMPILED_HEADERS
)

