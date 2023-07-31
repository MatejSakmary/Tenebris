#pragma once

#include <functional>

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
            window = glfwCreateWindow(dimensions.x, dimensions.y, "Tenebris", nullptr, nullptr);
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

#if defined(_WIN32)
            {
                auto hwnd = static_cast<HWND>(get_native_handle());
                BOOL value = TRUE;
                DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
                auto is_windows11_or_greater = []() -> bool {
                    using Fn_RtlGetVersion = void(WINAPI *)(OSVERSIONINFOEX *);
                    Fn_RtlGetVersion fn_RtlGetVersion = nullptr;
                    auto ntdll_dll = LoadLibrary(TEXT("ntdll.dll"));
                    if (ntdll_dll)
                        fn_RtlGetVersion = (Fn_RtlGetVersion)GetProcAddress(ntdll_dll, "RtlGetVersion");
                    auto version_info = OSVERSIONINFOEX{};
                    version_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
                    fn_RtlGetVersion(&version_info);
                    FreeLibrary(ntdll_dll);
                    return version_info.dwMajorVersion >= 10 && version_info.dwMinorVersion >= 0 && version_info.dwBuildNumber >= 22000;
                };
                if (!is_windows11_or_greater()) {
                    PostMessage(hwnd, WM_NCACTIVATE, FALSE, 0);
                    PostMessage(hwnd, WM_NCACTIVATE, TRUE, 0);
                    MSG msg;
                    while (PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE)) {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                    }
                }
            }
#endif
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