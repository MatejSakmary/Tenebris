#pragma once

#include <functional>

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_NATIVE_INCLUDE_NONE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <dwmapi.h>
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#endif
#include <GLFW/glfw3native.h>

const u32 WIDTH = 1920;
const u32 HEIGHT = 1080;

struct WindowVTable
{
    std::function<void(f64, f64)> mouse_callback;
    std::function<void(i32, i32)> window_resized_callback;
};

struct AppWindow
{
    public:
        AppWindow(std::function<void(f64, f64)> mouse_callback,
                  std::function<void(i32, i32)> window_resized_callback) :
                    v_table { .mouse_callback = mouse_callback, .window_resized_callback = window_resized_callback} 
        {
            glfwInit();
            /* Tell GLFW to not create OpenGL context */
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
            window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
            glfwSetWindowUserPointer(window, &v_table);
            glfwSetCursorPosCallback( 
                window,
                [](GLFWwindow *window, f64 x, f64 y)
                { 
                    auto &v_table = *reinterpret_cast<WindowVTable *>(glfwGetWindowUserPointer(window));
                    v_table.mouse_callback(x, y); 
                }
            );
            glfwSetFramebufferSizeCallback( 
                window, 
                [](GLFWwindow *window, i32 x, i32 y)
                { 
                    auto &v_table = *reinterpret_cast<WindowVTable *>(glfwGetWindowUserPointer(window));
                    v_table.window_resized_callback(x, y); 
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
            return glfwGetWin32Window(glfw_window_ptr);
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
        WindowVTable v_table;
        GLFWwindow* window;
};