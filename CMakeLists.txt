cmake_minimum_required(VERSION 3.16)
project(FpsChecker)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(FpsChecker main.cpp)

target_link_libraries(FpsChecker OpenGL::GL glfw)
if(WIN32)
    target_link_libraries(FpsChecker opengl32)
endif()
