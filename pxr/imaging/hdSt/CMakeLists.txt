set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE hdSt)

# XXX We tmp check for PXR_ENABLE_GL_SUPPORT since Storm currently still only
# runs on OpenGL. Once Storm uses only Hgi, remove GL_SUPPORT check.
if (NOT ${PXR_BUILD_GPU_SUPPORT})
    message(STATUS
        "Skipping ${PXR_PACKAGE} because PXR_BUILD_GPU_SUPPORT is OFF")
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

set(optionalLibs "")
set(optionalIncludeDirs "")
set(optionalPublicClasses "")
set(optionalPrivateClasses "")
if (${PXR_ENABLE_MATERIALX_IMAGING_SUPPORT})
    add_definitions(-DPXR_MATERIALX_STDLIB_DIR="${MATERIALX_STDLIB_DIR}")
    add_definitions(-DPXR_MATERIALX_RESOURCE_DIR="${MATERIALX_RESOURCE_DIR}")

    list(APPEND optionalLibs ${MATERIALX_LIBRARIES})
    list(APPEND optionalIncludeDirs ${MATERIALX_INCLUDE_DIRS})
    list(APPEND optionalPrivateClasses
         materialXFilter
         materialXShaderGen
    )
endif()
if (PXR_ENABLE_PTEX_SUPPORT)
    list(APPEND optionalLibs ${PTEX_LIBRARY})
    list(APPEND optionalIncludeDirs ${PTEX_INCLUDE_DIR})
    list(APPEND optionalPublicClasses ptexMipmapTextureLoader)
endif()


set(GFX "")
set(GFXADDITIONAL "")
set(GFXADDITIONALPUBLIC "")

if (PXR_ENABLE_GL_SUPPORT)
    list(APPEND GFX     glf
    			        hgiGL)
    list(APPEND GFXADDITIONAL   GL/codeGenGLSL
                        GL/glslProgramGL
                        GL/indirectDrawBatchGL
                        GL/renderPassShaderGL
                        GL/renderPassStateGL
                        GL/resourceBinderGL)

    list(APPEND GFXADDITIONALPUBLIC
                        GL/renderDelegateGL
                        GL/resourceFactoryGL)

endif()

if (PXR_ENABLE_METAL_SUPPORT)
    list(APPEND GFX     mtlf
                        hgiMetal)
    list(APPEND GFXADDITIONAL  Metal/codeGenMSL
                        Metal/indirectDrawBatchMetal
                        Metal/metalConversions
                        Metal/glslProgramMetal
                        Metal/renderPassShaderMetal
                        Metal/renderPassStateMetal
                        Metal/resourceBinderMetal)

    list(APPEND GFXADDITIONALPUBLIC
                        Metal/renderDelegateMetal
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
        ${OPENSUBDIV_LIBRARIES}
        ${optionalLibs}

    INCLUDE_DIRS
        ${OPENSUBDIV_INCLUDE_DIR}
        ${optionalIncludeDirs}

    PUBLIC_CLASSES
        basisCurves
        bufferArrayRange
        bufferResource
        commandBuffer
        copyComputation
        debugCodes
        dispatchBuffer
        domeLightComputations
        drawItem
        drawItemInstance
        drawTarget
        drawTargetRenderPassState
        dynamicUvTextureObject
        extComputation
        extCompGpuComputation
        extCompGpuComputationBufferSource
        extCompGpuComputationResource
        extCompGpuPrimvarBufferSource
        fallbackLightingShader
        field
        fieldSubtextureIdentifier
        flatNormals
        geometricShader
        glConversions
        glfTextureCpuData
        glslfxShader
        glslProgram
        glUtils
        hgiConversions
        imageShaderRenderPass
        instancer
        interleavedMemoryManager
        light      
        lightingShader
        material
        materialNetwork
        mesh
        mixinShader
        package
        points
        ptexTextureObject
        renderBuffer
        renderDelegate
        renderPass
        renderPassShader
        renderPassState
        resourceFactory
        resourceRegistry
        rprimUtils
        samplerObject
        samplerObjectRegistry
        shaderCode
        simpleLightingShader
        smoothNormals
        subtextureIdentifier
        surfaceShader
        texture
        textureBinder
        textureCpuData
        textureHandle
        textureHandleRegistry
        textureIdentifier
        textureObject
        textureObjectRegistry
        tokens
        udimTextureObject
        vboMemoryManager
        vboSimpleMemoryManager
        volume
        ${optionalPublicClasses}
        ${GFXADDITIONALPUBLIC}

    PUBLIC_HEADERS
        api.h
        dynamicUvTextureImplementation.h

    PRIVATE_CLASSES
        assetUvTextureCpuData
        basisCurvesComputations
        basisCurvesShaderKey
        basisCurvesTopology
        codeGen
        cullingShaderKey
        drawBatch
        extCompComputeShader
        imageShaderShaderKey
        immediateDrawBatch
        indirectDrawBatch
        materialParam
        meshShaderKey
        meshTopology 
        pointsShaderKey
        quadrangulate
        spatialHierarchy
        resourceBinder
        shaderKey
        subdivision
        subdivision3
        textureUtils
        triangulate
        unitTestGLDrawing
        unitTestHelper
        volumeShader
        volumeShaderKey
        ${GFXADDITIONAL}
	${optionalPrivateClasses}

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
        shaders/meshFaceCull.glslfx
        shaders/meshNormal.glslfx
        shaders/meshWire.glslfx
        shaders/points.glslfx
        shaders/pointId.glslfx
        shaders/ptexTexture.glslfx
        shaders/renderPass.glslfx
        shaders/renderPassShader.glslfx
        shaders/simpleLightingShader.glslfx
        shaders/terminals.glslfx
        shaders/visibility.glslfx
        shaders/volume.glslfx

    DOXYGEN_FILES
        overview.dox
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

