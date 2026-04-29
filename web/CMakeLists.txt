# web/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(SonicForgeWeb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─────────────────────────────────────────────────────────────────────────────
# Guard: the Wasm executable requires the Emscripten toolchain (emcmake cmake).
# When this file is included from the root CMakeLists.txt with a native GCC/Clang
# toolchain, skip the Wasm target and emit an informational message so the rest
# of the build (tests, examples) continues normally.
# ─────────────────────────────────────────────────────────────────────────────
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    message(STATUS "  [Web] Emscripten toolchain not active — Wasm target skipped.")
    message(STATUS "  [Web] To build sonicforge.wasm run:")
    message(STATUS "  [Web]   cd web && emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Release")
    message(STATUS "  [Web]           cmake --build build-web")
    return()
endif()

# Output Wasm directly to public/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/public)

# SonicForge core library (Emscripten build — compiles all DSP modules for Wasm)
add_library(sonicforge_web STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/oscillator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/state_variable_filter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/delayline.cpp
)
target_include_directories(sonicforge_web PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Wasm module: Raw WebAssembly binary (no JS glue).
# The target is named "sonicforge_worklet" to avoid clashing with the static
# library target "sonicforge" defined in the root CMakeLists.txt.
# OUTPUT_NAME ensures the file on disk is still "sonicforge.wasm" so the
# existing fetch('sonicforge.wasm') calls in the web demo need no changes.
add_executable(sonicforge_worklet src/sonicforge_worklet.cpp)
target_link_libraries(sonicforge_worklet PRIVATE sonicforge_web)

set_target_properties(sonicforge_worklet PROPERTIES
    OUTPUT_NAME "sonicforge"
    SUFFIX ".wasm"
    # --no-entry        : No main() entry point.
    # STANDALONE_WASM   : Standard .wasm without Emscripten JS startup glue.
    # EXPORTED_FUNCTIONS: Every C export that the AudioWorklet processor or
    #                     the main-thread visualisation instance may call.
    #
    # Audio / oscillator engine:
    #   _get_buffer, _audio_init, _audio_process, _audio_set_frequency,
    #   _audio_set_waveform, _audio_set_harmonics, _audio_destroy
    #
    # Visualisation (main-thread Wasm instance):
    #   _get_viz_buffer, _get_viz_buffer_capacity, _viz_render_wavetable
    #
    # FX chain — StateVariableFilter:
    #   _audio_set_filter_enabled, _audio_set_filter_mode,
    #   _audio_set_filter_cutoff, _audio_set_filter_resonance
    #
    # FX chain — WaveshaperProcessor:
    #   _audio_set_waveshaper_enabled, _audio_set_waveshaper_shape,
    #   _audio_set_waveshaper_drive
    #
    # FX chain — DelayLine:
    #   _audio_set_delay_enabled, _audio_set_delay_time_ms,
    #   _audio_set_delay_feedback
    #
    # FX chain — SmoothedValue (frequency glide):
    #   _audio_set_glide_ms
    LINK_FLAGS "-O3 -s STANDALONE_WASM --no-entry -s EXPORTED_FUNCTIONS=[\
'_audio_init',\
'_audio_process',\
'_audio_set_frequency',\
'_audio_set_waveform',\
'_audio_set_harmonics',\
'_audio_destroy',\
'_get_buffer',\
'_get_viz_buffer',\
'_get_viz_buffer_capacity',\
'_viz_render_wavetable',\
'_audio_set_filter_enabled',\
'_audio_set_filter_mode',\
'_audio_set_filter_cutoff',\
'_audio_set_filter_resonance',\
'_audio_set_waveshaper_enabled',\
'_audio_set_waveshaper_shape',\
'_audio_set_waveshaper_drive',\
'_audio_set_delay_enabled',\
'_audio_set_delay_time_ms',\
'_audio_set_delay_feedback',\
'_audio_set_glide_ms'\
]"
)
