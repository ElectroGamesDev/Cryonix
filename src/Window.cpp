#include "Window.h"
#include "Input.h"
#include <glfw3.h>
#include <iostream>

#ifdef PLATFORM_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#include <glfw3native.h>
#endif

namespace cl
{
    Window* Window::Create()
    {
#ifdef PLATFORM_WINDOWS
        return new WindowsWindow();
#else
        return nullptr;
#endif
    }

#ifdef PLATFORM_WINDOWS
    static void GLFWErrorCallback(int error, const char* description)
    {
        std::cerr << "GLFW Error [" << error << "]: " << description << std::endl;
    }

    void WindowsWindow::GLFWKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        if (action == GLFW_PRESS || action == GLFW_RELEASE)
        {
            Input::UpdateKeyState(static_cast<KeyCode>(key), action == GLFW_PRESS);
        }
    }

    void WindowsWindow::GLFWMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
    {
        if (action == GLFW_PRESS || action == GLFW_RELEASE)
        {
            Input::UpdateMouseButtonState(static_cast<MouseButton>(button), action == GLFW_PRESS);
        }
    }

    void WindowsWindow::GLFWCursorPosCallback(GLFWwindow* window, double xpos, double ypos)
    {
        Input::UpdateMousePosition(static_cast<float>(xpos), static_cast<float>(ypos));
    }

    void WindowsWindow::GLFWScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
    {
        Input::UpdateMouseWheel(static_cast<float>(yoffset));
    }

    WindowsWindow::WindowsWindow()
        : m_window(nullptr)
        , m_width(0)
        , m_height(0)
    {
    }

    WindowsWindow::~WindowsWindow()
    {
        Shutdown();
    }

    bool WindowsWindow::Init(const Config& config)
    {
        glfwSetErrorCallback(GLFWErrorCallback);

        if (!glfwInit())
            return false;

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, config.windowResizable ? GLFW_TRUE : GLFW_FALSE);

        m_width = config.windowWidth;
        m_height = config.windowHeight;

        m_window = glfwCreateWindow(
            m_width,
            m_height,
            config.windowTitle,
            config.windowFullscreen ? glfwGetPrimaryMonitor() : nullptr,
            nullptr
        );

        if (!m_window)
        {
            glfwTerminate();
            return false;
        }

        // Set callbacks
        glfwSetKeyCallback(m_window, GLFWKeyCallback);
        glfwSetMouseButtonCallback(m_window, GLFWMouseButtonCallback);
        glfwSetCursorPosCallback(m_window, GLFWCursorPosCallback);
        glfwSetScrollCallback(m_window, GLFWScrollCallback);

        return true;
    }

    void WindowsWindow::PollEvents()
    {
        glfwPollEvents();
    }

    bool WindowsWindow::ShouldClose() const
    {
        return m_window ? glfwWindowShouldClose(m_window) : true;
    }

    void WindowsWindow::Shutdown()
    {
        if (m_window)
        {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
        glfwTerminate();
    }

    void* WindowsWindow::GetNativeWindowHandle() const
    {
#ifdef PLATFORM_WINDOWS
        return glfwGetWin32Window(m_window);
#else
        return nullptr;
#endif
    }

    void WindowsWindow::GetWindowSize(int& width, int& height) const
    {
        if (m_window)
        {
            glfwGetWindowSize(m_window, &width, &height);
        }
        else
        {
            width = m_width;
            height = m_height;
        }
    }

    void WindowsWindow::SetWindowTitle(const char* title)
    {
        if (m_window)
        {
            glfwSetWindowTitle(m_window, title);
        }
    }
#endif
}