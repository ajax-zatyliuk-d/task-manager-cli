cmake_minimum_required(VERSION 3.16)
project(task_manager_cli LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_SAN "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0>=2.56)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/bin)

add_compile_options(-Wall -Wextra -Werror)
if(ENABLE_SAN)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

set(TASK_MANAGER_CORE_SOURCES
    src/task.c
    src/task_manager.c
    src/cli.c
)

foreach(core_target task_manager_core task_manager_core_testing)
    add_library(${core_target} STATIC ${TASK_MANAGER_CORE_SOURCES})
    target_include_directories(${core_target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(${core_target} PUBLIC PkgConfig::GLIB)
endforeach()

target_compile_definitions(task_manager_core_testing PUBLIC TASK_MANAGER_TESTING)

add_executable(task_manager main.c)
target_link_libraries(task_manager PRIVATE task_manager_core)

enable_testing()
foreach(test_name test_task test_task_manager test_cli)
    add_executable(${test_name} tests/${test_name}.c)
    target_link_libraries(${test_name} PRIVATE task_manager_core_testing)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
