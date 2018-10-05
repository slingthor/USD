set(PXR_PREFIX pxr/usdImaging)
set(PXR_PACKAGE usdImaging)

# Note: both python include directives are included for compatibility.
pxr_library(usdImaging
    LIBRARIES
        garch
        gf
        tf
        plug
        trace
        vt
        work
        hd
        hdx
        pxOsd
        sdf
        usd
        usdGeom
        usdHydra
        usdLux
        usdShade
        usdVol
        ar
        ${Boost_PYTHON_LIBRARY}
        ${PYTHON_LIBRARY}
        ${TBB_tbb_LIBRARY}

    INCLUDE_DIRS
        ${PYTHON_INCLUDE_PATH}
        ${PYTHON_INCLUDE_DIRS}
        ${TBB_INCLUDE_DIRS}

    PUBLIC_CLASSES
        adapterRegistry
        collectionCache
        debugCodes
        delegate
        drawModeAdapter
        indexProxy
        inheritedCache
        instancerContext
        hydraMaterialAdapter
        package
        tokens
        valueCache
        basisCurvesAdapter
        capsuleAdapter
        coneAdapter
        cubeAdapter
        cylinderAdapter
        cylinderLightAdapter
        diskLightAdapter
        distantLightAdapter
        domeLightAdapter
        fieldAdapter
        gprimAdapter
        instanceAdapter
        lightAdapter
        materialAdapter
        meshAdapter
        nurbsPatchAdapter
        pointsAdapter
        pointInstancerAdapter
        primAdapter
        rectLightAdapter
        sphereAdapter
        sphereLightAdapter
        volumeAdapter

    PUBLIC_HEADERS
        api.h
        unitTestHelper.h
        version.h

    PRIVATE_CLASSES
        textureUtils

    RESOURCE_FILES
        plugInfo.json
        shaders/drawMode.glslfx
)

