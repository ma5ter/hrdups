cmake_minimum_required(VERSION 3.20)
project(hrdups)

set(CMAKE_CXX_STANDARD 17)

add_executable(hrdups hrdups.cpp)

target_link_libraries(hrdups PRIVATE
        crypto
        )