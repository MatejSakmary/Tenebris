cmake_minimum_required(VERSION 3.21)

project(tenebris)

add_executable( ${PROJECT_NAME} 
    "source/main.cpp"
    "source/application.cpp"
    "source/camera.cpp"
    "source/gui_manager.cpp"
    "source/renderer/renderer.cpp"
    "source/terrain_gen/planet_generator.cpp"
    "source/renderer/texture_manager.cpp")

set(CDT_USE_AS_COMPILED_LIBRARY "ON")
add_subdirectory("external/CDT")

find_package(daxa CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(OpenEXR CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

message("${CMAKE_SYSTEM_NAME} - ${CMAKE_CXX_COMPILER_ID}")
if(CMAKE_SYSTEM_NAME MATCHES "Windows" AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message("Hack disabling interface compiler options on OpenEXR targets for clang to work")
    set_target_properties(OpenEXR::Iex PROPERTIES INTERFACE_COMPILE_OPTIONS "")
    set_target_properties(OpenEXR::IlmThread PROPERTIES INTERFACE_COMPILE_OPTIONS "")
    set_target_properties(OpenEXR::OpenEXR PROPERTIES INTERFACE_COMPILE_OPTIONS "")
    set_target_properties(OpenEXR::OpenEXRCore PROPERTIES INTERFACE_COMPILE_OPTIONS "")
    set_target_properties(OpenEXR::OpenEXRUtil PROPERTIES INTERFACE_COMPILE_OPTIONS "")
    set_target_properties(Imath::Imath PROPERTIES INTERFACE_COMPILE_OPTIONS "")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE "external/")
target_link_libraries(${PROJECT_NAME} PRIVATE
    imgui::imgui
    nlohmann_json::nlohmann_json    
    daxa::daxa
    OpenEXR::OpenEXR
    glm::glm
    glfw
    CDT::CDT
)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(${PROJECT_NAME} PRIVATE Dwmapi)
endif()
# Debug mode defines
target_compile_definitions(${PROJECT_NAME} PRIVATE "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:LOG_DEBUG>")
target_compile_definitions(${PROJECT_NAME} PRIVATE "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:__DEBUG__>")
