cmake_minimum_required(VERSION 3.16)

# Project Information
project(QuantilyxDoc 
    VERSION 1.0.0
    DESCRIPTION "Professional open-source document editor for Linux"
    LANGUAGES CXX
)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_PLUGINS "Build optional plugins" ON)
option(BUILD_WEB_INTERFACE "Build web interface" ON)
option(ENABLE_OCR_TESSERACT "Enable Tesseract OCR support" ON)
option(ENABLE_OCR_PADDLEOCR "Enable PaddleOCR support" OFF)
option(ENABLE_GPU_ACCELERATION "Enable GPU acceleration" ON)
option(BUILD_LEGACY "Build for older systems (Debian 9)" OFF)

# Build configuration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# CMake module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Werror=return-type
        -Wno-unused-parameter
    )
    
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    else()
        add_compile_options(-O3 -DNDEBUG)
    endif()
endif()

# Find required packages
find_package(Qt5 5.12 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    WebSockets
    PrintSupport
    Svg
    Concurrent
)

# Find Poppler for PDF support
find_package(Poppler REQUIRED COMPONENTS Qt5)

# Find other required libraries
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)

# Optional packages
if(ENABLE_OCR_TESSERACT)
    find_package(Tesseract)
    if(Tesseract_FOUND)
        add_definitions(-DHAVE_TESSERACT)
    endif()
endif()

if(ENABLE_OCR_PADDLEOCR)
    find_package(PaddleOCR)
    if(PaddleOCR_FOUND)
        add_definitions(-DHAVE_PADDLEOCR)
    endif()
endif()

# Check for GPU support
if(ENABLE_GPU_ACCELERATION)
    find_package(OpenGL)
    if(OpenGL_FOUND)
        add_definitions(-DHAVE_OPENGL)
    endif()
endif()

# Version configuration
configure_file(
    "${CMAKE_SOURCE_DIR}/src/utils/Version.h.in"
    "${CMAKE_BINARY_DIR}/generated/Version.h"
    @ONLY
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
    ${Qt5Core_INCLUDE_DIRS}
    ${Qt5Gui_INCLUDE_DIRS}
    ${Qt5Widgets_INCLUDE_DIRS}
)

# Add subdirectories
add_subdirectory(src)
add_subdirectory(resources)

if(BUILD_PLUGINS)
    add_subdirectory(plugins)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_WEB_INTERFACE)
    add_subdirectory(web-interface)
endif()

# Installation
install(TARGETS quantilyxdoc
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY resources/translations
    DESTINATION share/quantilyxdoc
)

install(DIRECTORY resources/themes
    DESTINATION share/quantilyxdoc
)

install(FILES packaging/desktop/quantilyxdoc.desktop
    DESTINATION share/applications
)

install(FILES resources/images/QuantilyxDoc.png
    DESTINATION share/icons/hicolor/512x512/apps
    RENAME quantilyxdoc.png
)

# CPack configuration for packaging
set(CPACK_PACKAGE_NAME "quantilyxdoc")
set(CPACK_PACKAGE_VENDOR "R² Innovative Software")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Professional document editor")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "QuantilyxDoc")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

# DEB package
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "R² Innovative Software")
set(CPACK_DEBIAN_PACKAGE_SECTION "editors")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt5core5a, libqt5gui5, libqt5widgets5, libpoppler-qt5-1")

# RPM package
set(CPACK_RPM_PACKAGE_LICENSE "GPLv3")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Editors")
set(CPACK_RPM_PACKAGE_REQUIRES "qt5-qtbase, poppler-qt5")

include(CPack)

# Print configuration summary
message(STATUS "")
message(STATUS "QuantilyxDoc v${PROJECT_VERSION} Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Features:")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "  Build plugins: ${BUILD_PLUGINS}")
message(STATUS "  Build web interface: ${BUILD_WEB_INTERFACE}")
message(STATUS "  Tesseract OCR: ${Tesseract_FOUND}")
message(STATUS "  PaddleOCR: ${PaddleOCR_FOUND}")
message(STATUS "  GPU acceleration: ${OpenGL_FOUND}")
message(STATUS "  Legacy build: ${BUILD_LEGACY}")
message(STATUS "")
message(STATUS "Dependencies:")
message(STATUS "  Qt5 version: ${Qt5Core_VERSION}")
message(STATUS "  Poppler-Qt5: ${Poppler_FOUND}")
message(STATUS "  OpenSSL: ${OPENSSL_VERSION}")
message(STATUS "")