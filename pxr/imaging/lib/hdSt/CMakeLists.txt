set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE hdSt)

if (NOT ${PXR_ENABLE_GL_SUPPORT})
    message(STATUS
        "Skipping ${PXR_PACKAGE} because PXR_ENABLE_GL_SUPPORT is OFF")
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


if (APPLE)
    set(GFX    mtlf)
    set(GFXADDITIONAL   Metal/bufferRelocatorMetal
                        Metal/bufferResourceMetal
                        Metal/codeGenMSL
                        Metal/dispatchBufferMetal
                        Metal/drawTargetTextureResourceMetal
                        Metal/flatNormalsMetal
                        Metal/indirectDrawBatchMetal
                        Metal/interleavedMemoryBufferMetal
                        Metal/metalConversions
                        Metal/mslProgram
                        Metal/mtlUtils
                        Metal/persistentBufferMetal
                        Metal/quadrangulateMetal
                        Metal/renderPassStateMetal
                        Metal/resourceBinderMetal
                        Metal/resourceMetal
                        Metal/smoothNormalsMetal
                        Metal/textureResourceMetal
                        Metal/vboMemoryBufferMetal
                        Metal/vboSimpleMemoryBufferMetal)
else()
    set(GFX)
    set(GFXADDITIONAL)
endif()

pxr_library(hdSt
    LIBRARIES
        garch
        glf
        hd
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
        dispatchBuffer
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
        flatNormals
        geometricShader
        glslfxShader
        instancer
        interleavedMemoryManager
        light      
        lightingShader
        material
        mesh
        mixinShaderCode
        package
        persistentBuffer
        points
        program
        renderDelegate
        renderPass
        renderPassShader
        renderPassState
        resourceBinder
        resourceRegistry
        shaderCode
        shaderKey
        smoothNormals
        surfaceShader
        texture
        textureResource
        tokens
        unitTestGLDrawing
        vboMemoryManager
        vboSimpleMemoryManager

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
        immediateDrawBatch
        indirectDrawBatch
        meshShaderKey
        meshTopology 
        pointsShaderKey
        quadrangulate
        subdivision
        subdivision3
        triangulate
        unitTestDelegate
        unitTestHelper
        GL/bufferRelocatorGL
        GL/bufferResourceGL
        GL/codeGenGLSL
        GL/dispatchBufferGL
        GL/glConversions
        GL/drawTargetTextureResourceGL
        GL/flatNormalsGL
        GL/glslProgram
        GL/glUtils
        GL/indirectDrawBatchGL
        GL/interleavedMemoryBufferGL
        GL/persistentBufferGL
        GL/quadrangulateGL
        GL/renderPassStateGL
        GL/resourceBinderGL
        GL/resourceGL
        GL/smoothNormalsGL
        GL/textureResourceGL
        GL/vboMemoryBufferGL
        GL/vboSimpleMemoryBufferGL
        ${GFXADDITIONAL}

    RESOURCE_FILES
        plugInfo.json
        shaders/basisCurves.glslfx
        shaders/compute.glslfx
        shaders/compute.metal
        shaders/edgeId.glslfx
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
        shaders/pointId.glslfx
        shaders/ptexTexture.glslfx
        shaders/renderPass.glslfx
        shaders/renderPassShader.glslfx
        shaders/terminals.glslfx
        shaders/visibility.glslfx
)

if (X11_FOUND OR APPLE)
pxr_build_test(testHdStBasicDrawing
    LIBRARIES
        hdSt
        hd
        garch
        glf
        ${GFX}
    CPPFILES
        testenv/testHdStBasicDrawing.cpp
)
endif()

