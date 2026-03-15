cmake_minimum_required(VERSION 3.20)
project(BricsCAD_BatchProcessing_Windows)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ============================================================================
# Windows-Pfade definieren
# ============================================================================
set(QT6_DIR "C:/Qt/6.8.3/msvc2022_64")
set(BRX_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/brx_sdk")
set(BRX_INC_DIR "${BRX_SDK_PATH}/inc")
set(BRX_LIB_DIR "${BRX_SDK_PATH}/lib64")
set(BRICSCAD_DIR "C:/Program Files/Bricsys/BricsCAD V26 de_DE")

# Prüfungen
if(NOT EXISTS ${BRX_SDK_PATH})
    message(FATAL_ERROR "BRX SDK not found at ${BRX_SDK_PATH}")
endif()

if(NOT EXISTS ${QT6_DIR})
    message(FATAL_ERROR "Qt6 not found at ${QT6_DIR}")
endif()

if(NOT EXISTS ${BRICSCAD_DIR})
    message(FATAL_ERROR "BricsCAD V26 not found at ${BRICSCAD_DIR}")
endif()

message(STATUS "✅ Qt6: ${QT6_DIR}")
message(STATUS "✅ BRX SDK: ${BRX_SDK_PATH}")
message(STATUS "✅ BricsCAD V26: ${BRICSCAD_DIR}")

# ============================================================================
# Qt6 Setup
# ============================================================================
set(CMAKE_PREFIX_PATH "${QT6_DIR}")
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ============================================================================
# Windows Fix Header - manually maintained for Qt 6.8+ compatibility
# ============================================================================
set(WINDOWS_FIX_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/src/windows_fix.h")
if(NOT EXISTS "${WINDOWS_FIX_HEADER}")
    message(FATAL_ERROR "windows_fix.h not found at ${WINDOWS_FIX_HEADER}")
endif()
message(STATUS "✅ Using manual windows_fix.h: ${WINDOWS_FIX_HEADER}")

# ============================================================================
# Include Directories
# ============================================================================
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src  # For windows_fix.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfc_stubs  # Empty MFC/ATL stubs for Qt-based plugin (no MFC needed)
    ${BRX_INC_DIR}
    # CRITICAL: Platform/substitutes contains EMPTY stubs for windows.h, intrin.h, windef.h
    # These are ONLY for Linux cross-compilation and MUST NOT be used on Windows!
    # They shadow the real Windows SDK headers and cause all CALLBACK/HANDLE/intrinsics errors.
    # ${BRX_INC_DIR}/Platform/substitutes  # DISABLED ON WINDOWS!
    ${BRX_INC_DIR}/Platform/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/data
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin
)

# ============================================================================
# Source Files
# ============================================================================
set(PLUGIN_SOURCES
    src/plugin/BatchProcessingPlugin.cpp
    src/plugin/Commands.cpp
    src/core/DwgProcessor.cpp
    src/core/LispProcessExecutor.cpp
    src/data/ProcessingOptionsImpl.cpp
    src/ui/MainWindow.cpp
    src/ui/OpenCirtTab.cpp
    src/ui/widgets/AcadColorGrid.cpp
)

set(PLUGIN_HEADERS
    src/core/DwgProcessor.h
    src/core/LispProcessExecutor.h
    src/data/ProcessingOptions.h
    src/ui/MainWindow.h
    src/ui/OpenCirtTab.h
    src/ui/widgets/AcadColorGrid.h
)

# ============================================================================
# BricsCAD Libraries
# ============================================================================
find_library(BRX26_LIB NAMES brx26.lib PATHS ${BRX_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
find_library(BRXHLRAPI26_LIB NAMES BrxHlrApi26.lib PATHS ${BRX_LIB_DIR} NO_DEFAULT_PATH)
find_library(BRXATIL26_LIB NAMES BrxATIL26.lib PATHS ${BRX_LIB_DIR} NO_DEFAULT_PATH)

message(STATUS "✅ BRX26: ${BRX26_LIB}")
if(BRXHLRAPI26_LIB)
    message(STATUS "✅ BRXHLRAPI26: ${BRXHLRAPI26_LIB}")
endif()
if(BRXATIL26_LIB)
    message(STATUS "✅ BRXATIL26: ${BRXATIL26_LIB}")
endif()

# ============================================================================
# Target
# ============================================================================
add_library(batch_processing_plugin SHARED 
    ${PLUGIN_SOURCES}
    ${PLUGIN_HEADERS}
)

# ============================================================================
# Compiler Definitions
# ============================================================================
target_compile_definitions(batch_processing_plugin PRIVATE
    _BRXEXPORT
    __BRXTARGET
    _UNICODE
    UNICODE
    BRXAPI
    WINDOWS
    _WINDOWS
    WIN64
    _WIN64
    NOMINMAX
)

# ============================================================================
# CRITICAL: Force-include windows_fix.h for ALL compilations (including MOC)
# ============================================================================
if(MSVC)
    target_compile_options(batch_processing_plugin PRIVATE
        /W3
        /MP
        /EHsc
        /permissive-
        /Zc:__cplusplus
        /wd4251
        /wd4275
        /FI"${CMAKE_CURRENT_SOURCE_DIR}/src/windows_fix.h"
    )
    
    set_target_properties(batch_processing_plugin PROPERTIES
        VS_PLATFORM_TOOLSET "v143"
    )
    
    message(STATUS "")
    message(STATUS "🔧 ===============================================")
    message(STATUS "🔧 Qt 6.8+ Windows Header Fix ACTIVE")
    message(STATUS "🔧 Method: /FI force-include on TARGET level")
    message(STATUS "🔧 Applies to: ALL files including MOC-generated")
    message(STATUS "🔧 ===============================================")
    message(STATUS "")
endif()

# ============================================================================
# Link Libraries
# ============================================================================
set(BRX_LIBRARIES ${BRX26_LIB})
if(BRXHLRAPI26_LIB)
    list(APPEND BRX_LIBRARIES ${BRXHLRAPI26_LIB})
endif()
if(BRXATIL26_LIB)
    list(APPEND BRX_LIBRARIES ${BRXATIL26_LIB})
endif()

# Add BRX lib directory to linker search path (for #pragma comment(lib,...) in BRX headers)
target_link_directories(batch_processing_plugin PRIVATE
    ${BRX_LIB_DIR}
)

target_link_libraries(batch_processing_plugin PRIVATE
    ${BRX_LIBRARIES}
    Qt6::Core
    Qt6::Widgets
)

# ============================================================================
# Output Configuration
# ============================================================================
set_target_properties(batch_processing_plugin PROPERTIES
    OUTPUT_NAME "batchtool"
    SUFFIX ".brx"
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
)

# ============================================================================
# Status
# ============================================================================
message(STATUS "")
message(STATUS "🎯 ==========================================")
message(STATUS "🎯 BricsCAD BatchProcessing Plugin - WINDOWS")
message(STATUS "🎯 ==========================================")
message(STATUS "📦 Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "📁 Qt6: ${QT6_DIR}")
message(STATUS "📁 BRX SDK: ${BRX_SDK_PATH}")
message(STATUS "🎯 Output: batchtool.brx")
message(STATUS "🎯 ==========================================")
message(STATUS "")

# Post-build message
add_custom_command(TARGET batch_processing_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "🎉 Build complete: batchtool.brx"
    COMMAND ${CMAKE_COMMAND} -E echo "🔧 Load with: APPLOAD in BricsCAD V26"
    COMMAND ${CMAKE_COMMAND} -E echo "✅ Windows MSVC 2022 Build - Qt 6.8+ compatible"
)
