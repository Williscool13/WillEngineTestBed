//
// Created by William on 2025-10-14.
//

#ifndef WILLENGINETESTBED_LAYER_INTERFACE_H
#define WILLENGINETESTBED_LAYER_INTERFACE_H

#include <Jolt/Jolt.h>

#include "JoltPhysics/Jolt/Physics/Collision/ObjectLayer.h"
#include "JoltPhysics/Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h"

namespace Physics
{
namespace Layers
{
    static constexpr JPH::ObjectLayer STATIC = 0;
    static constexpr JPH::ObjectLayer DYNAMIC = 1;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
}

namespace BroadPhaseLayers
{
    static constexpr JPH::BroadPhaseLayer STATIC(0);
    static constexpr JPH::BroadPhaseLayer DYNAMIC(1);
    static constexpr JPH::uint NUM_LAYERS = 2;
}

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
    BPLayerInterfaceImpl();

    ~BPLayerInterfaceImpl() override = default;

    [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override;

    [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override;

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* BPLayerInterfaceImpl::GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const
    {
        switch ((JPH::BroadPhaseLayer::Type) inLayer) {
            case (JPH::BroadPhaseLayer::Type) BroadPhaseLayers::STATIC: return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type) BroadPhaseLayers::DYNAMIC: return "MOVING";
            default: JPH_ASSERT(false);
                return "INVALID";
        }
    }
#endif

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS]{};
};


class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
    ~ObjectVsBroadPhaseLayerFilterImpl() override = default;

    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override;
};

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
{
public:
    ~ObjectLayerPairFilterImpl() override = default;

    /**
     * Should do physics checks against specified layer?
     * @param inLayer1
     * @param inLayer2
     * @return
     */
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override;
};
}


#endif //WILLENGINETESTBED_LAYER_INTERFACE_H
