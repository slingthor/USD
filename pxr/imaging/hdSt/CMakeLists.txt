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

set(optionalPublicClasses "")
set(optionalLibs "")
set(optionalIncludeDirs "")
if (PXR_ENABLE_PTEX_SUPPORT)
    list(APPEND optionalPublicClasses ptexMipmapTextureLoader)
    list(APPEND optionalLibs ${PTEX_LIBRARY})
    list(APPEND optionalIncludeDirs ${PTEX_INCLUDE_DIR})
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
                        GL/resourceBinderGL
                        GL/textureResourceGL)

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
                        Metal/resourceBinderMetal
                        Metal/textureResourceMetal)

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
        ${GLEW_LIBRARY}
        ${OPENSUBDIV_LIBRARIES}
        ${optionalLibs}

    INCLUDE_DIRS
        ${GLEW_INCLUDE_DIR}
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
        textureHandle
        textureHandleRegistry
        textureIdentifier
        textureObject
        textureObjectRegistry
        textureResource
        textureResourceHandle
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
        textureCpuData.h

    PRIVATE_CLASSES
        basisCurvesComputations
        basisCurvesShaderKey
        basisCurvesTopology
        codeGen
        computeShader
        cullingShaderKey
        drawBatch
        imageShaderShaderKey
        immediateDrawBatch
        indirectDrawBatch
        materialBufferSourceAndTextureHelper
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
        triangulate
        unitTestDelegate
        unitTestGLDrawing
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
        shaders/ptexTexture.glslfx
        shaders/renderPass.glslfx
        shaders/renderPassShader.glslfx
        shaders/simpleLightingShader.glslfx
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

