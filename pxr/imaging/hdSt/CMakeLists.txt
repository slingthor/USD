set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE hdSt)

if ((NOT ${PXR_ENABLE_GL_SUPPORT}) AND (NOT ${PXR_ENABLE_METAL_SUPPORT}))
    message(STATUS
        "Skipping ${PXR_PACKAGE} because PXR_ENABLE_GL_SUPPORT and PXR_ENABLE_METAL_SUPPORT is OFF")
    return()
endif()

if (OPENSUBDIV_HAS_GLSL_TRANSFORM_FEEDBACK)
    add_definitions(-DOPENSUBDIV_HAS_GLSL_TRANSFORM_FEEDBACK)
endif()
if (OPENSUBDIV_HAS_GLSL_COMPUTE)
    add_definitions(-DOPENSUBDIV_HAS_GLSL_COMPUTE)
endif()
if (OPENSUBDIV_HAS_METAL_COMPUTE)
    add_definitions(-DOPENSUBDIV_HAS_METAL_COMPUTE)
endif()

if (PXR_ENABLE_GL_SUPPORT)
    set(GFX             glf
    			        hgiGL)
    set(GFXADDITIONAL   GL/bufferRelocatorGL
                        GL/bufferResourceGL
                        GL/codeGenGLSL
                        GL/dispatchBufferGL
                        GL/domeLightComputationsGL
                        GL/drawTargetTextureResourceGL
                        GL/extCompGpuComputationGL
                        GL/flatNormalsGL
                        GL/glslProgram
                        GL/glUtils
                        GL/indirectDrawBatchGL
                        GL/interleavedMemoryBufferGL
                        GL/persistentBufferGL
                        GL/quadrangulateGL
                        GL/renderDelegateGL
                        GL/renderPassStateGL
                        GL/resourceBinderGL
                        GL/resourceGL
                        GL/smoothNormalsGL
                        GL/textureResourceGL
                        GL/vboMemoryBufferGL
                        GL/vboSimpleMemoryBufferGL
                        unitTestGLDrawing)

    set(GFXADDITIONALPUBLIC ${GFXADDITIONALPUBLIC}
                            GL/resourceFactoryGL)

endif()

if (PXR_ENABLE_METAL_SUPPORT)
    set(GFX             ${GFX}
                        mtlf)
    set(GFXADDITIONAL   ${GFXADDITIONAL}
                        Metal/bufferRelocatorMetal
                        Metal/bufferResourceMetal
                        Metal/codeGenMSL
                        Metal/dispatchBufferMetal
                        Metal/domeLightComputationsMetal
                        Metal/drawTargetTextureResourceMetal
                        Metal/extCompGpuComputationMetal
                        Metal/flatNormalsMetal
                        Metal/indirectDrawBatchMetal
                        Metal/interleavedMemoryBufferMetal
                        Metal/metalConversions
                        Metal/mslProgram
                        Metal/mtlUtils
                        Metal/persistentBufferMetal
                        Metal/quadrangulateMetal
                        Metal/renderDelegateMetal
                        Metal/renderPassStateMetal
                        Metal/resourceBinderMetal
                        Metal/resourceMetal
                        Metal/smoothNormalsMetal
                        Metal/textureResourceMetal
                        Metal/vboMemoryBufferMetal
                        Metal/vboSimpleMemoryBufferMetal)

set(GFXADDITIONALPUBLIC ${GFXADDITIONALPUBLIC}
                        Metal/resourceFactoryMetal)
endif()

pxr_library(hdSt
    LIBRARIES
        hio
        garch
        hd
        sdr
        tf
        trace
        ${GFX}
        ${GLEW_LIBRARY}
        ${OPENSUBDIV_LIBRARIES}

    INCLUDE_DIRS
        ${GLEW_INCLUDE_DIR}
        ${OPENSUBDIV_INCLUDE_DIR}

    PUBLIC_CLASSES
        basisCurves
        bufferRelocator
        bufferResource
        commandBuffer
        computeShader
        copyComputation
        debugCodes
        dispatchBuffer
        domeLightComputations
        drawItem
        drawItemInstance
        drawTarget
        drawTargetAttachmentDesc
        drawTargetAttachmentDescArray
        drawTargetRenderPassState
        extComputation
        extCompGpuComputation
        extCompGpuComputationBufferSource
        extCompGpuComputationResource
        extCompGpuPrimvarBufferSource
        fallbackLightingShader
        field
        fieldResource
        flatNormals
        geometricShader
        glConversions
        glslfxShader
        hgiConversions
        imageShaderRenderPass
        instancer
        interleavedMemoryManager
        light      
        lightingShader
        material
        mesh
        mixinShader
        package
        persistentBuffer
        points
        program
	    renderBuffer
        renderDelegate
        renderPass
        renderPassShader
        renderPassState
        resourceBinder
        resourceFactory
        resourceRegistry
        rprimUtils
        shaderCode
        shaderKey
        smoothNormals
        surfaceShader
        texture
        textureResource
        textureResourceHandle
        tokens
        vboMemoryManager
        vboSimpleMemoryManager
	volume
        ${GFXADDITIONALPUBLIC}

    PUBLIC_HEADERS
        api.h

    PRIVATE_CLASSES
        basisCurvesComputations
        basisCurvesShaderKey
        basisCurvesTopology
        codeGen
        cullingShaderKey
        drawBatch
        drawTargetTextureResource
        imageShaderShaderKey
        immediateDrawBatch
        indirectDrawBatch
        materialBufferSourceAndTextureHelper
        materialNetwork
        meshShaderKey
        meshTopology 
        pointsShaderKey
        quadrangulate
        spatialHierarchy
        subdivision
        subdivision3
        triangulate
        unitTestDelegate
        unitTestHelper
        volumeShader
	volumeShaderKey
        ${GFXADDITIONAL}

    RESOURCE_FILES
        plugInfo.json
        shaders/basisCurves.glslfx
        shaders/compute.glslfx
        shaders/compute.metal
	shaders/domeLight.glslfx
        shaders/edgeId.glslfx
        shaders/fallbackLighting.glslfx
        shaders/fallbackLightingShader.glslfx
        shaders/fallbackSurface.glslfx
        shaders/fallbackVolume.glslfx
        shaders/frustumCull.glslfx
        shaders/imageShader.glslfx
        shaders/instancing.glslfx
        shaders/lighting.glslfx
        shaders/lightingIntegrationShader.glslfx
        shaders/mesh.glslfx
        shaders/meshNormal.glslfx
        shaders/meshWire.glslfx
        shaders/points.glslfx
        shaders/pointId.glslfx
        shaders/ptexTextureGL.glslfx
        shaders/ptexTextureMetal.glslfx
        shaders/renderPass.glslfx
        shaders/renderPassShader.glslfx
        shaders/terminals.glslfx
        shaders/visibility.glslfx
        shaders/volume.glslfx
)

if (X11_FOUND OR APPLE)
pxr_build_test(testHdStBasicDrawing
    LIBRARIES
        hdSt
        hd
        garch
        ${GFX}
    CPPFILES
        testenv/testHdStBasicDrawing.cpp
)
endif()

