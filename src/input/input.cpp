//
// Created by William on 2024-08-24.
//

#include "input/input.h"

#include <ranges>
#include <fmt/format.h>

namespace Core
{
void Input::Init(SDL_Window* window, const uint32_t w, const uint32_t h)
{
    this->window = window;
    this->windowExtents = glm::vec2(w, h);
}

void Input::ProcessEvent(const SDL_Event& event)
{
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        {
            const auto it = keyStateData.find(static_cast<Key>(event.key.key));
            if (it != keyStateData.end()) {
                UpdateInputState(it->second, event.key.down);
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            const auto it = mouseStateData.find(static_cast<MouseButton>(SDL_BUTTON_MASK(event.button.button)));
            if (it != mouseStateData.end()) {
                UpdateInputState(it->second, event.button.down);
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:
        {
            mouseXDelta += static_cast<float>(event.motion.xrel);
            mouseYDelta += static_cast<float>(event.motion.yrel);
            mousePositionAbsolute = {event.motion.x, event.motion.y};
            mousePosition = mousePositionAbsolute / windowExtents;
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL:
        {
            mouseWheelDelta += event.wheel.mouse_y;
            break;
        }
        default:
            break;
    }
}

void Input::UpdateFocus(const Uint32 sdlWindowFlags)
{
    bIsWindowInputFocus = (sdlWindowFlags & SDL_WINDOW_INPUT_FOCUS) != 0;

    if (bIsWindowInputFocus && IsKeyPressed(Key::NUMLOCKCLEAR)) {
        bIsCursorActive = !bIsCursorActive;
        if (!window) {
            fmt::print("Input: Attempted to update focus but window is not defined, perhaps init was not called?\n");
            return;
        }
        SDL_SetWindowRelativeMouseMode(window, bIsCursorActive);
    }
}

void Input::FrameReset()
{
    for (auto& val : keyStateData | std::views::values) {
        val.pressed = false;
        val.released = false;
    }

    for (auto& val : mouseStateData | std::views::values) {
        val.pressed = false;
        val.released = false;
    }

    mouseXDelta = 0;
    mouseYDelta = 0;
}

void Input::UpdateInputState(InputStateData& inputButton, const bool isPressed)
{
    if (!inputButton.held && isPressed) {
        inputButton.pressed = true;
    }
    if (inputButton.held && !isPressed) {
        inputButton.released = true;
    }

    inputButton.held = isPressed;
}


bool Input::IsKeyPressed(const Key key) const
{
    return keyStateData.at(key).pressed;
}

bool Input::IsKeyReleased(const Key key) const
{
    return keyStateData.at(key).released;
}

bool Input::IsKeyDown(const Key key) const
{
    return keyStateData.at(key).held;
}

bool Input::IsMousePressed(const MouseButton mouseButton) const
{
    return mouseStateData.at(mouseButton).pressed;
}

bool Input::IsMouseReleased(const MouseButton mouseButton) const
{
    return mouseStateData.at(mouseButton).released;
}

bool Input::IsMouseDown(const MouseButton mouseButton) const
{
    return mouseStateData.at(mouseButton).held;
}

Input::InputStateData Input::GetKeyData(const Key key) const
{
    return keyStateData.at(key);
}

Input::InputStateData Input::GetMouseData(const MouseButton mouseButton) const
{
    return mouseStateData.at(mouseButton);
}
}
