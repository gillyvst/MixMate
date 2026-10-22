cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

# CI passes a build number so every release has a new version (hosts use it to notice updates).
set(MIXMATE_BUILD_NUMBER 0 CACHE STRING "Build number, used as the patch version")
project(MixMate VERSION 1.0.${MIXMATE_BUILD_NUMBER})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a local JUCE checkout if one is given (-DJUCE_DIR=...), otherwise download it.
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(NOT MIXMATE_FORMATS)
    set(MIXMATE_FORMATS VST3)
    if(APPLE)
        list(APPEND MIXMATE_FORMATS AU)
    endif()
endif()

juce_add_plugin(MixMate
    COMPANY_NAME "gillyvst"
    BUNDLE_ID "com.gillyvst.mixmate"
    PLUGIN_MANUFACTURER_CODE Gily
    PLUGIN_CODE MxMt
    PRODUCT_NAME "MixMate"
    FORMATS ${MIXMATE_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Analyzer Tools
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(MixMateData SOURCES
    Resources/headphones.tsv
    Resources/fonts/Poppins-Regular.ttf
    Resources/fonts/Poppins-SemiBold.ttf)

target_sources(MixMate PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/HeadphoneDatabase.cpp
    Source/HeadphonePicker.cpp
    Source/SettingsPanel.cpp)

target_compile_definitions(MixMate PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(MixMate
    PRIVATE
        MixMateData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional test program: checks the audio processing and renders screenshots of the UI.
option(MIXMATE_TESTS "Build the MixMate test program" OFF)
if(MIXMATE_TESTS)
    juce_add_console_app(MixMateTests PRODUCT_NAME "MixMateTests")
    target_sources(MixMateTests PRIVATE
        tests/MixMateTests.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/HeadphoneDatabase.cpp
        Source/HeadphonePicker.cpp
        Source/SettingsPanel.cpp)
    target_compile_definitions(MixMateTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(MixMateTests PRIVATE
        MixMateData
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
