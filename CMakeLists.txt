cmake_minimum_required(VERSION 3.22)

# macOS : binaire universel (Apple Silicon + Intel), compatible 10.13+
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

project(ravenn VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE : dossier local external/JUCE-8.0.12, sinon telechargement automatique
set(JUCE_PATH "" CACHE PATH "Dossier JUCE local (optionnel)")
if(NOT JUCE_PATH AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE-8.0.12/CMakeLists.txt")
    set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE-8.0.12")
endif()
if(JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.12.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(ravenn
    COMPANY_NAME "ravenn"
    BUNDLE_ID "com.ravenn.ravenn"
    PLUGIN_MANUFACTURER_CODE Rvnd
    PLUGIN_CODE Rvnn
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "ravenn"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Mastering
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

set(RAVENN_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetManager.cpp)

target_sources(ravenn PRIVATE ${RAVENN_SOURCES})

target_compile_definitions(ravenn PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

if(MSVC)
    target_compile_options(ravenn PRIVATE /utf-8)
endif()

target_link_libraries(ravenn
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Programme de test hors DAW
option(RAVENN_TESTS "Construire le test DSP" ON)
if(RAVENN_TESTS)
    juce_add_console_app(ravennTest PRODUCT_NAME "ravennTest")
    target_sources(ravennTest PRIVATE Tests/DspTest.cpp ${RAVENN_SOURCES})
    target_compile_definitions(ravennTest PRIVATE
        JucePlugin_Name="ravenn" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    if(MSVC)
        target_compile_options(ravennTest PRIVATE /utf-8)
    endif()
    target_link_libraries(ravennTest PRIVATE
        juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
