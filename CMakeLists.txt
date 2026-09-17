cmake_minimum_required(VERSION 3.22)

project(ArrowEch VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
option(ARROWECH_COPY_PLUGINS "Install plug-ins into ~/Library/Audio/Plug-Ins after building" ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ArrowEch
    COMPANY_NAME "Arrow"
    PLUGIN_MANUFACTURER_CODE Arrw
    PLUGIN_CODE ArEc
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "ArrowEch"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "ArrowEch needs your microphone so the standalone app can echo live sound."
    COPY_PLUGIN_AFTER_BUILD ${ARROWECH_COPY_PLUGINS})

target_sources(ArrowEch PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(ArrowEch PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ArrowEch
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline DSP smoke test: cmake --build build --target DSPSmokeTest && ./build/DSPSmokeTest_artefacts/Release/DSPSmokeTest
juce_add_console_app(DSPSmokeTest PRODUCT_NAME "DSPSmokeTest")
target_sources(DSPSmokeTest PRIVATE Tests/DSPSmokeTest.cpp)
target_compile_definitions(DSPSmokeTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(DSPSmokeTest PRIVATE juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)

# Renders the editor to PNG files for visual checks.
juce_add_console_app(UISnapshot PRODUCT_NAME "UISnapshot")
target_sources(UISnapshot PRIVATE Tests/UISnapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(UISnapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="ArrowEch")
target_link_libraries(UISnapshot PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
