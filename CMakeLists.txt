cmake_minimum_required(VERSION 3.0)

project(WEBSERVER)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-O2 -Wall -g")


file(GLOB_RECURSE SRCS 
    "./code/log/*.cpp" 
    "./code/pool/*.cpp" 
    "./code/timer/*.cpp" 
    "./code/http/*.cpp" 
    "./code/server/*.cpp" 
    "./code/buffer/*.cpp" 
    "./code/main.cpp"
)

add_executable(server ${SRCS})

target_link_libraries(server pthread mysqlclient)

# Clean rule
add_custom_target(clean-all
    COMMAND ${CMAKE_BUILD_TOOL} clean
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/${TARGET}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/Testing
    COMMENT "Clean all generated files."
)