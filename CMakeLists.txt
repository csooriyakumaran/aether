cmake_minimum_required(VERSION 3.25)

# Project configuration ---
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PROJECT_VERSION_STRING)
string(STRIP "${PROJECT_VERSION_STRING}" PROJECT_VERSION_STRING)

string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+(\\.[0-9]+)?" PROJECT_VERSION_NUMERIC "${PROJECT_VERSION_STRING}")

if (NOT PROJECT_VERSION_NUMERIC)
    message(FATAL_ERROR "Invalid VERSION file format: '${PROJECT_VERSION_STRING}'")
endif()

project(aether
    VERSION ${PROJECT_VERSION_NUMERIC}
    LANGUAGES C
)

option(AETHER_BUILD_EXAMPLES "Build AETHER examples" OFF)
option(AETHER_BUILD_TESTS "Build AETHER test" OFF)
option(AETHER_BUILD_INSTALL "Enable install rules" OFF)


add_library(aether INTERFACE)
add_library(aether::aether ALIAS aether)

target_compile_features(aether INTERFACE c_std_99)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/aether/aether-version.h
    @ONLY
)

target_include_directories(aether
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(AETHER_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(AETHER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if (AETHER_BUILD_INSTALL)
    include(GNUInstallDirs)

    install(DIRECTORY include/aether DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/aether/aether-version.h
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/aether
    )
    install(FILES README.md LICENSE VERSION DESTINATION .)
endif()
