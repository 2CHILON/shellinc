cmake_minimum_required(VERSION 3.20)
project(shellinc VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(shellinc src/main.c)
target_include_directories(shellinc PRIVATE include)
target_compile_definitions(shellinc PRIVATE SHELLINC_VERSION="${PROJECT_VERSION}")

if(MSVC)
  target_compile_options(shellinc PRIVATE /W4)
else()
  target_compile_options(shellinc PRIVATE -Wall -Wextra -Wpedantic)
endif()