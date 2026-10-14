cmake_minimum_required(VERSION 3.22)

# Mac: un solo binario que funciona en Mac con chip Apple (M1/M2/M3...) e Intel
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)

project(CumbiaGlitch VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.9
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(PLUGIN_FORMATS VST3 AU)
else()
    set(PLUGIN_FORMATS VST3)
endif()

juce_add_plugin(CumbiaGlitch
    COMPANY_NAME "Ricardo"
    BUNDLE_ID "com.ricardo.cumbiaglitch"
    PRODUCT_NAME "CumbiaGlitch"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Rcdo
    PLUGIN_CODE Cgch
    VST3_CATEGORIES Instrument Sampler
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS ${PLUGIN_FORMATS})

target_sources(CumbiaGlitch PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/GlitchEngine.cpp)

target_compile_definitions(CumbiaGlitch PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

option(CUMBIA_BUILD_TESTS "Compilar la prueba de audio offline" OFF)
if(CUMBIA_BUILD_TESTS)
    juce_add_console_app(RenderTest PRODUCT_NAME "RenderTest")
    target_sources(RenderTest PRIVATE
        Tests/RenderTest.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/GlitchEngine.cpp)
    target_compile_definitions(RenderTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(RenderTest PRIVATE juce::juce_audio_utils juce::juce_dsp
                          PUBLIC juce::juce_recommended_config_flags)
endif()

target_link_libraries(CumbiaGlitch
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
