cmake_minimum_required(VERSION 3.15)
project(luna LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)

add_executable(luna
    src/main.cpp
    src/utils/http.cpp
    src/llm/openai_backend.cpp
)

target_include_directories(luna PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(luna PRIVATE CURL::libcurl)

if(MSVC)
    target_compile_options(luna PRIVATE /W4)
else()
    target_compile_options(luna PRIVATE -Wall -Wextra -Wpedantic)
endif()