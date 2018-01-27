set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE hdSt)

if (OPENSUBDIV_HAS_GLSL_TRANSFORM_FEEDBACK)
    add_definitions(-DOPENSUBDIV_HAS_GLSL_TRANSFORM_FEEDBACK)
endif()
if (OPENSUBDIV_HAS_GLSL_COMPUTE)
    add_definitions(-DOPENSUBDIV_HAS_GLSL_COMPUTE)
endif()

if (APPLE)
    set(GFX    mtlf)
    set(GFXADDITIONAL   Metal/indirectDrawBatchMetal
                        Metal/drawTargetTextureResourceMetal)
else()
    set(GFX)
    set(GFXADDITIONAL)
endif()

pxr_library(hdSt
    LIBRARIES
        cameraUtil
        hd
        tf
        tracelite

    INCLUDE_DIRS
        ${OPENSUBDIV_INCLUDE_DIR}

    PUBLIC_CLASSES
        basisCurves
        camera
        commandBuffer
        dispatchBuffer
        drawItemInstance
        drawTarget
        drawTargetAttachmentDesc
        drawTargetAttachmentDescArray
        drawTargetRenderPassState
        glslfxShader
        instancer
        light      
        mesh
        mixinShaderCode
        points
        renderDelegate
        renderPass
        resourceRegistry
        shader
        surfaceShader
        unitTestGLDrawing

    PUBLIC_HEADERS
        api.h

    PRIVATE_CLASSES
        basisCurvesComputations
        basisCurvesShaderKey
        basisCurvesTopology
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
        unitTestHelper
        GL/indirectDrawBatchGL
        GL/drawTargetTextureResourceGL
        ${GFXADDITIONAL}

    RESOURCE_FILES
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

