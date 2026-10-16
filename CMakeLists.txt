cmake_minimum_required(VERSION 3.20)
project(shellinc VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
  set(PLATFORM_SRC src/platform_win.c)
else()
  set(PLATFORM_SRC src/platform_posix.c)
endif()

add_library(shellcore STATIC
  src/lexer.c
  src/parser.c
  src/builtins.c
  src/repl.c
  ${PLATFORM_SRC}
)
target_include_directories(shellcore PUBLIC include)

add_executable(shellinc src/main.c)
target_link_libraries(shellinc PRIVATE shellcore)
target_compile_definitions(shellinc PRIVATE SHELLINC_VERSION="${PROJECT_VERSION}")

enable_testing()

add_executable(test_lexer_parser tests/test_lexer_parser.c)
target_link_libraries(test_lexer_parser PRIVATE shellcore)
add_test(NAME lexer_parser COMMAND test_lexer_parser)

add_executable(test_builtins tests/test_builtins.c)
target_link_libraries(test_builtins PRIVATE shellcore)
add_test(NAME builtins COMMAND test_builtins)

foreach(tgt shellcore shellinc test_lexer_parser test_builtins)
  if(MSVC)
    target_compile_options(${tgt} PRIVATE /W4)
  else()
    target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()