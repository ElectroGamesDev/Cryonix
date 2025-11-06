#pragma once

#include "Config.h"

struct GLFWwindow;

namespace cl
{
    class Window
    {
    public:
        virtual ~Window() = default;

        virtual bool Init(const Config& config) = 0;
        virtual void PollEvents() = 0;
        virtual bool ShouldClose() const = 0;
        virtual void Shutdown() = 0;

        virtual void* GetNativeWindowHandle() const = 0;
        virtual void GetWindowSize(int& width, int& height) const = 0;
        virtual void SetWindowTitle(const char* title) = 0;

        static Window* Create();
    };

#ifdef PLATFORM_WINDOWS
    class WindowsWindow : public Window
    {
    public:
        WindowsWindow();
        ~WindowsWindow() override;

        bool Init(const Config& config) override;
        void PollEvents() override;
        bool ShouldClose() const override;
        void Shutdown() override;

        void* GetNativeWindowHandle() const override;
        void GetWindowSize(int& width, int& height) const override;
        void SetWindowTitle(const char* title) override;

    private:
        static void GLFWKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void GLFWMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        static void GLFWCursorPosCallback(GLFWwindow* window, double xpos, double ypos);
        static void GLFWScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

        GLFWwindow* m_window;
        int m_width;
        int m_height;
    };
#endif
}