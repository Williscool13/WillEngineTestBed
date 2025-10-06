//
// Created by William on 2025-10-06.
//

#ifndef WILLENGINETESTBED_GAME_OBJECT_H
#define WILLENGINETESTBED_GAME_OBJECT_H
#include <vector>
#include <algorithm>

#include "types.h"

class GameObject;

class Component
{
public:
    virtual ~Component() = default;

    virtual void BeginPlay() = 0;

    virtual void Update(float dt) = 0;

    virtual void EndPlay() = 0;

    void SetOwner(GameObject* _owner)
    {
        owner = _owner;
    }

    GameObject* GetOwner() const
    {
        return owner;
    }

private:
    GameObject* owner{};
};

class PhysicsComponent : public Component
{
public:
    float velocityX = 0.0f;

    void BeginPlay() override
    {
        velocityX = -1.0f;
    }

    void Update(float dt) override;

    void EndPlay() override {}
};



class BounceComponent : public Component
{
public:
    float velocityY = 0.0f;
    float minY = 0.0f;
    float maxY = 10.0f;

    void BeginPlay() override
    {
        velocityY = 1.0f;
    }

    void Update(float dt) override;

    void EndPlay() override {}
};

class RenderComponent : public Component
{
public:
    float alpha = 1.0f;
    float pulseTime = 0.0f;
    float pulseFrequency = 1.0f;

    void BeginPlay() override {}

    void Update(float dt) override
    {
        pulseTime += dt;
        alpha = (sin(pulseTime * pulseFrequency) + 1.0f) * 0.5f;
    }

    void EndPlay() override {}
};

class GameObject
{
public:
    ~GameObject()
    {
        for (Component* comp : components) {
            delete comp;
        }
    }

    void BeginPlay()
    {
        for (Component* component : components) {
            component->BeginPlay();
        }
    }

    void Update(float deltaTime)
    {
        for (Component* component : components) {
            component->Update(deltaTime);
        }
    }

    void EndPlay()
    {
        for (Component* component : components) {
            component->EndPlay();
        }
    }

    void AddComponent(Component* component)
    {
        components.push_back(component);
        component->SetOwner(this);
    }

    void RemoveComponent(Component* component)
    {
        auto it = std::find(components.begin(), components.end(), component);
        if (it != components.end()) {
            delete *it;
            components.erase(it);
        }
        component->SetOwner(nullptr);
    }

    void RemoveAllComponents()
    {
        for (const Component* component : components) {
            delete component;
        }
        components.clear();
    }

    Transform transform;
private:
    std::vector<Component*> components;
};


#endif //WILLENGINETESTBED_GAME_OBJECT_H
