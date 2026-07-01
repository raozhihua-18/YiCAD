# 设置cmake 最低版本
cmake_minimum_required (VERSION 3.21)

# 设置项目名称
project (YiCAD)

# Include GNUInstallDirs for standard install directories
include(GNUInstallDirs)

set(PACKAGE_VERSION_MAJOR "0")
set(PACKAGE_VERSION_MINOR "20")
set(PACKAGE_VERSION_PATCH "0")
set(PACKAGE_VERSION_SUFFIX "dev") # either "dev" for development snapshot or "" (empty string)
set(YICAD_VERSION_PATCH "0") # number of patch release (e.g. "4" for the 0.18.4 release)
set(YICAD_VERSION "${PACKAGE_VERSION_MAJOR}.${PACKAGE_VERSION_MINOR}")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakePredefinedTargets")

# Add Path for searching cmake files
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmakeutil.cmake)

# All third-party dependencies are resolved via Conan 2 (see cmake/dependencies.cmake).
# Qt 5.15, SARibbonBar, and CDT are discovered via find_package from user-provided paths.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/deployqt.cmake)

# Where binaries should be placed. Multi-config generators such as Visual
# Studio append the configuration name unless each config is set explicitly.
set(YICAD_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${YICAD_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${YICAD_OUTPUT_DIRECTORY})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${YICAD_OUTPUT_DIRECTORY})
set(_yicad_output_configs Debug Release RelWithDebInfo MinSizeRel)
if(CMAKE_CONFIGURATION_TYPES)
    list(APPEND _yicad_output_configs ${CMAKE_CONFIGURATION_TYPES})
endif()
list(REMOVE_DUPLICATES _yicad_output_configs)
foreach(_yicad_config IN LISTS _yicad_output_configs)
    string(TOUPPER "${_yicad_config}" _yicad_config_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_yicad_config_upper} ${YICAD_OUTPUT_DIRECTORY})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_yicad_config_upper} ${YICAD_OUTPUT_DIRECTORY})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_yicad_config_upper} ${YICAD_OUTPUT_DIRECTORY})
endforeach()
unset(_yicad_config_upper)
unset(_yicad_config)
unset(_yicad_output_configs)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Define compiler warnings
if (NOT MSVC) # Too much warnings on MSVC
     add_definitions("-Wall")
endif()

# 设置 std 和 c++版本为
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)

# Boost is consumed through Conan (header-only). See cmake/dependencies.cmake.

# settings path
set(SETTINGS_PATH "${PROJECT_BINARY_DIR}/bin")

#disable warnings
#4819 non Unicode format warning
if(MSVC)
     add_compile_options(/MP /W3 /wd4819 /Zc:inline)
endif()

OPTION(UPDATE_TRANSLATIONS "Update translations on compile" OFF)
MESSAGE(STATUS "UPDATE TRANSLATIONS: ${UPDATE_TRANSLATIONS}")
option(USE_VISUAL_LEAK_DETECTOR "Use visual leak detector to detect memory leakage" OFF)

# 设置子项目
add_subdirectory("YiCAD")
add_subdirectory("examples/demo_plugin")

# Install third-party DLLs
# Helper function to install DLLs from a directory
function(install_third_party_dlls)
    if(NOT WIN32)
        return()
    endif()

    # Install SARibbonBar DLLs
    if(SARibbonBar_DLL_RELEASE)
        install(FILES "${SARibbonBar_DLL_RELEASE}"
                DESTINATION ${CMAKE_INSTALL_BINDIR}
                CONFIGURATIONS Release RelWithDebInfo MinSizeRel)
    endif()
    if(SARibbonBar_DLL_DEBUG)
        install(FILES "${SARibbonBar_DLL_DEBUG}"
                DESTINATION ${CMAKE_INSTALL_BINDIR}
                CONFIGURATIONS Debug)
    endif()

    # Install CDT DLLs
    if(CDT_DIR)
        find_file(CDT_DLL_RELEASE
            NAMES CDT.dll
            HINTS ${CDT_DIR}/../../bin ${CDT_DIR}/../bin ${CDT_DIR}/bin
            PATH_SUFFIXES Release
            NO_DEFAULT_PATH
        )
        find_file(CDT_DLL_DEBUG
            NAMES CDTd.dll CDT.dll
            HINTS ${CDT_DIR}/../../bin ${CDT_DIR}/../bin ${CDT_DIR}/bin
            PATH_SUFFIXES Debug
            NO_DEFAULT_PATH
        )

        if(CDT_DLL_RELEASE)
            message(STATUS "CDT DLL (Release): ${CDT_DLL_RELEASE}")
            install(FILES "${CDT_DLL_RELEASE}"
                    DESTINATION ${CMAKE_INSTALL_BINDIR}
                    CONFIGURATIONS Release RelWithDebInfo MinSizeRel)
        endif()
        if(CDT_DLL_DEBUG)
            message(STATUS "CDT DLL (Debug): ${CDT_DLL_DEBUG}")
            install(FILES "${CDT_DLL_DEBUG}"
                    DESTINATION ${CMAKE_INSTALL_BINDIR}
                    CONFIGURATIONS Debug)
        endif()
    endif()

    # The YiCAD target install rule copies the deployed runtime bundle from
    # the runtime output directory. Do not copy ${CMAKE_BINARY_DIR}/bin here:
    # if the install prefix is placed below that directory, CMake recursively
    # installs the destination into itself.
endfunction()

# Call the install function
install_third_party_dlls()

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        DESTINATION ${CMAKE_INSTALL_BINDIR}/licenses)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/licenses/"
        DESTINATION ${CMAKE_INSTALL_BINDIR}/licenses
        FILES_MATCHING PATTERN "*.txt")
