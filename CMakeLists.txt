cmake_minimum_required(VERSION 3.22)

# Static MSVC runtime: the plugin works without installing VC++ redistributables
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")

project(FormantFlex VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FORMANTFLEX_STANDALONE "Also build a standalone app" ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.9
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

set(FF_FORMATS VST3)
if(APPLE)
    list(APPEND FF_FORMATS AU)
endif()
if(FORMANTFLEX_STANDALONE)
    list(APPEND FF_FORMATS Standalone)
endif()

juce_add_plugin(FormantFlex
    COMPANY_NAME                "FormantFlex"
    BUNDLE_ID                   "com.formantflex.plugin"
    PLUGIN_MANUFACTURER_CODE    Ffxx
    PLUGIN_CODE                 Fmf1
    FORMATS                     ${FF_FORMATS}
    PRODUCT_NAME                "FormantFlex"
    DESCRIPTION                 "Voice formant shifter"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Pitch\ Shift
    AU_MAIN_TYPE                kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(FormantFlex PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(FormantFlex PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(FormantFlex
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
