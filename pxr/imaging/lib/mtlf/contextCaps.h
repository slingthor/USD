//
// Copyright 2018 Pixar
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
#ifndef MTLF_CONTEXT_CAPS_H
#define MTLF_CONTEXT_CAPS_H

#include "pxr/pxr.h"
#include "pxr/imaging/mtlf/api.h"
#include "pxr/imaging/garch/contextCaps.h"


#include <boost/noncopyable.hpp>

PXR_NAMESPACE_OPEN_SCOPE


/// \class MtlfContextCaps
///
/// This class is intended to be a cache of the capabilites
/// (resource limits and features) of the underlying
/// GL context.
///
/// It serves two purposes.  Firstly to reduce driver
/// transition overhead of querying these values.
/// Secondly to provide access to these values from other
/// threads that don't have the context bound.
///
/// TO DO (bug #124971):
///   - LoadCaps() should be called whenever the context
///     changes.
///   - Provide a mechanism where other Hd systems can
///     subscribe to when the caps changes, so they can
///     update and invalidate.
///
class MtlfContextCaps : public GarchContextCaps {

public:
    MTLF_API
    static int GetAPIVersion();

private:

    MtlfContextCaps();
    virtual ~MtlfContextCaps() {}

    void _LoadCaps();

    friend class MtlfResourceFactory;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MTLF_CONTEXT_CAPS_H

