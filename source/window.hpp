#pragma once

#include <functional>

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_NATIVE_INCLUDE_NONE
#define WIN32_LEAN_AND_MEAN
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>


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
        AppWindow(const u32vec2 dimensions, const WindowVTable & vtable ) :
            dimensions{dimensions},
            vtable{vtable}
        {
            glfwInit();
            /* Tell GLFW to not create OpenGL context */
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
            window = glfwCreateWindow(dimensions.x, dimensions.y, "Atmosphere-daxa", nullptr, nullptr);
            glfwSetWindowUserPointer(window, &(this->vtable));
            glfwSetCursorPosCallback( 
                window,
                [](GLFWwindow *window, f64 x, f64 y)
                { 
                    auto &vtable = *reinterpret_cast<WindowVTable *>(glfwGetWindowUserPointer(window));
                    vtable.mouse_pos_callback(x, y); 
                }
            );
            glfwSetMouseButtonCallback(
                window,
                [](GLFWwindow *window, i32 button, i32 action, i32 mods)
                {
                    auto &vtable = *reinterpret_cast<WindowVTable *>(glfwGetWindowUserPointer(window));
                    vtable.mouse_button_callback(button, action, mods);
                }
            );
            glfwSetKeyCallback(
                window,
                [](GLFWwindow *window, i32 key, i32 code, i32 action, i32 mods)
                {
                    auto &vtable = *reinterpret_cast<WindowVTable *>(glfwGetWindowUserPointer(window));
                    vtable.key_callback(key, code, action, mods);
                }
            );
            glfwSetFramebufferSizeCallback( 
                window, 
                [](GLFWwindow *window, i32 x, i32 y)
                { 
                    auto &vtable = *reinterpret_cast<WindowVTable *>(glfwGetWindowUserPointer(window));
                    vtable.window_resized_callback(x, y); 
                }
            );
        }

        i32 get_key_state(i32 key) { return glfwGetKey(window, key); }
        void set_window_close() { glfwSetWindowShouldClose(window, true); }
        void set_input_mode(i32 mode, i32 value) { glfwSetInputMode(window, mode, value); }
        bool get_window_should_close() { return glfwWindowShouldClose(window); }

        auto get_native_handle() -> daxa::NativeWindowHandle 
        {
#if defined(_WIN32)
            return glfwGetWin32Window(window);
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
        u32vec2 dimensions;
        WindowVTable vtable;
};