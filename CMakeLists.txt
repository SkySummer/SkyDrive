cmake_minimum_required(VERSION 3.13)
project(SkyDrive LANGUAGES CXX)

# 自动创建源码目录下的 data 和 data/files 文件夹
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/data")
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/data/files")

# 设置全局 C++ 标准为 C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# clang-tidy 检查
find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
if (CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--format-style=file")
endif ()

# 定义源代码和头文件目录
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
set(INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")

# 递归收集所有源文件
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "${SRC_DIR}/*.cpp")

add_compile_definitions(ROOT_PATH=${CMAKE_SOURCE_DIR})

# 定义可执行文件目标
add_executable(SkyDrive main.cpp ${SRC_FILES})

# 设置头文件包含目录
target_include_directories(SkyDrive PRIVATE ${INCLUDE_DIR})

# 启用常见警告、额外警告和标准严格检查
target_compile_options(SkyDrive PRIVATE -Wall -Wextra -Wpedantic)
