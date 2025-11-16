#include "Input.h"
#include <unordered_map>

namespace cx
{
    struct InputState
    {
        std::unordered_map<int, bool> keysCurrent;
        std::unordered_map<int, bool> keysPrevious;

        std::unordered_map<int, bool> mouseButtonsCurrent;
        std::unordered_map<int, bool> mouseButtonsPrevious;

        Vector2 mousePosition;
        Vector2 mousePreviousPosition;
        float mouseWheelDelta;
    };

    static InputState* s_state = nullptr;

    void Input::Init()
    {
        s_state = new InputState();
    }

    void Input::Update()
    {
        if (!s_state)
            return;

        // Update previous states
        s_state->keysPrevious = s_state->keysCurrent;
        s_state->mouseButtonsPrevious = s_state->mouseButtonsCurrent;
        s_state->mousePreviousPosition = s_state->mousePosition;

        // Reset wheel delta
        s_state->mouseWheelDelta = 0.0f;
    }

    void Input::Shutdown()
    {
        if (s_state)
        {
            delete s_state;
            s_state = nullptr;
        }
    }

    bool Input::IsKeyPressed(KeyCode key)
    {
        if (!s_state)
            return false;

        int keyCode = static_cast<int>(key);
        return s_state->keysCurrent[keyCode] && !s_state->keysPrevious[keyCode];
    }

    bool Input::IsKeyDown(KeyCode key)
    {
        if (!s_state)
            return false;

        int keyCode = static_cast<int>(key);
        return s_state->keysCurrent[keyCode];
    }

    bool Input::IsKeyReleased(KeyCode key)
    {
        if (!s_state)
            return false;

        int keyCode = static_cast<int>(key);
        return !s_state->keysCurrent[keyCode] && s_state->keysPrevious[keyCode];
    }

    bool Input::IsMouseButtonPressed(MouseButton button)
    {
        if (!s_state)
            return false;

        int btn = static_cast<int>(button);
        return s_state->mouseButtonsCurrent[btn] && !s_state->mouseButtonsPrevious[btn];
    }

    bool Input::IsMouseButtonDown(MouseButton button)
    {
        if (!s_state)
            return false;

        int btn = static_cast<int>(button);
        return s_state->mouseButtonsCurrent[btn];
    }

    bool Input::IsMouseButtonReleased(MouseButton button)
    {
        if (!s_state)
            return false;

        int btn = static_cast<int>(button);
        return !s_state->mouseButtonsCurrent[btn] && s_state->mouseButtonsPrevious[btn];
    }

    Vector2 Input::GetMousePosition()
    {
        if (!s_state)
            return Vector2();

        return s_state->mousePosition;
    }

    Vector2 Input::GetMouseDelta()
    {
        if (!s_state)
            return Vector2();

        return s_state->mousePosition - s_state->mousePreviousPosition;
    }

    float Input::GetMouseWheelDelta()
    {
        if (!s_state)
            return 0.0f;

        return s_state->mouseWheelDelta;
    }

    void Input::UpdateKeyState(KeyCode key, bool pressed)
    {
        if (!s_state)
            return;

        s_state->keysCurrent[static_cast<int>(key)] = pressed;
    }

    void Input::UpdateMouseButtonState(MouseButton button, bool pressed)
    {
        if (!s_state)
            return;

        s_state->mouseButtonsCurrent[static_cast<int>(button)] = pressed;
    }

    void Input::UpdateMousePosition(float x, float y)
    {
        if (!s_state)
            return;

        s_state->mousePosition = Vector2(x, y);
    }

    void Input::UpdateMouseWheel(float delta)
    {
        if (!s_state)
            return;

        s_state->mouseWheelDelta = delta;
    }
}