cmake_minimum_required(VERSION 3.22)
project(UncertainVoice VERSION 0.1.0)

# ── Dual build : -DUV_TRIAL=OFF (payant) / -DUV_TRIAL=ON (free) ──────────
option(UV_TRIAL "Build the free version" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

if(UV_TRIAL)
    set(UV_NAME "Uncertain Voice Free")
    set(UV_CODE Uvcf)
else()
    set(UV_NAME "Uncertain Voice")
    set(UV_CODE Uvcp)
endif()

juce_add_plugin(UncertainVoice
    COMPANY_NAME              "Uncertain"
    PLUGIN_MANUFACTURER_CODE  Uncr
    PLUGIN_CODE               ${UV_CODE}
    FORMATS                   VST3 Standalone
    PRODUCT_NAME              "${UV_NAME}"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE)

target_sources(UncertainVoice PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(UncertainVoice PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(UV_TRIAL)
    target_compile_definitions(UncertainVoice PRIVATE UV_TRIAL=1)
endif()

target_link_libraries(UncertainVoice
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
