cmake_minimum_required(VERSION 3.21)

project(accessible_cache_repro LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets)

option(ACCESSIBLE_REPRO_ASAN
       "Build the reproducers with AddressSanitizer (recommended)" OFF)

function(add_repro target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
    target_compile_options(${target} PRIVATE -g -O0 -fno-omit-frame-pointer)
    if(ACCESSIBLE_REPRO_ASAN AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE -fsanitize=address)
        target_link_options(${target} PRIVATE -fsanitize=address)
    endif()
endfunction()

# Deterministic, platform-independent proxy for the re-entrant Cocoa callback.
add_repro(cache_reentry cache_reentry.cpp)

# The same re-entrancy, reached through a real QTableView and a fictional model.
add_repro(itemview_reentry itemview_reentry.cpp)

# Second crash path: QAccessibleTable::modelChange() and QTreeView reset/expand.
add_repro(treeview_reentry treeview_reentry.cpp)
