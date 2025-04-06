cmake_minimum_required(VERSION 3.21...3.31)
project(cr)
set(CMAKE_CXX_STANDARD 98)

include_directories(cr)

set(HOTRELOAD_LIB_NAME plugin)
set(HOTRELOAD_WATCH_DIR_POSIX "${PROJECT_SOURCE_DIR}/src")
set(HOTRELOAD_LIB_PATH_POSIX "${CMAKE_BINARY_DIR}/${HOTRELOAD_LIB_NAME}.dll")
set(HOTRELOAD_WATCH_DIR "")
set(HOTRELOAD_LIB_PATH "")

if (WIN32)
    string(REPLACE "/" "\\\\" HOTRELOAD_WATCH_DIR "${HOTRELOAD_WATCH_DIR_POSIX}")
    string(REPLACE "/" "\\\\" HOTRELOAD_LIB_PATH "${HOTRELOAD_LIB_PATH_POSIX}")
endif()

add_compile_definitions(
    CR_DEBUG
    _CRT_SECURE_NO_WARNINGS

    HOTRELOAD_WATCH_DIR="${HOTRELOAD_WATCH_DIR}"
    HOTRELOAD_LIB_PATH="${HOTRELOAD_LIB_PATH}"
    HOTRELOAD_BUILD_COMMAND="cmake --build ${CMAKE_BINARY_DIR} --config Debug --target ${HOTRELOAD_LIB_NAME}"
    )

add_executable(hotreloader hotreload_win.c)
add_library(${HOTRELOAD_LIB_NAME} MODULE src/main.c)
add_dependencies(hotreloader ${HOTRELOAD_LIB_NAME})