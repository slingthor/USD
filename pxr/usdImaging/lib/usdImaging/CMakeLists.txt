set(PXR_PREFIX pxr/usdImaging)
set(PXR_PACKAGE usdImaging)

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
        ${TBB_tbb_LIBRARY}

    INCLUDE_DIRS
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
        textureUtils
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

