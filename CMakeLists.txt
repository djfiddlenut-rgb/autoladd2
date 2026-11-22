cmake_minimum_required(VERSION 3.22)

# Static MSVC runtime: the .vst3 has no dependency on the Visual C++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(autoladd VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(AUTOLADD_BUILD_TESTS "Build the offline DSP/GUI test harness" OFF)
set(AUTOLADD_JUCE_DIR "" CACHE PATH "Use a local JUCE checkout instead of downloading one")

if(AUTOLADD_JUCE_DIR)
    add_subdirectory(${AUTOLADD_JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.12
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(AUTOLADD_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Win7LookAndFeel.h
    Source/Dsp/AutoTuneEngine.h
    Source/Dsp/PitchTracker.h
    Source/Dsp/Scales.h)

juce_add_plugin(autoladd
    COMPANY_NAME "autoladd"
    PRODUCT_NAME "autoladd"
    PLUGIN_MANUFACTURER_CODE Aldd
    PLUGIN_CODE Ald1
    FORMATS VST3
    VST3_CATEGORIES Fx "Pitch Shift"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(autoladd PRIVATE ${AUTOLADD_SOURCES})

target_compile_definitions(autoladd PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(autoladd
    PRIVATE
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(AUTOLADD_BUILD_TESTS)
    add_subdirectory(Tests)
endif()
