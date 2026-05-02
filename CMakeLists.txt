cmake_minimum_required(VERSION 3.15)

# Export compile commands for clangd/IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(SonicForgeDSP
    VERSION 0.1.0
    DESCRIPTION "High-performance real-time DSP library for modular synthesis"
    LANGUAGES CXX
)

# =============================================================================
# C++ Standard Configuration
# =============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =============================================================================
# Compiler Warnings and Flags
# =============================================================================
# Enable comprehensive warnings for better code quality
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
    )
    
    # Optimization flags for Release builds
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    if(SONICFORGE_OPTIMIZE_FOR_HOST)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native")
        message(STATUS "  Host optimization: ON (-march=native)")
    else()
        message(STATUS "  Host optimization: OFF (portable binary)")
    endif()
    
    # Debug flags
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
endif()

# Optional sanitizer support (requires libasan and libubsan)
option(SONICFORGE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer in Debug builds" OFF)
if(SONICFORGE_ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-fsanitize=address" HAVE_ASAN)
    check_cxx_compiler_flag("-fsanitize=undefined" HAVE_UBSAN)
    
    if(HAVE_ASAN AND HAVE_UBSAN)
        message(STATUS "Enabling AddressSanitizer and UndefinedBehaviorSanitizer")
        add_compile_options(-fsanitize=address,undefined)
        add_link_options(-fsanitize=address,undefined)
    else()
        message(WARNING "Sanitizers requested but not fully supported by compiler")
    endif()
endif()

# =============================================================================
# Build Options
# =============================================================================
option(SONICFORGE_BUILD_EXAMPLES "Build example programs" ON)
option(SONICFORGE_BUILD_TESTS "Build unit tests" ON)

# Fix #4: -march=native is fast on the build machine but produces binaries
# that may crash on older CPUs (missing AVX / SSE4 etc.).  Default OFF so
# distributed binaries are portable; developers can enable it locally.
option(SONICFORGE_OPTIMIZE_FOR_HOST
    "Tune the binary for the build machine's exact CPU (-march=native)"
    OFF
)

option(SONICFORGE_ENABLE_LTO
    "Enable link-time optimisation for Release builds (INTERPROCEDURAL_OPTIMIZATION)"
    ON
)

# =============================================================================
# Main Library Target
# =============================================================================
add_library(sonicforge
    src/oscillator.cpp
    src/state_variable_filter.cpp
    src/delayline.cpp
)

# Public include directory
target_include_directories(sonicforge
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Library properties
set_target_properties(sonicforge PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON
)

# =============================================================================
# Link-Time Optimization (LTO)
# =============================================================================
# Uses INTERPROCEDURAL_OPTIMIZATION_RELEASE so it activates only for Release
# on both single-config (Unix Makefiles/Ninja) and multi-config (Visual Studio)
# generators.  CheckIPOSupported guards against toolchains that don't have it.
if(SONICFORGE_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _ipo_ok OUTPUT _ipo_msg LANGUAGES CXX)
    if(_ipo_ok)
        set_target_properties(sonicforge PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        message(STATUS "  LTO:  ON (Release builds)")
    else()
        message(STATUS "  LTO:  not available (${_ipo_msg})")
    endif()
endif()

# =============================================================================
# WebAssembly Build (requires Emscripten toolchain)
# =============================================================================
option(SONICFORGE_BUILD_WEB "Build WebAssembly AudioWorklet module" OFF)

if(SONICFORGE_BUILD_WEB)
    add_subdirectory(web)
endif()

# =============================================================================
# Example Programs
# =============================================================================
if(SONICFORGE_BUILD_EXAMPLES)
    # Simple sine wave generator example
    add_executable(sine_example examples/sine_example.cpp)
    target_link_libraries(sine_example PRIVATE sonicforge)
    
    # WAV file writer example
    add_executable(wav_writer_example examples/wav_writer_example.cpp)
    target_link_libraries(wav_writer_example PRIVATE sonicforge)
endif()

# =============================================================================
# Unit Tests
# =============================================================================
if(SONICFORGE_BUILD_TESTS)
    # ── Fetch GoogleTest via CPM ────────────────────────────────────────────────
    # CPM is bootstrapped here (inside the tests guard) so that configuring the
    # core library without tests does NOT trigger any network activity.
    # INSTALL_GTEST=OFF  — do not install GTest headers/libs alongside the
    #                      project's own install target.
    # gtest_force_shared_crt=ON  — on MSVC, match the shared runtime used by
    #                              the rest of the project (avoids LNK4098).
    include(cmake/get_cpm.cmake)
    CPMAddPackage(
        NAME googletest
        GITHUB_REPOSITORY google/googletest
        VERSION 1.15.2
        OPTIONS
            "INSTALL_GTEST OFF"
            "gtest_force_shared_crt ON"
    )

    enable_testing()

    add_executable(oscillator_test
        tests/oscillator_test.cpp
        tests/new_modules_test.cpp
    )
    target_link_libraries(oscillator_test PRIVATE sonicforge GTest::gtest_main)

    # gtest_discover_tests runs the binary at build time to enumerate tests,
    # giving one CTest entry per TEST() case.  It cannot execute on a cross-
    # compile target, so fall back to static source parsing in that case.
    include(GoogleTest)
    if(CMAKE_CROSSCOMPILING)
        gtest_add_tests(TARGET oscillator_test)
    else()
        gtest_discover_tests(oscillator_test)
    endif()
endif()

# =============================================================================
# Installation
# =============================================================================
include(GNUInstallDirs)

# Install the library
install(TARGETS sonicforge
    EXPORT SonicForgeTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(DIRECTORY include/sonicforge
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install CMake config files
install(EXPORT SonicForgeTargets
    FILE SonicForgeTargets.cmake
    NAMESPACE SonicForge::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SonicForge
)

# Create pkg-config file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sonicforge.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/sonicforge.pc
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sonicforge.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# =============================================================================
# Summary
# =============================================================================
message(STATUS "")
message(STATUS "SonicForge DSP Configuration:")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build Examples: ${SONICFORGE_BUILD_EXAMPLES}")
message(STATUS "  Build Tests:    ${SONICFORGE_BUILD_TESTS} (GoogleTest via CPM)")
message(STATUS "  Host Opt:       ${SONICFORGE_OPTIMIZE_FOR_HOST}")
message(STATUS "  LTO:            ${SONICFORGE_ENABLE_LTO}")
message(STATUS "")
message(STATUS "  Build Web:      ${SONICFORGE_BUILD_WEB}")
