#pragma once

#include "Maths.h"

namespace cl
{
    enum class KeyCode
    {
        Unknown = -1,
        Space = 32,
        Apostrophe = 39,
        Comma = 44,
        Minus = 45,
        Period = 46,
        Slash = 47,
        Num0 = 48, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
        Semicolon = 59,
        Equal = 61,
        A = 65, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        LeftBracket = 91,
        Backslash = 92,
        RightBracket = 93,
        GraveAccent = 96,
        Escape = 256,
        Enter = 257,
        Tab = 258,
        Backspace = 259,
        Insert = 260,
        Delete = 261,
        Right = 262,
        Left = 263,
        Down = 264,
        Up = 265,
        PageUp = 266,
        PageDown = 267,
        Home = 268,
        End = 269,
        CapsLock = 280,
        ScrollLock = 281,
        NumLock = 282,
        PrintScreen = 283,
        Pause = 284,
        F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
        LeftShift = 340,
        LeftControl = 341,
        LeftAlt = 342,
        LeftSuper = 343,
        RightShift = 344,
        RightControl = 345,
        RightAlt = 346,
        RightSuper = 347
    };

    enum class MouseButton
    {
        Left = 0,
        Right = 1,
        Middle = 2,
        Button4 = 3,
        Button5 = 4,
        Button6 = 5,
        Button7 = 6,
        Button8 = 7
    };

    class Input
    {
    public:
        static void Init();
        static void Update();
        static void Shutdown();

        // Keyboard
        static bool IsKeyPressed(KeyCode key);
        static bool IsKeyDown(KeyCode key);
        static bool IsKeyReleased(KeyCode key);

        // Mouse
        static bool IsMouseButtonPressed(MouseButton button);
        static bool IsMouseButtonDown(MouseButton button);
        static bool IsMouseButtonReleased(MouseButton button);
        static Vector2 GetMousePosition();
        static Vector2 GetMouseDelta();
        static float GetMouseWheelDelta();

    private:
        static void UpdateKeyState(KeyCode key, bool pressed);
        static void UpdateMouseButtonState(MouseButton button, bool pressed);
        static void UpdateMousePosition(float x, float y);
        static void UpdateMouseWheel(float delta);

        friend class WindowsWindow;
    };
}