cmake_minimum_required(VERSION 3.22)
project(IDWVoiceMIDIStudio VERSION 3.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is fetched automatically when it is not checked out locally.
# A local ThirdParty/JUCE checkout is still supported for offline development.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/JUCE/CMakeLists.txt")
    add_subdirectory(ThirdParty/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

set(IDW_FORMATS Standalone VST3)
if(APPLE)
    list(APPEND IDW_FORMATS AU)
endif()

juce_add_plugin(IDWVoiceMIDIStudio
    COMPANY_NAME "In Da Wind Entertainment"
    BUNDLE_ID "com.idwent.idwvoicemidistudio"
    PLUGIN_MANUFACTURER_CODE IDWE
    PLUGIN_CODE IDV8
    FORMATS ${IDW_FORMATS}
    PRODUCT_NAME "IDW Voice MIDI Studio"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "IDW Voice MIDI Studio needs microphone access for voice-to-MIDI."
)

# The source files use <JuceHeader.h>; generate JUCE's compatibility header.
juce_generate_juce_header(IDWVoiceMIDIStudio)

target_sources(IDWVoiceMIDIStudio PRIVATE
    Source/PluginProcessor.cpp Source/PluginProcessor.h
    Source/PluginEditor.cpp Source/PluginEditor.h
    Source/YinPitchDetector.cpp Source/YinPitchDetector.h
    Source/BeatboxClassifier.cpp Source/BeatboxClassifier.h
    Source/MidiLearnManager.cpp Source/MidiLearnManager.h
    Source/ScaleManager.cpp Source/ScaleManager.h
    Source/PresetManager.cpp Source/PresetManager.h
    Source/MPEAllocator.cpp Source/MPEAllocator.h
)

target_compile_definitions(IDWVoiceMIDIStudio PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(IDWVoiceMIDIStudio PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
