cmake_minimum_required(VERSION 3.22)

project(TheOne VERSION 1.4.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: lokaler Pfad (-DJUCE_PATH=...) oder automatischer Download
if(DEFINED JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# CLAP (Bitwig u.a.) ueber clap-juce-extensions
if(NOT THEONE_NO_CLAP)
    include(FetchContent)
    FetchContent_Declare(
        clap-juce-extensions
        GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
        GIT_TAG main
        GIT_SUBMODULES_RECURSE TRUE
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(clap-juce-extensions)
endif()

juce_add_plugin(TheOne
    COMPANY_NAME "Dani Koenig"
    BUNDLE_ID "ch.danikoenig.theone"
    PLUGIN_MANUFACTURER_CODE Dkng
    PLUGIN_CODE Tone
    FORMATS VST3 AU Standalone
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    PRODUCT_NAME "The One"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)

target_sources(TheOne PRIVATE
    Source/Engine.cpp
    Source/Effects.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Look.cpp)

target_compile_definitions(TheOne PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(TheOne
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(NOT THEONE_NO_CLAP)
    clap_juce_extensions_plugin(TARGET TheOne
        CLAP_ID "ch.danikoenig.theone"
        CLAP_FEATURES instrument synthesizer stereo)
endif()

if(THEONE_TEST_SRC)
    target_compile_definitions(TheOne PUBLIC JUCE_MODAL_LOOPS_PERMITTED=1)
endif()

# Optionaler Test (nur lokal): -DTHEONE_TEST_SRC=tests/GuiSnapshot.cpp
if(THEONE_TEST_SRC)
    add_executable(TheOneTest ${THEONE_TEST_SRC})
    target_link_libraries(TheOneTest PRIVATE TheOne)
    target_include_directories(TheOneTest PRIVATE $<TARGET_PROPERTY:TheOne,INCLUDE_DIRECTORIES>)
    target_compile_definitions(TheOneTest PRIVATE $<TARGET_PROPERTY:TheOne,COMPILE_DEFINITIONS>)
endif()
