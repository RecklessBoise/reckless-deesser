cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(RecklessDeEsser VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(RDE_COPY_PLUGIN "Copy the built plug-ins into the user plug-in folders" OFF)
option(RDE_BUILD_TESTS "Build the DSP unit tests" ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

set(RDE_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND RDE_FORMATS AU)
endif()

juce_add_plugin(RecklessDeEsser
    COMPANY_NAME             "Reckless"
    COMPANY_WEBSITE          "https://github.com/RecklessBoise/reckless-deesser"
    BUNDLE_ID                com.reckless.deesser
    PLUGIN_MANUFACTURER_CODE Rckl
    PLUGIN_CODE              Rdes
    PRODUCT_NAME             "Reckless DeEsser"
    DESCRIPTION              "Linear-phase de-esser with floating threshold"
    FORMATS                  ${RDE_FORMATS}
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES          Fx Dynamics
    AU_MAIN_TYPE             kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD  ${RDE_COPY_PLUGIN})

target_sources(RecklessDeEsser PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/ui/Widgets.cpp
    Source/ui/SpectrumDisplay.cpp)

target_include_directories(RecklessDeEsser PRIVATE Source)

target_compile_definitions(RecklessDeEsser PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(RecklessDeEsser
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(RDE_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(RecklessDeEsserTests PRODUCT_NAME "RecklessDeEsserTests")
    target_sources(RecklessDeEsserTests PRIVATE tests/DspTests.cpp)
    target_include_directories(RecklessDeEsserTests PRIVATE Source)
    target_compile_definitions(RecklessDeEsserTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(RecklessDeEsserTests
        PRIVATE juce::juce_dsp juce::juce_audio_basics
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME dsp COMMAND RecklessDeEsserTests)
endif()

option(RDE_BUILD_TOOLS "Build the off-screen UI snapshot tool" OFF)

if(RDE_BUILD_TOOLS)
    juce_add_gui_app(RecklessDeEsserSnapshot PRODUCT_NAME "RecklessDeEsserSnapshot")
    target_sources(RecklessDeEsserSnapshot PRIVATE
        tools/Snapshot.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Parameters.cpp
        Source/ui/Widgets.cpp
        Source/ui/SpectrumDisplay.cpp)
    target_include_directories(RecklessDeEsserSnapshot PRIVATE Source)
    target_compile_definitions(RecklessDeEsserSnapshot PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="Reckless DeEsser")
    target_link_libraries(RecklessDeEsserSnapshot
        PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags)
endif()
