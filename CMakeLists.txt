cmake_minimum_required(VERSION 3.22)
project(ChoirMaker VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Quick DSP-only build (no JUCE download):  cmake -DCHOIR_ENGINE_TEST_ONLY=ON ..
option(CHOIR_ENGINE_TEST_ONLY "Only build the DSP unit test" OFF)
if(CHOIR_ENGINE_TEST_ONLY)
    add_executable(engine_test tests/engine_test.cpp)
    target_include_directories(engine_test PRIVATE Source)
    return()
endif()

option(CHOIR_BUILD_LV2 "Also build an LV2 plugin (Linux)" OFF)

# macOS: universal binary, sensible minimum
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

set(CHOIR_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND CHOIR_FORMATS AU)
endif()
if(UNIX AND NOT APPLE AND CHOIR_BUILD_LV2)
    list(APPEND CHOIR_FORMATS LV2)
endif()

juce_add_plugin(ChoirMaker
    COMPANY_NAME                "ChoirMaker Audio"
    PLUGIN_MANUFACTURER_CODE    Chmk
    PLUGIN_CODE                 Chr1
    PRODUCT_NAME                "ChoirMaker"
    FORMATS                     ${CHOIR_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    VST3_CATEGORIES             Fx Modulation
    AU_MAIN_TYPE                kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(ChoirMaker PRIVATE
    Source/ChoirEngine.h
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp)

target_include_directories(ChoirMaker PRIVATE Source)

target_compile_definitions(ChoirMaker PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ChoirMaker
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
