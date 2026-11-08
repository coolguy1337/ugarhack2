cmake_minimum_required(VERSION 3.22)
project(PitchHack VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")   # universal binary
endif()

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")   # static CRT: no VC++ redistributable needed
endif()

# Use a local JUCE checkout with -DPITCHHACK_JUCE_DIR=/path/to/JUCE, otherwise download it.
if(PITCHHACK_JUCE_DIR)
  add_subdirectory(${PITCHHACK_JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                            GIT_TAG 8.0.8 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

# CLAP support (clap-juce-extensions). Local checkout: -DPITCHHACK_CLAP_EXT_DIR=/path
if(PITCHHACK_CLAP_EXT_DIR)
  add_subdirectory(${PITCHHACK_CLAP_EXT_DIR} clap-juce-extensions EXCLUDE_FROM_ALL)
else()
  include(FetchContent)
  FetchContent_Declare(clap-juce-extensions GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
                                            GIT_TAG 6bde108329ff9fc24404b79b8b397261a75cf6a8)
  FetchContent_MakeAvailable(clap-juce-extensions)
endif()

juce_add_plugin(PitchHack
    PRODUCT_NAME "Pitch Hack"
    COMPANY_NAME "PitchHackPort"
    BUNDLE_ID com.pitchhackport.pitchhack
    PLUGIN_MANUFACTURER_CODE Phpt
    PLUGIN_CODE Phck
    FORMATS VST3 Standalone
    VST3_CATEGORIES Fx "Pitch Shift"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(PitchHack PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(PitchHack PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(PitchHack PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

clap_juce_extensions_plugin(TARGET PitchHack
    CLAP_ID "com.pitchhackport.pitchhack"
    CLAP_FEATURES audio-effect pitch-shifter stereo
    CLAP_PROCESS_EVENTS_RESOLUTION_SAMPLES 64
    CLAP_USE_JUCE_PARAMETER_RANGES DISCRETE)

# DSP unit test (no JUCE):  cmake --build . --target dsp_test && ./dsp_test
add_executable(dsp_test EXCLUDE_FROM_ALL tests/dsp_test.cpp)
