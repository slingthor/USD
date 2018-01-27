set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE hd)

set(optionalPublicClasses "")

if (OPENSUBDIV_HAS_GLSL_TRANSFORM_FEEDBACK)
    add_definitions(-DOPENSUBDIV_HAS_GLSL_TRANSFORM_FEEDBACK)
endif()
if (OPENSUBDIV_HAS_GLSL_COMPUTE)
    add_definitions(-DOPENSUBDIV_HAS_GLSL_COMPUTE)
endif()

if (APPLE)
    set(GFX             mtlf)
    set(GFXADDITIONAL   Metal/codeGenMSL
                        Metal/bufferRelocatorMetal
                        Metal/bufferResourceMetal
                        Metal/mslProgram
                        Metal/mtlUtils
                        Metal/persistentBufferMetal
                        Metal/resourceMetal
                        Metal/textureResourceMetal)

else()
    set(GFX)
    set(GFXADDITIONAL)
endif()

pxr_library(hd
    LIBRARIES
        plug
        tf
        tracelite
        vt
        work
        sdf
        garch
        hf
        pxOsd
        glf
        ${GFX}
        ${OPENSUBDIV_LIBRARIES}
        ${GLEW_LIBRARY}
        ${TBB_tbb_LIBRARY}

    INCLUDE_DIRS
        ${OPENSUBDIV_INCLUDE_DIR}
        ${GLEW_INCLUDE_DIR}
        ${TBB_INCLUDE_DIRS}
        ${OPENSUBDIV_INCLUDE_DIR}

    PUBLIC_CLASSES
        basisCurves
        basisCurvesTopology
        binding
        bprim
        bufferArray
        bufferArrayRange
        bufferArrayRegistry
        bufferRelocator
        bufferResource
        bufferSource
        bufferSpec
        changeTracker
        codeGen
        computation
        computeShader
        conversions
        copyComputation
        debugCodes
        dirtyList
        drawingCoord
        drawItem
        engine
        extCompCpuComputation
        extCompGpuComputation
        extCompGpuComputationBufferSource
        extCompGpuComputationResource
        extCompPrimvarBufferSource
        extComputation
        extComputationContext
        fallbackLightingShader
        geometricShader
        instancer
        instanceRegistry
        interleavedMemoryManager
        lightingShader
        mesh
        meshTopology
        meshUtil
        package
        patchIndex
        perfLog
        points
        primGather
        primTypeIndex
        program
        renderContextCaps
        renderDelegate
        renderIndex
        renderPass
        renderPassShader
        renderPassState
        repr
        resource
        resourceBinder
        resourceRegistry
        rprim
        rprimCollection
        rprimSharedData
        sceneDelegate
        shader
        shaderCode
        shaderKey
        shaderParam
        sortedIds                   
        smoothNormals
        sprim
        strategyBase
        task
        texture
        textureResource
        tokens
        topology
        unitTestDelegate
        vboMemoryManager
        vboSimpleMemoryManager
        vertexAdjacency
        vtBufferSource
        GL/bufferRelocatorGL
        GL/bufferResourceGL
        GL/codeGenGLSL
        GL/glslProgram
        GL/glUtils
        GL/persistentBufferGL
        GL/resourceGL
        GL/textureResourceGL
        ${GFXADDITIONAL}

    PUBLIC_HEADERS
        api.h
        enums.h
        persistentBuffer.h
        types.h
        version.h

    PRIVATE_CLASSES
        compExtCompInputSource
        extCompInputSource
        extComputationContextInternal
        sceneExtCompInputSource
        unitTestHelper
        unitTestNullRenderDelegate
        unitTestNullRenderPass
        vtExtractor
        ${optionalPrivateClasses}

    RESOURCE_FILES
        plugInfo.json
        shaders/basisCurves.glslfx
        shaders/compute.glslfx
        shaders/fallbackLighting.glslfx
        shaders/fallbackLightingShader.glslfx
        shaders/fallbackSurface.glslfx
        shaders/frustumCull.glslfx
        shaders/instancing.glslfx
        shaders/lighting.glslfx
        shaders/lightingIntegrationShader.glslfx
        shaders/mesh.glslfx
        shaders/meshNormal.glslfx
        shaders/meshWire.glslfx
        shaders/points.glslfx
        shaders/ptexTexture.glslfx
        shaders/renderPass.glslfx
        shaders/renderPassShader.glslfx
        shaders/terminals.glslfx
)

