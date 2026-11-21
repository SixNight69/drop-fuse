cmake_minimum_required(VERSION 3.22)

# Universal binary on macOS (Apple Silicon + Intel)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(DropTNT VERSION 3.3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

# Copy into ~/Library after build (handy while developing). The installer script turns this off.
option(RIOT_COPY_AFTER_BUILD "Copy plugins to the user plug-in folders after building" ON)

if(APPLE)
    set(RIOT_FORMATS AU VST3 Standalone)
else()
    set(RIOT_FORMATS VST3 Standalone)
endif()

juce_add_plugin(DropTNT
    COMPANY_NAME                "RiotAudio"
    BUNDLE_ID                   "com.riotaudio.droptnt"
    PLUGIN_MANUFACTURER_CODE    Riot
    PLUGIN_CODE                 Dtnt
    FORMATS                     ${RIOT_FORMATS}
    PRODUCT_NAME                "Drop TNT"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD     ${RIOT_COPY_AFTER_BUILD})

target_sources(DropTNT PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(DropTNT PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(DropTNT
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
