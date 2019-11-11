//
// Copyright 2016 Pixar
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
#ifndef HD_RESOURCE_REGISTRY_H
#define HD_RESOURCE_REGISTRY_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/api.h"
#include "pxr/imaging/hd/version.h"

#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/bufferArrayRegistry.h"
#include "pxr/imaging/hd/bufferSource.h"
#include "pxr/imaging/hd/bufferSpec.h"
#include "pxr/imaging/hd/instanceRegistry.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/strategyBase.h"

#include "pxr/imaging/hf/perfLog.h"

#include "pxr/base/vt/dictionary.h"
#include "pxr/base/tf/singleton.h"
#include "pxr/base/tf/token.h"

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <map>
#include <memory>
#include <atomic>
#include <tbb/concurrent_vector.h>

PXR_NAMESPACE_OPEN_SCOPE


typedef boost::shared_ptr<class HdBufferArray> HdBufferArraySharedPtr;
typedef boost::shared_ptr<class HdMeshTopology> HdMeshTopologySharedPtr;
typedef boost::shared_ptr<class HdBasisCurvesTopology> 
                                                 HdBasisCurvesTopologySharedPtr;
typedef boost::shared_ptr<class HdComputation> HdComputationSharedPtr;
typedef boost::shared_ptr<class HdTextureResource> HdTextureResourceSharedPtr;
typedef boost::shared_ptr<class Hd_VertexAdjacency> Hd_VertexAdjacencySharedPtr;
typedef boost::shared_ptr<class HdResourceRegistry> HdResourceRegistrySharedPtr;

/// \class HdResourceRegistry
///
/// A central registry of all GPU resources.
///
class HdResourceRegistry : public boost::noncopyable  {
public:
    HF_MALLOC_TAG_NEW("new HdResourceRegistry");

    HD_API
    HdResourceRegistry();

    HD_API
    virtual ~HdResourceRegistry();
    /// Allocate new non uniform buffer array range
    HD_API
    HdBufferArrayRangeSharedPtr AllocateNonUniformBufferArrayRange(
        TfToken const &role,
        HdBufferSpecVector const &bufferSpecs,
        HdBufferArrayUsageHint usageHint);

    /// Allocate new immutable non uniform buffer array range
    HD_API
    HdBufferArrayRangeSharedPtr AllocateNonUniformImmutableBufferArrayRange(
        TfToken const &role,
        HdBufferSpecVector const &bufferSpecs,
        HdBufferArrayUsageHint usageHint);

    /// Allocate new uniform buffer range
    HD_API
    HdBufferArrayRangeSharedPtr AllocateUniformBufferArrayRange(
        TfToken const &role,
        HdBufferSpecVector const &bufferSpecs,
        HdBufferArrayUsageHint usageHint);

    /// Allocate new shader storage buffer range
    HD_API
    HdBufferArrayRangeSharedPtr AllocateShaderStorageBufferArrayRange(
        TfToken const &role,
        HdBufferSpecVector const &bufferSpecs,
        HdBufferArrayUsageHint usageHint);

    /// Allocate single entry (non-aggregated) buffer array range
    HD_API
    HdBufferArrayRangeSharedPtr AllocateSingleBufferArrayRange(
        TfToken const &role,
        HdBufferSpecVector const &bufferSpecs,
        HdBufferArrayUsageHint usageHint);

    /// Append source data for given range to be committed later.
    HD_API
    void AddSources(HdBufferArrayRangeSharedPtr const &range,
                    HdBufferSourceVector &sources);

    /// Append a source data for given range to be committed later.
    HD_API
    void AddSource(HdBufferArrayRangeSharedPtr const &range,
                   HdBufferSourceSharedPtr const &source);

    /// Append a source data just to be resolved (used for cpu computations).
    HD_API
    void AddSource(HdBufferSourceSharedPtr const &source);

    /// Append a gpu computation into queue.
    /// The parameter 'range' specifies the destination buffer range,
    /// which has to be allocated by caller of this function.
    ///
    /// note: GPU computations will be executed in the order that
    /// they are registered.
    HD_API
    void AddComputation(HdBufferArrayRangeSharedPtr const &range,
                        HdComputationSharedPtr const &computaion);

    /// Commits all in-flight source data to the GPU, freeing the source
    /// buffers.
    HD_API
    void Commit();

    /// cleanup all buffers and remove if empty
    HD_API
    void GarbageCollect();

    /// cleanup all Bprim registries
    HD_API
    void GarbageCollectBprims();

    /// Set the aggregation strategy for non uniform parameters
    /// (vertex, varying, facevarying)
    /// Takes ownership of the passed in strategy object.
    void SetNonUniformAggregationStrategy(HdAggregationStrategy *strategy) {
        _nonUniformAggregationStrategy.reset(strategy);
    }

    /// Set the aggregation strategy for non uniform immutable parameters
    /// (vertex, varying, facevarying)
    /// Takes ownership of the passed in strategy object.
    void SetNonUniformImmutableAggregationStrategy(
        HdAggregationStrategy *strategy) {
        _nonUniformImmutableAggregationStrategy.reset(strategy);
    }

    /// Set the aggregation strategy for uniform (shader globals)
    /// Takes ownership of the passed in strategy object.
    void SetUniformAggregationStrategy(HdAggregationStrategy *strategy) {
        _uniformUboAggregationStrategy.reset(strategy);
    }

    /// Set the aggregation strategy for SSBO (uniform primvars)
    /// Takes ownership of the passed in strategy object.
    void SetShaderStorageAggregationStrategy(HdAggregationStrategy *strategy) {
        _uniformSsboAggregationStrategy.reset(strategy);
    }

    /// Set the aggregation strategy for single buffers (for nested instancer).
    /// Takes ownership of the passed in strategy object.
    void SetSingleStorageAggregationStrategy(HdAggregationStrategy *strategy) {
        _singleAggregationStrategy.reset(strategy);
    }

    /// Returns whether an aggregation strategy is set for non uniform params.
    bool HasNonUniformAggregationStrategy() const {
        return _nonUniformAggregationStrategy.get();
    }

    /// Returns whether an aggregation strategy is set for non uniform
    /// immutable params.
    bool HasNonUniformImmutableAggregationStrategy() const {
        return _nonUniformImmutableAggregationStrategy.get();
    }

    /// Returns whether an aggregation strategy is set for uniform params.
    bool HasUniformAggregationStrategy() const {
        return _uniformUboAggregationStrategy.get();
    }

    /// Returns whether an aggregation strategy is set for SSBO.
    bool HasShaderStorageAggregationStrategy() const {
        return _uniformSsboAggregationStrategy.get();
    }

    /// Returns whether an aggregation strategy is set for single buffers.
    bool HasSingleStorageAggregationStrategy() const {
        return _singleAggregationStrategy.get();
    }

    /// Returns a report of resource allocation by role in bytes and
    /// a summary total allocation of GPU memory in bytes for this registry.
    HD_API
    VtDictionary GetResourceAllocation() const;

    /// Topology instancing
    /// Returns (in the pointer) the HdInstance points to shared HdMeshTopology,
    /// distinguished by given ID. If IsFirstInstance() of the instance is
    /// true, the caller is responsible to initialize the HdMeshTopology.
    /// Also the HdMeshTopology has to be owned by someone else, otherwise
    /// the entry will get deleted on GarbageCollect(). HdInstance is
    /// intended to be used for temporary pointer, so the caller should not
    /// hold the instance for an extended time. ID is used as a hash key
    /// and the resolution of hash collision is the client's responsibility.
    ///
    /// Note: As entries can be added by multiple threads the routine
    /// returns a lock on the instance registry.  This lock should be held
    /// until the HdInstance object is destroyed.
    HD_API
    HdInstance<HdMeshTopologySharedPtr>
    RegisterMeshTopology(HdInstance<HdMeshTopologySharedPtr>::ID id);

    HD_API
    HdInstance<HdBasisCurvesTopologySharedPtr>
    RegisterBasisCurvesTopology(
        HdInstance<HdBasisCurvesTopologySharedPtr>::ID id);

    HD_API
    HdInstance<Hd_VertexAdjacencySharedPtr>
    RegisterVertexAdjacency(HdInstance<Hd_VertexAdjacencySharedPtr>::ID id);

    /// Index buffer array range instancing
    /// Returns the HdInstance points to shared HdBufferArrayRange,
    /// distinguished by given ID.
    /// *Refer the comment on RegisterTopology for the same consideration.
    HD_API
    HdInstance<HdBufferArrayRangeSharedPtr>
    RegisterMeshIndexRange(
        HdInstance<HdBufferArrayRangeSharedPtr>::ID id, TfToken const &name);

    HD_API
    HdInstance<HdBufferArrayRangeSharedPtr>
    RegisterBasisCurvesIndexRange(
       HdInstance<HdBufferArrayRangeSharedPtr>::ID id, TfToken const &name);

    /// Primvar array range instancing
    /// Returns the HdInstance pointing to shared HdBufferArrayRange,
    /// distinguished by given ID.
    /// *Refer the comment on RegisterTopology for the same consideration.
    HD_API
    HdInstance<HdBufferArrayRangeSharedPtr>
    RegisterPrimvarRange(
        HdInstance<HdBufferArrayRangeSharedPtr>::ID id);

    /// ExtComputation data array range instancing
    /// Returns the HdInstance pointing to shared HdBufferArrayRange,
    /// distinguished by given ID.
    /// *Refer the comment on RegisterTopology for the same consideration.
    HD_API
    HdInstance<HdBufferArrayRangeSharedPtr>
    RegisterExtComputationDataRange(
        HdInstance<HdBufferArrayRangeSharedPtr>::ID id);

    /// Globally unique id for texture, see RegisterTextureResource() for
    /// details.
    typedef size_t TextureKey;

    /// Register a texture into the texture registry.
    /// Typically the other id's used refer to unique content
    /// where as for textures it's a unique id provided by the scene delegate.
    /// Hydra expects the id's to be unique in the context of a scene/stage
    /// aka render index.  However, the texture registry can be shared between
    /// multiple render indices, so the renderIndexId is used to create
    /// a globally unique id for the texture resource.
    HD_API
    HdInstance<HdTextureResourceSharedPtr>
    RegisterTextureResource(TextureKey id);

    /// Find a texture in the texture registry. If found, it returns it.
    /// See RegisterTextureResource() for parameter details.
    HD_API
    HdInstance<HdTextureResourceSharedPtr>
    FindTextureResource(TextureKey id, bool *found);

    /// Invalidate any shaders registered with this registry.
    HD_API
    virtual void InvalidateShaderRegistry();

    /// Debug dump
    HD_API
    friend std::ostream &operator <<(std::ostream &out,
                                     const HdResourceRegistry& self);

protected:
    /// A hook for derived registries to perform additional GC when
    /// GarbageCollect() is invoked.
    virtual void _GarbageCollect();

    /// A hook for derived registries to tally their resources
    /// by key into the given dictionary.  Any additions should
    /// be cumulative with the existing key values. 
    virtual void _TallyResourceAllocation(VtDictionary *result) const;

protected:

    // aggregated buffer array
    HdBufferArrayRegistry _nonUniformBufferArrayRegistry;
    HdBufferArrayRegistry _nonUniformImmutableBufferArrayRegistry;
    HdBufferArrayRegistry _uniformUboBufferArrayRegistry;
    HdBufferArrayRegistry _uniformSsboBufferArrayRegistry;
    HdBufferArrayRegistry _singleBufferArrayRegistry;

    // current aggregation strategies
    std::unique_ptr<HdAggregationStrategy> _nonUniformAggregationStrategy;
    std::unique_ptr<HdAggregationStrategy>
                                _nonUniformImmutableAggregationStrategy;
    std::unique_ptr<HdAggregationStrategy> _uniformUboAggregationStrategy;
    std::unique_ptr<HdAggregationStrategy> _uniformSsboAggregationStrategy;
    std::unique_ptr<HdAggregationStrategy> _singleAggregationStrategy;

private:

    // TODO: this is a transient structure. we'll revisit the BufferSource
    // interface later.
    struct _PendingSource {
        _PendingSource(HdBufferArrayRangeSharedPtr const &range)
            : range(range)
            , sources() 
        {
        }

        _PendingSource(HdBufferArrayRangeSharedPtr const &range,
                       HdBufferSourceSharedPtr     const &source)
            : range(range)
            , sources(1, source)
        {
        }

        HdBufferArrayRangeSharedPtr range;
        HdBufferSourceVector sources;
    };

    typedef tbb::concurrent_vector<_PendingSource> _PendingSourceList;
    _PendingSourceList    _pendingSources;
    std::atomic_size_t   _numBufferSourcesToResolve;
    


    struct _PendingComputation{
        _PendingComputation(HdBufferArrayRangeSharedPtr const &range,
                            HdComputationSharedPtr const &computation)
            : range(range), computation(computation) { }
        HdBufferArrayRangeSharedPtr range;
        HdComputationSharedPtr computation;
    };

    typedef tbb::concurrent_vector<_PendingComputation> _PendingComputationList;
    _PendingComputationList  _pendingComputations;

    // instance registries

    // Register mesh topology.
    HdInstanceRegistry<HdMeshTopologySharedPtr>
        _meshTopologyRegistry;

    // Register basisCurves topology.
    HdInstanceRegistry<HdBasisCurvesTopologySharedPtr>
        _basisCurvesTopologyRegistry;

    // Register vertex adjacency.
    HdInstanceRegistry<Hd_VertexAdjacencySharedPtr>
        _vertexAdjacencyRegistry;

    // Register topology index buffers.
    typedef HdInstanceRegistry<HdBufferArrayRangeSharedPtr>
        _TopologyIndexRangeInstanceRegistry;
    typedef tbb::concurrent_unordered_map< TfToken,
                                           _TopologyIndexRangeInstanceRegistry,
                                           TfToken::HashFunctor >
        _TopologyIndexRangeInstanceRegMap;

    _TopologyIndexRangeInstanceRegMap _meshTopologyIndexRangeRegistry;
    _TopologyIndexRangeInstanceRegMap _basisCurvesTopologyIndexRangeRegistry;

    // Register shared primvar buffers.
    HdInstanceRegistry<HdBufferArrayRangeSharedPtr>
        _primvarRangeRegistry;

    // Register ext computation resource.
    HdInstanceRegistry<HdBufferArrayRangeSharedPtr>
        _extComputationDataRangeRegistry;

    // texture resource registry
    HdInstanceRegistry<HdTextureResourceSharedPtr>
        _textureResourceRegistry;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif //HD_RESOURCE_REGISTRY_H
