cmake_minimum_required(VERSION 3.10)
project(3DGame)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML REQUIRED COMPONENTS window system)
find_package(OpenGL REQUIRED)

# Явно ищем GLU
find_library(GLU_LIBRARY NAMES GLU libGLU)
if(NOT GLU_LIBRARY)
    message(FATAL_ERROR "GLU library not found")
endif()

add_executable(3dgame main.cpp)

target_link_libraries(3dgame PRIVATE
    sfml-window sfml-system
    OpenGL::GL
    ${GLU_LIBRARY}
)
