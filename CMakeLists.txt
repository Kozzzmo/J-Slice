cmake_minimum_required(VERSION 3.22)

project(J_SLICE VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    # static runtime : the plugin works without installing the Visual C++ redistributable
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(JSLICE_BUILD_TESTS "Build the offline test program" ON)
option(JSLICE_BUILD_PLUGIN "Build the plugin" ON)

# JUCE : use a local checkout if JUCE_SOURCE_DIR is given, otherwise download it
if(DEFINED JUCE_SOURCE_DIR)
    add_subdirectory(${JUCE_SOURCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(CORE_SOURCES
    Source/Core/Analyzer.cpp
    Source/Core/Generator.cpp
    Source/Core/Engine.cpp)

set(COMMON_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_FLAC=1
    JUCE_USE_OGGVORBIS=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODAL_LOOPS_PERMITTED=1)

#==============================================================================
if(JSLICE_BUILD_PLUGIN)
juce_add_plugin(JSlice
    PRODUCT_NAME "J-Slice"
    COMPANY_NAME "Kozzzmo"
    BUNDLE_ID "com.kozzzmo.jslice"
    PLUGIN_MANUFACTURER_CODE Kozz
    PLUGIN_CODE JSlc
    DESCRIPTION "Jungle / drum and bass break slicer and generator"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Sampler Drum
    FORMATS VST3 Standalone)

target_sources(JSlice PRIVATE
    ${CORE_SOURCES}
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/LookAndFeel.cpp
    Source/UI/WaveformView.cpp
    Source/UI/StepGrid.cpp
    Source/UI/BrowserPanel.cpp)

target_include_directories(JSlice PRIVATE Source)
target_compile_definitions(JSlice PUBLIC ${COMMON_DEFINITIONS})

target_link_libraries(JSlice
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
endif()

#==============================================================================
if(JSLICE_BUILD_TESTS)
    juce_add_console_app(JSliceTests PRODUCT_NAME "JSliceTests")
    target_sources(JSliceTests PRIVATE Tests/TestMain.cpp ${CORE_SOURCES})
    target_include_directories(JSliceTests PRIVATE Source)
    target_compile_definitions(JSliceTests PRIVATE ${COMMON_DEFINITIONS})
    target_link_libraries(JSliceTests
        PRIVATE
            juce::juce_audio_formats
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()

#==============================================================================
# Integration harness (Linux only, used during development)
if(JSLICE_BUILD_PLUGIN AND JSLICE_BUILD_TESTS AND UNIX AND NOT APPLE)
    juce_add_console_app(JSliceHarness PRODUCT_NAME "JSliceHarness")
    target_sources(JSliceHarness PRIVATE Tests/Harness.cpp)
    target_include_directories(JSliceHarness PRIVATE Source)
    target_link_libraries(JSliceHarness PRIVATE JSlice juce::juce_audio_utils)
endif()
