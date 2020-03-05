//
// Copyright 2019 Pixar
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
#ifndef PXR_IMAGING_HDX_PRESENT_TASK_H
#define PXR_IMAGING_HDX_PRESENT_TASK_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/task.h"
#include "pxr/imaging/hdx/api.h"
#include "pxr/imaging/hdx/fullscreenShader.h"

PXR_NAMESPACE_OPEN_SCOPE

class HgiInterop;

/// \class HdxPresentTask
///
/// A task for taking the final rendered and composited result of the aovs and
/// blit it into the viewers framebuffer.
///
class HdxPresentTask : public HdTask
{
public:
    HDX_API
    HdxPresentTask(HdSceneDelegate* delegate, SdfPath const& id);

    HDX_API
    virtual ~HdxPresentTask();

    HDX_API
    virtual void Sync(HdSceneDelegate* delegate,
                      HdTaskContext* ctx,
                      HdDirtyBits* dirtyBits) override;

    HDX_API
    virtual void Prepare(HdTaskContext* ctx,
                         HdRenderIndex* renderIndex) override;

    HDX_API
    virtual void Execute(HdTaskContext* ctx) override;

private:
    class Hgi* _hgi;

    std::unique_ptr<HdxFullscreenShader> _compositor;
    HgiInterop* _interop;

    HdxPresentTask() = delete;
    HdxPresentTask(const HdxPresentTask &) = delete;
    HdxPresentTask &operator =(const HdxPresentTask &) = delete;
    
    bool _flipImage;
};


/// \class HdxPresentTaskParams
///
/// PresentTask parameters.
///
struct HdxPresentTaskParams
{
    HdxPresentTaskParams() {}
    
    bool flipImage = false;
};

// VtValue requirements
HDX_API
std::ostream& operator<<(std::ostream& out, const HdxPresentTaskParams& pv);
HDX_API
bool operator==(const HdxPresentTaskParams& lhs,
                const HdxPresentTaskParams& rhs);
HDX_API
bool operator!=(const HdxPresentTaskParams& lhs,
                const HdxPresentTaskParams& rhs);


PXR_NAMESPACE_CLOSE_SCOPE

#endif
