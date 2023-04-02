cmake_minimum_required(VERSION 3.21)

project(atmosphere)


add_executable( ${PROJECT_NAME} 
    "source/main.cpp"
    "source/application.cpp"
    "source/camera.cpp"
    "source/renderer/renderer.cpp"
    "source/terrain_gen/planet_generator.cpp"
)

set(CDT_USE_AS_COMPILED_LIBRARY "ON")
add_subdirectory("external/CDT")

find_package(daxa CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_path(STB_INCLUDE_DIRS "stb_c_lexer.h")

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_include_directories(${PROJECT_NAME} PRIVATE ${STB_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE "external/")
target_link_libraries(${PROJECT_NAME} PRIVATE
    imgui::imgui
    daxa::daxa
    glm::glm
    glfw
    CDT::CDT
)

# Debug mode defines
target_compile_definitions(${PROJECT_NAME} PRIVATE "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:LOG_DEBUG>")

# if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
#     if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
#         target_link_options(${PROJECT_NAME} PRIVATE "-Wl,/ENTRY:mainCRTStartup,/SUBSYSTEM:WINDOWS")
#     else()
#         target_link_options(${PROJECT_NAME} PRIVATE /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWS)
#     endif()
# endif()