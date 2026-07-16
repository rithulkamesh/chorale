cmake_minimum_required(VERSION 3.24)
project(OpenHarmony VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(OpenHarmony
    COMPANY_NAME "OpenHarmony"
    PLUGIN_MANUFACTURER_CODE Ohrm
    PLUGIN_CODE Ohvh
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "OpenHarmony"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE          # MIDI-driven harmony mode (step 8)
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

set(DSP_SOURCES
    src/dsp/YinTracker.cpp
    src/dsp/KeyEngine.cpp
    src/dsp/PsolaShifter.cpp
    src/dsp/HarmonyEngine.cpp
)

target_sources(OpenHarmony PRIVATE
    src/PluginProcessor.cpp
    ${DSP_SOURCES}
)

target_compile_definitions(OpenHarmony PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# Offline DSP tests: JUCE-free, run against synthesized vocal signals.
add_executable(dsp_tests tests/test_dsp.cpp ${DSP_SOURCES})
target_include_directories(dsp_tests PRIVATE src)

target_link_libraries(OpenHarmony
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
