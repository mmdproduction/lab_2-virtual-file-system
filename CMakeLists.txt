cmake_minimum_required(VERSION 3.16)

project(VirtualFileSystem VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(vfs_core STATIC
    Core/VirtualFileSystem/VirtualFileSystem.cpp
    Core/VirtualFileSystem/FileNameIndex.cpp
)

target_include_directories(vfs_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Core"
    "${CMAKE_CURRENT_SOURCE_DIR}/Extern/AVL/AVL"
)

target_sources(vfs_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Dictionary/IDictionary.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Dictionary/AVLDictionary.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Sequence/Sequence.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Sequence/DynamicArray/DynamicArray.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Sequence/ArraySequence/ArraySequence.hpp"
)

target_compile_features(vfs_core PUBLIC cxx_std_20)

add_executable(vfs_app App/main.cpp)
target_link_libraries(vfs_app PRIVATE vfs_core)

if(MSVC)
    target_compile_options(vfs_app PRIVATE /W4 /permissive-)
else()
    target_compile_options(vfs_app PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)

if(BUILD_TESTING)
    add_test(NAME vfs_core_smoke COMMAND vfs_app)
    set_tests_properties(vfs_core_smoke PROPERTIES
        PASS_REGULAR_EXPRESSION "player[.]png: 12 37"
    )
endif()
