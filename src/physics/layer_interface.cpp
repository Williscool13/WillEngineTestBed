//
// Created by William on 2025-10-14.
//

#include "layer_interface.h"

namespace Physics
{
BPLayerInterfaceImpl::BPLayerInterfaceImpl()
{
    mObjectToBroadPhase[Layers::STATIC] = BroadPhaseLayers::STATIC;
    mObjectToBroadPhase[Layers::DYNAMIC] = BroadPhaseLayers::DYNAMIC;
}

JPH::uint BPLayerInterfaceImpl::GetNumBroadPhaseLayers() const
{
    return BroadPhaseLayers::NUM_LAYERS;
}

JPH::BroadPhaseLayer BPLayerInterfaceImpl::GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const
{
    JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
    return mObjectToBroadPhase[inLayer];
}

bool ObjectVsBroadPhaseLayerFilterImpl::ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const
{
    return !(inLayer1 == Layers::STATIC && inLayer2 == BroadPhaseLayers::STATIC);
}

bool ObjectLayerPairFilterImpl::ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const
{
    return !(inLayer1 == Layers::STATIC && inLayer2 == Layers::STATIC);
}
}
