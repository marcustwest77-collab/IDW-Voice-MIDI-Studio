cmake_minimum_required(VERSION 3.22)
project(IDWVoiceMIDIStudio VERSION 13.0.0)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is fetched automatically when it is not checked out locally.
# A local ThirdParty/JUCE checkout is still supported for offline development.
set(IDW_JUCE_PATH "" CACHE PATH "Optional path to a local JUCE checkout")
if(IDW_JUCE_PATH)
    add_subdirectory("${IDW_JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/JUCE/CMakeLists.txt")
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
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Brand/IDWLogoFull.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Brand/IDWLogoIcon.png"
)

juce_add_binary_data(IDWBrandAssets
    SOURCES
        Resources/Brand/IDWLogoFull.png
        Resources/Brand/IDWLogoIcon.png
)

# The source files use <JuceHeader.h>; generate JUCE's compatibility header.
juce_generate_juce_header(IDWVoiceMIDIStudio)

target_sources(IDWVoiceMIDIStudio PRIVATE
    Source/PluginProcessor.cpp Source/PluginProcessor.h
    Source/PluginEditor.cpp Source/PluginEditor.h Source/VocalFXPanel.h Source/VocalTuner.h Source/VocalEffects.h Source/AutoKeyDetector.h Source/AdaptiveTune.h Source/FormantPitchShifter.h Source/ScaleAwareHarmony.h Source/ReleaseSafety.h Source/SkinTheme.h Source/CustomSkin.h Source/SkinDesigner.h Source/SongPattern.h Source/SongMetronome.h Source/SongLane.h Source/SongTimelinePanel.h Source/SongStudio.h Source/SongStudioPanel.h Source/NativeDictation.h Source/DictationAudioQueue.h Source/SongDictationPanel.h Source/SongRecovery.h
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
    IDWBrandAssets
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

include(CTest)
if(BUILD_TESTING)
    add_executable(IDWSongMixCheckTests Tests/SongMixCheckTests.cpp)
    target_include_directories(IDWSongMixCheckTests PRIVATE Source)
    target_compile_definitions(IDWSongMixCheckTests PRIVATE JUCE_USE_CURL=0)
    target_link_libraries(IDWSongMixCheckTests PRIVATE juce::juce_audio_formats)
    add_test(NAME IDWSongMixCheck COMMAND IDWSongMixCheckTests)
    add_executable(IDWSongMeterTests Tests/SongMeterTests.cpp)
    target_include_directories(IDWSongMeterTests PRIVATE Source)
    target_compile_definitions(IDWSongMeterTests PRIVATE JUCE_USE_CURL=0)
    target_link_libraries(IDWSongMeterTests PRIVATE juce::juce_audio_formats)
    add_test(NAME IDWSongMeters COMMAND IDWSongMeterTests)
    add_executable(IDWSongExportJobTests Tests/SongExportJobTests.cpp)
    target_include_directories(IDWSongExportJobTests PRIVATE Source)
    target_compile_definitions(IDWSongExportJobTests PRIVATE JUCE_USE_CURL=0)
    target_link_libraries(IDWSongExportJobTests PRIVATE juce::juce_audio_formats)
    add_test(NAME IDWSongExportJob COMMAND IDWSongExportJobTests)
    add_executable(IDWSongStemTests Tests/SongStemTests.cpp)
    target_include_directories(IDWSongStemTests PRIVATE Source)
    target_compile_definitions(IDWSongStemTests PRIVATE JUCE_USE_CURL=0)
    target_link_libraries(IDWSongStemTests PRIVATE juce::juce_audio_formats)
    add_test(NAME IDWSongStems COMMAND IDWSongStemTests)
    add_executable(IDWSongPhraseTests Tests/SongPhraseTests.cpp)
    target_include_directories(IDWSongPhraseTests PRIVATE Source)
    target_compile_definitions(IDWSongPhraseTests PRIVATE JUCE_USE_CURL=0)
    target_link_libraries(IDWSongPhraseTests PRIVATE juce::juce_audio_formats)
    add_test(NAME IDWSongPhrases COMMAND IDWSongPhraseTests)
    add_executable(IDWSongTakeTests Tests/SongTakeTests.cpp)
    target_include_directories(IDWSongTakeTests PRIVATE Source)
    target_compile_definitions(IDWSongTakeTests PRIVATE JUCE_USE_CURL=0)
    target_link_libraries(IDWSongTakeTests PRIVATE juce::juce_audio_formats)
    add_test(NAME IDWSongTakes COMMAND IDWSongTakeTests)
    add_executable(IDWSongOverdubTests Tests/SongOverdubTests.cpp)
    target_include_directories(IDWSongOverdubTests PRIVATE Source)
    target_compile_definitions(IDWSongOverdubTests PRIVATE JUCE_USE_CURL=0)
    target_link_libraries(IDWSongOverdubTests PRIVATE juce::juce_audio_formats)
    add_test(NAME IDWSongOverdub COMMAND IDWSongOverdubTests)
    add_executable(IDWSongSeekTests Tests/SongSeekTests.cpp)
    target_include_directories(IDWSongSeekTests PRIVATE Source)
    target_compile_definitions(IDWSongSeekTests PRIVATE JUCE_USE_CURL=0)
    target_link_libraries(IDWSongSeekTests PRIVATE juce::juce_audio_formats)
    add_test(NAME IDWSongSeek COMMAND IDWSongSeekTests)
    add_executable(IDWSongBeatArrangementTests Tests/SongBeatArrangementTests.cpp)
    target_include_directories(IDWSongBeatArrangementTests PRIVATE Source)
    target_compile_definitions(IDWSongBeatArrangementTests PRIVATE JUCE_USE_CURL=0)
    target_link_libraries(IDWSongBeatArrangementTests PRIVATE juce::juce_audio_formats)
    add_test(NAME IDWSongBeatArrangement COMMAND IDWSongBeatArrangementTests)
    add_executable(IDWSongSectionTests Tests/SongSectionTests.cpp)
    target_include_directories(IDWSongSectionTests PRIVATE Source)
    target_compile_definitions(IDWSongSectionTests PRIVATE JUCE_USE_CURL=0)
    target_link_libraries(IDWSongSectionTests PRIVATE juce::juce_audio_formats)
    add_test(NAME IDWSongSections COMMAND IDWSongSectionTests)
    add_executable(IDWSongFadeTests Tests/SongFadeTests.cpp)
    target_include_directories(IDWSongFadeTests PRIVATE Source)
    target_compile_definitions(IDWSongFadeTests PRIVATE JUCE_USE_CURL=0)
    target_link_libraries(IDWSongFadeTests PRIVATE juce::juce_audio_formats)
    add_test(NAME IDWSongFades COMMAND IDWSongFadeTests)
    add_executable(IDWSongClipTests Tests/SongClipTests.cpp)
    target_include_directories(IDWSongClipTests PRIVATE Source)
    target_compile_definitions(IDWSongClipTests PRIVATE JUCE_USE_CURL=0)
    target_link_libraries(IDWSongClipTests PRIVATE juce::juce_audio_formats)
    add_test(NAME IDWSongClips COMMAND IDWSongClipTests)
    add_executable(IDWSongArrangementTests Tests/SongArrangementTests.cpp)
    target_include_directories(IDWSongArrangementTests PRIVATE Source)
    target_compile_definitions(IDWSongArrangementTests PRIVATE JUCE_USE_CURL=0)
    target_link_libraries(IDWSongArrangementTests PRIVATE juce::juce_audio_formats)
    add_test(NAME IDWSongArrangement COMMAND IDWSongArrangementTests)
    add_executable(IDWSongCountInTests Tests/SongCountInTests.cpp)
    target_include_directories(IDWSongCountInTests PRIVATE Source)
    target_compile_definitions(IDWSongCountInTests PRIVATE JUCE_USE_CURL=0)
    target_link_libraries(IDWSongCountInTests PRIVATE juce::juce_audio_formats)
    add_test(NAME IDWSongCountIn COMMAND IDWSongCountInTests)
    add_executable(IDWSongRecoveryTests Tests/SongRecoveryTests.cpp)
    target_include_directories(IDWSongRecoveryTests PRIVATE Source)
    target_compile_definitions(IDWSongRecoveryTests PRIVATE JUCE_USE_CURL=0)
    target_link_libraries(IDWSongRecoveryTests PRIVATE juce::juce_audio_formats)
    add_test(NAME IDWSongRecovery COMMAND IDWSongRecoveryTests)
    find_package(Threads REQUIRED)
    add_executable(IDWDictationQueueTests Tests/DictationAudioQueueTests.cpp)
    target_include_directories(IDWDictationQueueTests PRIVATE Source)
    target_link_libraries(IDWDictationQueueTests PRIVATE Threads::Threads)
    add_test(NAME IDWDictationQueue COMMAND IDWDictationQueueTests)
    add_library(IDWFakeVosk SHARED Tests/FakeVosk.cpp)
    target_link_libraries(IDWFakeVosk PRIVATE Threads::Threads)
    add_executable(IDWNativeDictationTests Tests/NativeDictationTests.cpp)
    target_include_directories(IDWNativeDictationTests PRIVATE Source)
    target_compile_definitions(IDWNativeDictationTests PRIVATE JUCE_USE_CURL=0)
    target_link_libraries(IDWNativeDictationTests PRIVATE juce::juce_core)
    add_dependencies(IDWNativeDictationTests IDWFakeVosk)
    add_test(NAME IDWNativeDictation COMMAND IDWNativeDictationTests $<TARGET_FILE:IDWFakeVosk>)
    add_executable(IDWNativeSpeechSmoke Tests/NativeSpeechSmoke.cpp)
    target_include_directories(IDWNativeSpeechSmoke PRIVATE Source)
    target_compile_definitions(IDWNativeSpeechSmoke PRIVATE JUCE_USE_CURL=0)
    target_link_libraries(IDWNativeSpeechSmoke PRIVATE juce::juce_audio_formats)
    add_executable(IDWTests Tests/ProcessorTests.cpp)
    target_include_directories(IDWTests PRIVATE Source "$<TARGET_PROPERTY:IDWVoiceMIDIStudio,JUCE_GENERATED_SOURCES_DIRECTORY>")
    target_compile_definitions(IDWTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(IDWTests PRIVATE IDWVoiceMIDIStudio IDWBrandAssets juce::juce_audio_utils juce::juce_dsp)
    add_test(NAME IDWProcessorRegression COMMAND IDWTests)
    add_executable(IDWSetupTests Tests/SetupDiagnosticsTests.cpp)
    target_include_directories(IDWSetupTests PRIVATE Source)
    add_test(NAME IDWSetupDiagnostics COMMAND IDWSetupTests)
    add_executable(IDWDawGuideTests Tests/DawGuidesTests.cpp)
    target_include_directories(IDWDawGuideTests PRIVATE Source)
    add_test(NAME IDWDawGuides COMMAND IDWDawGuideTests)
    add_executable(IDWVocalTunerTests Tests/VocalTunerTests.cpp)
    target_include_directories(IDWVocalTunerTests PRIVATE Source)
    add_test(NAME IDWVocalTuner COMMAND IDWVocalTunerTests)
    add_executable(IDWVocalEffectsTests Tests/VocalEffectsTests.cpp)
    target_include_directories(IDWVocalEffectsTests PRIVATE Source)
    add_test(NAME IDWVocalEffects COMMAND IDWVocalEffectsTests)
    add_executable(IDWAutoKeyTests Tests/AutoKeyTests.cpp)
    target_include_directories(IDWAutoKeyTests PRIVATE Source)
    add_test(NAME IDWAutoKey COMMAND IDWAutoKeyTests)
    add_executable(IDWAdaptiveTuneTests Tests/AdaptiveTuneTests.cpp)
    target_include_directories(IDWAdaptiveTuneTests PRIVATE Source)
    add_test(NAME IDWAdaptiveTune COMMAND IDWAdaptiveTuneTests)
    add_executable(IDWFormantHarmonyTests Tests/FormantHarmonyTests.cpp)
    target_include_directories(IDWFormantHarmonyTests PRIVATE Source)
    add_test(NAME IDWFormantHarmony COMMAND IDWFormantHarmonyTests)
    add_executable(IDWReleaseSafetyTests Tests/ReleaseSafetyTests.cpp)
    target_include_directories(IDWReleaseSafetyTests PRIVATE Source)
    add_test(NAME IDWReleaseSafety COMMAND IDWReleaseSafetyTests)
    add_executable(IDWSongStudioTests Tests/SongStudioTests.cpp)
    target_include_directories(IDWSongStudioTests PRIVATE Source)
    target_compile_definitions(IDWSongStudioTests PRIVATE JUCE_USE_CURL=0)
    target_link_libraries(IDWSongStudioTests PRIVATE juce::juce_audio_formats)
    add_test(NAME IDWSongStudio COMMAND IDWSongStudioTests)
    add_executable(IDWSongPatternTests Tests/SongPatternTests.cpp)
    target_include_directories(IDWSongPatternTests PRIVATE Source)
    add_test(NAME IDWSongPattern COMMAND IDWSongPatternTests)
    add_executable(IDWCustomSkinTests Tests/CustomSkinTests.cpp)
    target_include_directories(IDWCustomSkinTests PRIVATE Source)
    add_test(NAME IDWCustomSkins COMMAND IDWCustomSkinTests)
    add_executable(IDWSkinThemeTests Tests/SkinThemeTests.cpp)
    target_include_directories(IDWSkinThemeTests PRIVATE Source)
    add_test(NAME IDWSkinThemes COMMAND IDWSkinThemeTests)
    add_executable(IDWHarmonyTests Tests/HarmonyTests.cpp)
    target_include_directories(IDWHarmonyTests PRIVATE Source)
    add_test(NAME IDWHarmony COMMAND IDWHarmonyTests)
    add_executable(IDWSynthTests Tests/StudioSynthTests.cpp)
    target_include_directories(IDWSynthTests PRIVATE Source)
    add_test(NAME IDWStudioSynth COMMAND IDWSynthTests)
    add_executable(IDWMidiSafetyTests Tests/MidiNoteLedgerTests.cpp)
    target_include_directories(IDWMidiSafetyTests PRIVATE Source)
    add_test(NAME IDWMidiSafety COMMAND IDWMidiSafetyTests)
    add_executable(IDWRollingTests Tests/RollingMidiTests.cpp)
    target_include_directories(IDWRollingTests PRIVATE Source)
    add_test(NAME IDWRetrospective COMMAND IDWRollingTests)
endif()

target_sources(IDWVoiceMIDIStudio_Standalone PRIVATE Source/StandaloneApp.cpp)
target_compile_definitions(IDWVoiceMIDIStudio_Standalone PRIVATE JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)


# V14 DAW prototype is opt-in and does not modify the existing plugin formats.
option(IDW_BUILD_DAW "Build experimental V14 DAW shell" OFF)
if(IDW_BUILD_DAW)
    juce_add_gui_app(IDWDAW
        VERSION 14.0.0
        PRODUCT_NAME "IDW DAW V14 Foundation"
        COMPANY_NAME "In Da Wind Entertainment"
        BUNDLE_ID "com.idwent.idwdaw")
    juce_generate_juce_header(IDWDAW)
    target_sources(IDWDAW PRIVATE DAW/Main.cpp DAW/SessionModel.h)
    target_include_directories(IDWDAW PRIVATE DAW)
    target_link_libraries(IDWDAW PRIVATE juce::juce_gui_extra juce::juce_audio_utils
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    target_compile_definitions(IDWDAW PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    if(WIN32)
        target_link_libraries(IDWDAW PRIVATE sapi ole32)
    endif()
    if(BUILD_TESTING)
        add_executable(IDWDAWTrackVocalEffectsTests DAW/TrackVocalEffectsTests.cpp)
        add_test(NAME IDWDAWTrackVocalEffects COMMAND IDWDAWTrackVocalEffectsTests)
        add_executable(IDWDAWSessionTests DAW/SessionTests.cpp)
        target_include_directories(IDWDAWSessionTests PRIVATE DAW)
        add_test(NAME IDWDAWSession COMMAND IDWDAWSessionTests)
        add_executable(IDWDAWTimelineTests DAW/TimelineTests.cpp)
        target_include_directories(IDWDAWTimelineTests PRIVATE DAW)
        add_test(NAME IDWDAWTimeline COMMAND IDWDAWTimelineTests)
        add_executable(IDWDAWPersistenceTests DAW/SessionPersistenceTests.cpp)
        target_include_directories(IDWDAWPersistenceTests PRIVATE DAW)
        target_link_libraries(IDWDAWPersistenceTests PRIVATE juce::juce_core juce::juce_audio_formats juce::juce_audio_devices)
        target_compile_definitions(IDWDAWPersistenceTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
        add_test(NAME IDWDAWPersistence COMMAND IDWDAWPersistenceTests)
        add_executable(IDWDAWMidiBenchmark DAW/MidiRenderBenchmark.cpp)
        target_include_directories(IDWDAWMidiBenchmark PRIVATE DAW)
        add_test(NAME IDWDAWMidiBenchmarkSmoke COMMAND IDWDAWMidiBenchmark)
        set_tests_properties(IDWDAWMidiBenchmarkSmoke PROPERTIES LABELS "benchmark;daw")
        add_executable(IDWDAWBounceTests DAW/BounceTests.cpp)
        target_include_directories(IDWDAWBounceTests PRIVATE DAW)
        target_link_libraries(IDWDAWBounceTests PRIVATE juce::juce_core juce::juce_audio_formats)
        target_compile_definitions(IDWDAWBounceTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
        add_test(NAME IDWDAWBounce COMMAND IDWDAWBounceTests)
    endif()
endif()
target_link_libraries(IDWVoiceMIDIStudio_Standalone PRIVATE IDWBrandAssets)
