cmake_minimum_required(VERSION 3.24)
project(Chorale VERSION 1.2.1 LANGUAGES C CXX)

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

juce_add_plugin(Chorale
    COMPANY_NAME "Chorale"
    PLUGIN_MANUFACTURER_CODE Chrl
    PLUGIN_CODE Chor
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Chorale"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE          # MIDI-driven harmony mode (step 8)
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

set(DSP_SOURCES
    src/dsp/YinTracker.cpp
    src/dsp/KeyEngine.cpp
    src/dsp/PsolaShifter.cpp
    src/dsp/HarmonyEngine.cpp
)

# IBM Plex (OFL) embedded so the UI renders identically everywhere.
juce_add_binary_data(ChoraleFonts SOURCES
    assets/fonts/IBMPlexSans-Regular.ttf
    assets/fonts/IBMPlexSans-SemiBold.ttf
    assets/fonts/IBMPlexMono-Regular.ttf
)

target_sources(Chorale PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/ui/Theme.cpp
    src/ui/ChoraleLookAndFeel.cpp
    src/ui/GainFader.cpp
    src/ui/NoteChips.cpp
    src/ui/StageView.cpp
    src/ui/MixerView.cpp
    src/ui/VoiceDetailPanel.cpp
    src/ui/EqPanel.cpp
    src/ui/CompPanel.cpp
    src/ui/FxPanels.cpp
    src/ui/GraphCanvas.cpp
    src/ui/FxView.cpp
    ${DSP_SOURCES}
)

target_compile_definitions(Chorale PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# Offline DSP tests: JUCE-free, run against synthesized vocal signals.
add_executable(dsp_tests tests/test_dsp.cpp ${DSP_SOURCES})
target_include_directories(dsp_tests PRIVATE src)

# Offline harmonizer CLI for running real vocal WAVs through the engine.
add_executable(harmonize tools/harmonize.cpp ${DSP_SOURCES})
target_include_directories(harmonize PRIVATE src)

# Offline CPU benchmark for the DSP path (JUCE-free).
add_executable(benchmark tools/benchmark.cpp ${DSP_SOURCES})
target_include_directories(benchmark PRIVATE src)
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU|AppleClang")
    target_compile_options(benchmark PRIVATE -O3)
endif()

target_link_libraries(Chorale
    PRIVATE
        ChoraleFonts
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
