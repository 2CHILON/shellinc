# Runs one end-to-end case. CTest invokes it as:
#   cmake -DSHELLINC_EXE=<shell> -DTOOL_DIR=<helper dir> -DCASE=<path without extension>
#         -P run_case.cmake
#
# Files next to CASE:
#   <name>.in    stdin for the shell                       (required)
#   <name>.out   expected stdout                           (optional, default: empty)
#   <name>.err   expected stderr                           (optional, not checked if absent)
#   <name>.code  expected exit status                      (optional, default: 0)

cmake_minimum_required(VERSION 3.20)

foreach(var SHELLINC_EXE TOOL_DIR CASE)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "run_case.cmake: -D${var}=... is required")
  endif()
endforeach()

get_filename_component(case_name "${CASE}" NAME)

# Test scripts run the helper program by name, so put it on PATH.
if(WIN32)
  set(ENV{PATH} "${TOOL_DIR};$ENV{PATH}")
else()
  set(ENV{PATH} "${TOOL_DIR}:$ENV{PATH}")
endif()

# Every case runs in its own empty directory.
set(work_dir "${TOOL_DIR}/e2e-work/${case_name}")
file(REMOVE_RECURSE "${work_dir}")
file(MAKE_DIRECTORY "${work_dir}")

# Makes output from both platforms comparable: LF line endings, no prompts,
# no version banner, no leading or trailing blank space.
function(normalize text out_var)
  string(REPLACE "\r\n" "\n" text "${text}")
  string(REPLACE "shellinc> " "" text "${text}")
  string(REGEX REPLACE "^shellinc [0-9][0-9.]*\n" "" text "${text}")
  string(STRIP "${text}" text)
  set(${out_var} "${text}" PARENT_SCOPE)
endfunction()

execute_process(
  COMMAND "${SHELLINC_EXE}"
  INPUT_FILE "${CASE}.in"
  WORKING_DIRECTORY "${work_dir}"
  OUTPUT_VARIABLE actual_out
  ERROR_VARIABLE actual_err
  RESULT_VARIABLE actual_code
  TIMEOUT 30
)

set(failed FALSE)

# exit status
set(expected_code "0")
if(EXISTS "${CASE}.code")
  file(READ "${CASE}.code" expected_code)
  string(STRIP "${expected_code}" expected_code)
endif()
if(NOT "${actual_code}" STREQUAL "${expected_code}")
  message("exit status: expected '${expected_code}', got '${actual_code}'")
  set(failed TRUE)
endif()

# stdout
set(expected_out "")
if(EXISTS "${CASE}.out")
  file(READ "${CASE}.out" expected_out)
endif()
normalize("${expected_out}" expected_out)
normalize("${actual_out}" actual_out)
if(NOT "${actual_out}" STREQUAL "${expected_out}")
  message("stdout differs\n--- expected ---\n${expected_out}\n--- actual ---\n${actual_out}\n---")
  set(failed TRUE)
endif()

# stderr (only when a .err file exists)
if(EXISTS "${CASE}.err")
  file(READ "${CASE}.err" expected_err)
  normalize("${expected_err}" expected_err)
  normalize("${actual_err}" actual_err)
  if(NOT "${actual_err}" STREQUAL "${expected_err}")
    message("stderr differs\n--- expected ---\n${expected_err}\n--- actual ---\n${actual_err}\n---")
    set(failed TRUE)
  endif()
endif()

if(failed)
  message(FATAL_ERROR "end-to-end case '${case_name}' failed")
endif()