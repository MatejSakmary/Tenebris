#pragma once

#include <functional>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <daxa/daxa.hpp>
#include <utility>

using namespace daxa::types;

struct WindowVTable
{
    std::function<void(f64, f64)> mouse_pos_callback;
    std::function<void(i32, i32, i32)> mouse_button_callback;
    std::function<void(i32, i32, i32, i32)> key_callback;
    std::function<void(i32, i32)> window_resized_callback;
};

struct AppWindow
{
    public:
        AppWindow(const i32vec2 dimensions, WindowVTable vtable) :
            dimensions{dimensions},
            vtable{std::move(vtable)}
        {
            glfwInit();
            /* Tell GLFW to not create OpenGL context */
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            window = glfwCreateWindow(dimensions.x, dimensions.y, "Atmosphere-daxa", nullptr, nullptr);
            glfwSetWindowUserPointer(window, &(this->vtable));
            glfwSetCursorPosCallback( 
                window,
                [](GLFWwindow *window_, f64 x, f64 y)
                { 
                    auto &vtable = *reinterpret_cast<WindowVTable *>(glfwGetWindowUserPointer(window_));
                    vtable.mouse_pos_callback(x, y); 
                }
            );
            glfwSetMouseButtonCallback(
                window,
                [](GLFWwindow *window_, i32 button, i32 action, i32 mods)
                {
                    auto &vtable = *reinterpret_cast<WindowVTable *>(glfwGetWindowUserPointer(window_));
                    vtable.mouse_button_callback(button, action, mods);
                }
            );
            glfwSetKeyCallback(
                window,
                [](GLFWwindow *window_, i32 key, i32 code, i32 action, i32 mods)
                {
                    auto &vtable = *reinterpret_cast<WindowVTable *>(glfwGetWindowUserPointer(window_));
                    vtable.key_callback(key, code, action, mods);
                }
            );
            glfwSetFramebufferSizeCallback( 
                window, 
                [](GLFWwindow *window_, i32 x, i32 y)
                { 
                    auto &vtable = *reinterpret_cast<WindowVTable *>(glfwGetWindowUserPointer(window_));
                    vtable.window_resized_callback(x, y); 
                }
            );
        }
        void set_input_mode(i32 mode, i32 value) { glfwSetInputMode(window, mode, value); }

        [[nodiscard]] i32 get_key_state(i32 key) { return glfwGetKey(window, key); }
        [[nodiscard]] auto get_window_should_close() const -> bool { return glfwWindowShouldClose(window); }
        [[nodiscard]] auto get_glfw_window_handle() const -> GLFWwindow* { return window; }
        [[nodiscard]] auto get_native_handle() const -> daxa::NativeWindowHandle
        {
#if defined(_WIN32)
            return reinterpret_cast<daxa::NativeWindowHandle>(glfwGetWin32Window(window));
#elif defined(__linux__)
            return reinterpret_cast<daxa::NativeWindowHandle>(glfwGetX11Window(window));
#endif
        }

        ~AppWindow()
        {
            glfwDestroyWindow(window);
            glfwTerminate();
        }
    private:
        GLFWwindow* window;
        [[maybe_unused]] i32vec2 dimensions;
        WindowVTable vtable;
};