# web/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(SonicForgeWeb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output Wasm directly to public/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/public)

# SonicForge core library
add_library(sonicforge_web STATIC ${CMAKE_CURRENT_SOURCE_DIR}/../src/oscillator.cpp)
target_include_directories(sonicforge_web PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Wasm module: Raw WebAssembly binary (no JS glue)
add_executable(sonicforge src/sonicforge_worklet.cpp)
target_link_libraries(sonicforge PRIVATE sonicforge_web)

set_target_properties(sonicforge PROPERTIES
    SUFFIX ".wasm"
    # --no-entry: Tells the linker there is no main() function.
    # STANDALONE_WASM: Produces a standard Wasm file without Emscripten JS startup code.
    # EXPORTED_FUNCTIONS: List of C functions to keep alive.
    #   Audio path  : get_buffer, audio_init, audio_process, audio_set_frequency,
    #                 audio_set_waveform, audio_set_harmonics, audio_destroy
    #   Viz path    : get_viz_buffer, get_viz_buffer_capacity, viz_render_wavetable
    #                 (used by the second Wasm instance on the main thread)
    LINK_FLAGS "-O3 -s STANDALONE_WASM --no-entry -s EXPORTED_FUNCTIONS=['_audio_init','_audio_process','_audio_set_frequency','_audio_set_waveform','_audio_set_harmonics','_audio_destroy','_get_buffer','_get_viz_buffer','_get_viz_buffer_capacity','_viz_render_wavetable']"
)