//
// Created by William on 2025-10-06.
//

#include "game_object.h"

void PhysicsComponent::Update(float dt)
{
    if (velocityX > 1.0f) {
        velocityX = -1.0f;
    }
    else if (velocityX < -1.0f) {
        velocityX = 1.0f;
    }
    else {
        velocityX += 0.1f;
    }

    GetOwner()->transform.position.x += velocityX * dt;
}

void BounceComponent::Update(float dt)
{
    if (GetOwner()->transform.position.y < minY && velocityY < 0) {
        velocityY = 1.0f;
    }
    else if (GetOwner()->transform.position.y > maxY && velocityY > 0) {
        velocityY = -1.0f;
    }

    GetOwner()->transform.position.y += velocityY * dt;
}
