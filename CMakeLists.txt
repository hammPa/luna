cmake_minimum_required(VERSION 3.15)
project(luna LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

set(SQLITECPP_RUN_CPPLINT OFF CACHE BOOL "" FORCE)
set(SQLITECPP_RUN_CPPCHECK OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/SQLiteCpp)

set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/FTXUI)

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
    src/telegram/telegram_file_sink.cpp
    src/auth/totp.cpp
    src/search/tavily_provider.cpp
    src/search/search_factory.cpp
    src/utils/log.cpp
    src/tui/tui_app.cpp
    src/tui/tui_prompter.cpp
    src/tui/tui_render.cpp
    src/tui/tui_view.cpp
    src/tui/tui_events.cpp
    src/tui/tui_markdown.cpp
    src/app/run_cli
    src/app/register_tools.cpp
)

target_include_directories(luna PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/src
)
# SYSTEM: header FTXUI tidak ikut memunculkan peringatan
target_include_directories(luna SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/FTXUI/include
)

target_link_libraries(luna PRIVATE
    CURL::libcurl
    SQLiteCpp
    ftxui::screen
    ftxui::dom
    ftxui::component
    Threads::Threads
)

if(MSVC)
    target_compile_options(luna PRIVATE /W4)
else()
    target_compile_options(luna PRIVATE -Wall -Wextra -Wpedantic)
endif()