cmake_minimum_required(VERSION 3.21...3.31)
project(cr)
set(CMAKE_CXX_STANDARD 98)
set(CMAKE_C_STANDARD 99)

include_directories(cr)

set(HOTRELOAD_LIB_NAME plugin)
set(HOTRELOAD_WATCH_DIR_POSIX "${PROJECT_SOURCE_DIR}/src")
set(HOTRELOAD_LIB_PATH_WIN "${CMAKE_BINARY_DIR}/${HOTRELOAD_LIB_NAME}.dll")
set(HOTRELOAD_LIB_PATH_MAC "${CMAKE_BINARY_DIR}/lib${HOTRELOAD_LIB_NAME}.so")
set(HOTRELOAD_WATCH_DIR "")
set(HOTRELOAD_LIB_PATH "")
set(HOTRELOAD_BIN_PATH "")

if (WIN32)
    string(REPLACE "/" "\\\\" HOTRELOAD_WATCH_DIR "${HOTRELOAD_WATCH_DIR_POSIX}")
    string(REPLACE "/" "\\\\" HOTRELOAD_LIB_PATH "${HOTRELOAD_LIB_PATH_WIN}")
    string(REPLACE "/" "\\\\" HOTRELOAD_BIN_PATH "${CMAKE_BINARY_DIR}")
elseif(APPLE)
    set(HOTRELOAD_WATCH_DIR ${HOTRELOAD_WATCH_DIR_POSIX})
    set(HOTRELOAD_LIB_PATH ${HOTRELOAD_LIB_PATH_MAC})
    set(HOTRELOAD_BIN_PATH ${CMAKE_BINARY_DIR})
endif()

add_compile_definitions(
    CR_DEBUG
    _CRT_SECURE_NO_WARNINGS

    HOTRELOAD_WATCH_DIR="${HOTRELOAD_WATCH_DIR}"
    HOTRELOAD_LIB_PATH="${HOTRELOAD_LIB_PATH}"
    HOTRELOAD_BUILD_COMMAND="cmake --build ${HOTRELOAD_BIN_PATH} --config Debug --target ${HOTRELOAD_LIB_NAME}"
    )

add_executable(hotreloader hotreload_win.c)
if(APPLE)
    target_link_libraries(hotreloader PRIVATE "-framework CoreServices")
endif()
add_library(${HOTRELOAD_LIB_NAME} MODULE src/main.c)
add_dependencies(hotreloader ${HOTRELOAD_LIB_NAME})