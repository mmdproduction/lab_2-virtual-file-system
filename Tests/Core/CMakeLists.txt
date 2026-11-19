# Use the existing vendored Google Test without accessing the network.
if(NOT TARGET GTest::gtest_main)
    if(EXISTS "${PROJECT_SOURCE_DIR}/Extern/GoogleTest/CMakeLists.txt")
        set(BUILD_GMOCK OFF CACHE BOOL "Build Google Mock" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "Install Google Test" FORCE)
        if(MSVC)
            set(gtest_force_shared_crt ON CACHE BOOL "Use shared MSVC runtime" FORCE)
        endif()
        add_subdirectory("${PROJECT_SOURCE_DIR}/Extern/GoogleTest"
                         "${CMAKE_CURRENT_BINARY_DIR}/GoogleTest" EXCLUDE_FROM_ALL)
    else()
        find_package(GTest CONFIG REQUIRED)
    endif()
endif()

add_executable(vfs_core_tests
    dynamic_array_test.cpp
    array_sequence_test.cpp
    avl_dictionary_test.cpp
    file_name_index_test.cpp
    user_manager_test.cpp
    vfs_operations_test.cpp
    access_test.cpp
    persistence_test.cpp
)
target_link_libraries(vfs_core_tests PRIVATE vfs_core GTest::gtest_main)
target_compile_features(vfs_core_tests PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(vfs_core_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(vfs_core_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GoogleTest)
gtest_discover_tests(vfs_core_tests
    TEST_PREFIX "Core."
    NO_PRETTY_VALUES
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS "core" TIMEOUT 15
)
