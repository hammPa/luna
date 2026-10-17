cmake_minimum_required(VERSION 3.15)
project(luna LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)

set(SQLITECPP_RUN_CPPLINT OFF CACHE BOOL "" FORCE)
set(SQLITECPP_RUN_CPPCHECK OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/SQLiteCpp)

add_executable(luna
    src/main.cpp
    src/utils/http.cpp
    src/utils/config.cpp
    src/utils/string_utils.cpp
    src/llm/openai_backend.cpp
    src/llm/backend_factory.cpp
    src/memory/memory.cpp
    src/app/commands.cpp
    src/app/chat_session.cpp
    src/tools/tool_registry.cpp
    src/tools/permission.cpp
    src/tools/prompter.cpp
    src/tools/path_guard.cpp
    src/telegram/telegram_client.cpp
    src/telegram/telegram_loop.cpp
    src/telegram/telegram_prompter.cpp
    src/auth/totp.cpp
)

target_include_directories(luna PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(luna PRIVATE CURL::libcurl SQLiteCpp)

if(MSVC)
    target_compile_options(luna PRIVATE /W4)
else()
    target_compile_options(luna PRIVATE -Wall -Wextra -Wpedantic)
endif()