cmake_minimum_required(VERSION 3.10)
project(3DGame)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML REQUIRED COMPONENTS window system)

add_executable(3dgame main.cpp)
target_link_libraries(3dgame PRIVATE sfml-window sfml-system ${OPENGL_LIBRARIES})l
