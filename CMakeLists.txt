cmake_minimum_required(VERSION 3.16)

project(VirtualFileSystem VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(vfs_core STATIC
    Core/Common/Guid.cpp
    Core/Project/ProjectCodec.cpp
    Core/Authorization/AuthorizationService.cpp
    Core/Authorization/AuthorizationGraph.cpp
    Core/Project/ProjectAdministration.cpp
    Core/Project/ProjectNavigation.cpp
    Core/Authorization/AdminGrantStore.cpp
    Core/Project/ProjectState.cpp
    Core/Project/ProjectService.cpp
    Core/VirtualFileSystem/VirtualFileSystem.cpp
    Core/VirtualFileSystem/FileNameIndex.cpp
    Core/VirtualFileSystem/UserManager.cpp
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

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(vfs_gui_widgets STATIC
    App/Gui/VfsTreeModel.cpp
    App/Gui/MainWindow.cpp
    App/Gui/AccountListModel.cpp
    App/Gui/AccessRuleModel.cpp
    App/Gui/AdminGrantModel.cpp
    App/Gui/AdminAccounts.cpp
    App/Gui/AdminDialog.cpp
    App/Gui/UserChoiceModel.cpp
    App/Gui/FileIconDialog.cpp
    App/Gui/AutosaveController.cpp
    App/Gui/ProjectStore.cpp
    App/Gui/FileOpener.cpp
    App/Gui/FileSearchModel.cpp
    App/Gui/SearchDialog.cpp
)
target_include_directories(vfs_gui_widgets PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/App")
target_link_libraries(vfs_gui_widgets PUBLIC vfs_core Qt6::Widgets)

add_executable(vfs_gui App/gui_main.cpp)
target_link_libraries(vfs_gui PRIVATE vfs_gui_widgets)

target_compile_options(vfs_gui PRIVATE -Wall -Wextra -Wpedantic)