//
// Copyright 2017 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#include "pxr/imaging/glf/glew.h"

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/GL/renderDelegateGL.h"
#include "pxr/usdImaging/usdImagingGL/renderParams.h"

#include "pxr/imaging/garch/resourceFactory.h"

#include "pxr/imaging/glf/contextCaps.h"
#include "pxr/imaging/glf/diagnostic.h"

PXR_NAMESPACE_OPEN_SCOPE

HdStRenderDelegateGL::HdStRenderDelegateGL()
    : HdStRenderDelegate()
{
}

HdStRenderDelegateGL::HdStRenderDelegateGL(HdRenderSettingsMap const& settingsMap)
    : HdStRenderDelegate(settingsMap)
{
}

HdStRenderDelegateGL::~HdStRenderDelegateGL()
{
    // Nothing
}

bool
HdStRenderDelegateGL::IsSupported()
{
    if (GlfContextCaps::GetAPIVersion() >= 400)
        return true;

    return false;
}

void HdStRenderDelegateGL::PrepareRender(
    DelegateParams const &params)
{
    GarchContextCaps const &caps =
        GarchResourceFactory::GetInstance()->GetContextCaps();
    _isCoreProfileContext = caps.coreProfile;
    
    // User is responsible for initializing GL context and glew
    bool isCoreProfileContext = caps.coreProfile;
    
    GLF_GROUP_FUNCTION();
    
    if (isCoreProfileContext) {
        // We must bind a VAO (Vertex Array Object) because core profile
        // contexts do not have a default vertex array object. VAO objects are
        // container objects which are not shared between contexts, so we create
        // and bind a VAO here so that core rendering code does not have to
        // explicitly manage per-GL context state.
        glGenVertexArrays(1, &_vao);
        glBindVertexArray(_vao);
    } else {
        glPushAttrib(GL_ENABLE_BIT | GL_POLYGON_BIT | GL_DEPTH_BUFFER_BIT);
    }
    
    // hydra orients all geometry during topological processing so that
    // front faces have ccw winding. We disable culling because culling
    // is handled by fragment shader discard.
    if (params.flipFrontFacing) {
        glFrontFace(GL_CW); // < State is pushed via GL_POLYGON_BIT
    } else {
        glFrontFace(GL_CCW); // < State is pushed via GL_POLYGON_BIT
    }
    glDisable(GL_CULL_FACE);
    
    if (params.applyRenderState) {
        glDisable(GL_BLEND);
    }
    
    // note: to get benefit of alpha-to-coverage, the target framebuffer
    // has to be a MSAA buffer.
    if (params.enableIdRender) {
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    } else if (params.enableSampleAlphaToCoverage) {
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    }
    
    // for points width
    glEnable(GL_PROGRAM_POINT_SIZE);
    
    // TODO:
    //  * forceRefresh
    //  * showGuides, showRender, showProxy
    //  * gammaCorrectColors
}

void HdStRenderDelegateGL::FinalizeRender()
{
    if (_isCoreProfileContext) {
        
        glBindVertexArray(0);
        // XXX: We should not delete the VAO on every draw call, but we
        // currently must because it is GL Context state and we do not control
        // the context.
        glDeleteVertexArrays(1, &_vao);
        
    } else {
        glPopAttrib(); // GL_ENABLE_BIT | GL_POLYGON_BIT | GL_DEPTH_BUFFER_BIT
    }
}


PXR_NAMESPACE_CLOSE_SCOPE
