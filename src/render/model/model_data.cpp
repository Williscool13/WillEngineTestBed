//
// Created by William on 2025-10-20.
//

#include "model_data.h"

namespace Renderer
{
RuntimeNode::RuntimeNode(const Node& n)
{
    parent = n.parent;
    depth = n.depth;
    meshIndex = n.meshIndex;
    transform = {n.localTranslation, n.localRotation, n.localScale};
    jointMatrixIndex = n.inverseBindIndex;
}
} // Renderer
