cmake_minimum_required(VERSION 3.22)
project(IDWVoiceMIDIStudio VERSION 9.2.0)

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

include(CTest)
if(BUILD_TESTING)
    add_executable(IDWTests Tests/ProcessorTests.cpp)
    target_include_directories(IDWTests PRIVATE Source "$<TARGET_PROPERTY:IDWVoiceMIDIStudio,JUCE_GENERATED_SOURCES_DIRECTORY>")
    target_compile_definitions(IDWTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(IDWTests PRIVATE IDWVoiceMIDIStudio juce::juce_audio_utils juce::juce_dsp)
    add_test(NAME IDWProcessorRegression COMMAND IDWTests)
    add_executable(IDWSetupTests Tests/SetupDiagnosticsTests.cpp)
    target_include_directories(IDWSetupTests PRIVATE Source)
    add_test(NAME IDWSetupDiagnostics COMMAND IDWSetupTests)
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
        PRODUCT_NAME "IDW DAW V14 Foundation"
        COMPANY_NAME "In Da Wind Entertainment"
        BUNDLE_ID "com.idwent.idwdaw")
    juce_generate_juce_header(IDWDAW)
    target_sources(IDWDAW PRIVATE DAW/Main.cpp DAW/SessionModel.h
        DAW/RemoteLocalSocketReceiver.cpp DAW/RemoteLocalSessionSecret.cpp DAW/RemoteWindowsBootstrapChannel.cpp
        DAW/RemoteWindowsBridgeSession.cpp)
    target_include_directories(IDWDAW PRIVATE DAW)
    target_link_libraries(IDWDAW PRIVATE juce::juce_gui_extra juce::juce_audio_utils
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    target_compile_definitions(IDWDAW PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    if(WIN32)
        target_link_libraries(IDWDAW PRIVATE sapi ole32 ws2_32 bcrypt advapi32)
    endif()
    if(APPLE)
        find_library(IDW_SECURITY_FRAMEWORK Security REQUIRED)
        target_link_libraries(IDWDAW PRIVATE "${IDW_SECURITY_FRAMEWORK}")
    endif()
    if(BUILD_TESTING)
        find_package(Threads REQUIRED)
        add_executable(IDWDAWTLSOfferWireTests DAW/tests/RemoteTLSOfferWireTests.cpp)
        add_test(NAME IDWDAWTLSOfferWire COMMAND IDWDAWTLSOfferWireTests)
        set_tests_properties(IDWDAWTLSOfferWire PROPERTIES TIMEOUT 30)
        add_executable(IDWDAWTransportDispatchGuardTests DAW/tests/RemoteTransportDispatchGuardTests.cpp)
        target_link_libraries(IDWDAWTransportDispatchGuardTests PRIVATE Threads::Threads)
        add_test(NAME IDWDAWTransportDispatchGuard COMMAND IDWDAWTransportDispatchGuardTests)
        set_tests_properties(IDWDAWTransportDispatchGuard PROPERTIES TIMEOUT 30)
        add_executable(IDWDAWLocalSocketReceiverTests
            DAW/tests/RemoteLocalSocketReceiverTests.cpp DAW/RemoteLocalSocketReceiver.cpp)
        target_link_libraries(IDWDAWLocalSocketReceiverTests PRIVATE Threads::Threads)
        if(WIN32)
            target_link_libraries(IDWDAWLocalSocketReceiverTests PRIVATE ws2_32)
        endif()
        add_test(NAME IDWDAWLocalSocketReceiver COMMAND IDWDAWLocalSocketReceiverTests)
        set_tests_properties(IDWDAWLocalSocketReceiver PROPERTIES TIMEOUT 30)
        add_executable(IDWDAWLocalSessionOwnerTests DAW/tests/RemoteLocalSessionOwnerTests.cpp
            DAW/RemoteLocalSocketReceiver.cpp DAW/RemoteLocalSessionSecret.cpp)
        target_link_libraries(IDWDAWLocalSessionOwnerTests PRIVATE Threads::Threads)
        if(WIN32)
            target_link_libraries(IDWDAWLocalSessionOwnerTests PRIVATE ws2_32 bcrypt)
        endif()
        if(APPLE)
            target_link_libraries(IDWDAWLocalSessionOwnerTests PRIVATE "${IDW_SECURITY_FRAMEWORK}")
        endif()
        add_test(NAME IDWDAWLocalSessionOwner COMMAND IDWDAWLocalSessionOwnerTests)
        set_tests_properties(IDWDAWLocalSessionOwner PROPERTIES TIMEOUT 30)
        add_executable(IDWDAWWindowsBootstrapChannelTests DAW/tests/RemoteWindowsBootstrapChannelTests.cpp
            DAW/RemoteWindowsBootstrapChannel.cpp DAW/RemoteLocalSocketReceiver.cpp DAW/RemoteLocalSessionSecret.cpp)
        target_link_libraries(IDWDAWWindowsBootstrapChannelTests PRIVATE Threads::Threads)
        if(WIN32)
            target_link_libraries(IDWDAWWindowsBootstrapChannelTests PRIVATE ws2_32 bcrypt advapi32)
        endif()
        if(APPLE)
            target_link_libraries(IDWDAWWindowsBootstrapChannelTests PRIVATE "${IDW_SECURITY_FRAMEWORK}")
        endif()
        add_test(NAME IDWDAWWindowsBootstrapChannel COMMAND IDWDAWWindowsBootstrapChannelTests)
        set_tests_properties(IDWDAWWindowsBootstrapChannel PROPERTIES TIMEOUT 30)
        add_executable(IDWDAWWindowsBridgeSessionTests DAW/tests/RemoteWindowsBridgeSessionTests.cpp
            DAW/RemoteWindowsBridgeSession.cpp DAW/RemoteWindowsBootstrapChannel.cpp
            DAW/RemoteLocalSocketReceiver.cpp DAW/RemoteLocalSessionSecret.cpp)
        target_link_libraries(IDWDAWWindowsBridgeSessionTests PRIVATE Threads::Threads)
        if(WIN32)
            target_link_libraries(IDWDAWWindowsBridgeSessionTests PRIVATE ws2_32 bcrypt advapi32)
        endif()
        if(APPLE)
            target_link_libraries(IDWDAWWindowsBridgeSessionTests PRIVATE "${IDW_SECURITY_FRAMEWORK}")
        endif()
        add_test(NAME IDWDAWWindowsBridgeSession COMMAND IDWDAWWindowsBridgeSessionTests)
        set_tests_properties(IDWDAWWindowsBridgeSession PROPERTIES TIMEOUT 30)
        # Test fixture is driven by the Python mTLS integration test, never run alone.
        add_executable(IDWDAWNativeLocalReviewFixture DAW/tests/RemoteNativeLocalReviewFixture.cpp
            DAW/RemoteLocalSocketReceiver.cpp DAW/RemoteLocalSessionSecret.cpp DAW/RemoteWindowsBootstrapChannel.cpp)
        target_link_libraries(IDWDAWNativeLocalReviewFixture PRIVATE Threads::Threads)
        if(WIN32)
            target_link_libraries(IDWDAWNativeLocalReviewFixture PRIVATE ws2_32 bcrypt advapi32 shell32)
        endif()
        if(APPLE)
            target_link_libraries(IDWDAWNativeLocalReviewFixture PRIVATE "${IDW_SECURITY_FRAMEWORK}")
        endif()
        add_executable(IDWDAWRecordingTests DAW/tests/RecordingInterruptionTests.cpp)
        target_include_directories(IDWDAWRecordingTests PRIVATE DAW)
        target_link_libraries(IDWDAWRecordingTests PRIVATE juce::juce_audio_formats juce::juce_audio_devices)
        target_compile_definitions(IDWDAWRecordingTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
        add_test(NAME IDWDAWRecordingInterruption COMMAND IDWDAWRecordingTests)
        add_executable(IDWDAWRelinkTests DAW/tests/AudioRelinkTests.cpp)
        target_include_directories(IDWDAWRelinkTests PRIVATE DAW)
        target_link_libraries(IDWDAWRelinkTests PRIVATE juce::juce_audio_formats juce::juce_audio_devices)
        target_compile_definitions(IDWDAWRelinkTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
        add_test(NAME IDWDAWRelink COMMAND IDWDAWRelinkTests)
        add_executable(IDWDAWRecoveryTests DAW/tests/SessionRecoveryTests.cpp)
        target_include_directories(IDWDAWRecoveryTests PRIVATE DAW)
        target_link_libraries(IDWDAWRecoveryTests PRIVATE juce::juce_core)
        target_compile_definitions(IDWDAWRecoveryTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
        add_test(NAME IDWDAWRecovery COMMAND IDWDAWRecoveryTests)
        add_executable(IDWDAWClipHistoryTests DAW/tests/ClipHistoryTests.cpp)
        target_include_directories(IDWDAWClipHistoryTests PRIVATE DAW)
        add_test(NAME IDWDAWClipHistory COMMAND IDWDAWClipHistoryTests)
        add_executable(IDWDAWClipTransportTests DAW/tests/ClipTransportTests.cpp)
        target_include_directories(IDWDAWClipTransportTests PRIVATE DAW)
        target_link_libraries(IDWDAWClipTransportTests PRIVATE juce::juce_audio_formats juce::juce_audio_devices)
        target_compile_definitions(IDWDAWClipTransportTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
        add_test(NAME IDWDAWClipTransport COMMAND IDWDAWClipTransportTests)
        add_executable(IDWDAWSnapTests DAW/tests/SnapGridTests.cpp)
        target_include_directories(IDWDAWSnapTests PRIVATE DAW)
        add_test(NAME IDWDAWSnap COMMAND IDWDAWSnapTests)
        add_executable(IDWDAWZoomTests DAW/tests/TimelineZoomTests.cpp)
        target_include_directories(IDWDAWZoomTests PRIVATE DAW)
        add_test(NAME IDWDAWZoom COMMAND IDWDAWZoomTests)
        add_executable(IDWDAWWaveformTests DAW/tests/WaveformViewTests.cpp)
        target_include_directories(IDWDAWWaveformTests PRIVATE DAW)
        add_test(NAME IDWDAWWaveform COMMAND IDWDAWWaveformTests)
        add_executable(IDWDAWClipFadeTests DAW/tests/ClipFadeTests.cpp)
        target_include_directories(IDWDAWClipFadeTests PRIVATE DAW)
        add_test(NAME IDWDAWClipFades COMMAND IDWDAWClipFadeTests)
        add_executable(IDWDAWMultiClipTests DAW/tests/MultiClipPlaybackTests.cpp)
        target_include_directories(IDWDAWMultiClipTests PRIVATE DAW)
        add_test(NAME IDWDAWMultiClip COMMAND IDWDAWMultiClipTests)
        add_executable(IDWDAWClipEditingTests DAW/tests/ClipEditingTests.cpp)
        target_include_directories(IDWDAWClipEditingTests PRIVATE DAW)
        add_test(NAME IDWDAWClipEditing COMMAND IDWDAWClipEditingTests)
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
        add_executable(IDWDAWRemotePermissionTests DAW/tests/RemotePermissionPolicyTests.cpp)
        target_compile_features(IDWDAWRemotePermissionTests PRIVATE cxx_std_17)
        add_test(NAME IDWDAWRemotePermission COMMAND IDWDAWRemotePermissionTests)
        add_executable(IDWDAWRemoteCommandTests DAW/tests/RemoteCommandValidationTests.cpp)
        target_compile_features(IDWDAWRemoteCommandTests PRIVATE cxx_std_17)
        add_test(NAME IDWDAWRemoteCommand COMMAND IDWDAWRemoteCommandTests)
        add_executable(IDWDAWRemoteSessionControllerTests DAW/tests/RemoteSessionControllerTests.cpp)
        target_compile_features(IDWDAWRemoteSessionControllerTests PRIVATE cxx_std_17)
        add_test(NAME IDWDAWRemoteSessionController COMMAND IDWDAWRemoteSessionControllerTests)
        add_executable(IDWDAWRemoteHandshakeTests DAW/tests/RemoteHandshakeGateTests.cpp)
        target_compile_features(IDWDAWRemoteHandshakeTests PRIVATE cxx_std_17)
        add_test(NAME IDWDAWRemoteHandshake COMMAND IDWDAWRemoteHandshakeTests)
        add_executable(IDWDAWRemoteWireFrameTests DAW/tests/RemoteWireFrameTests.cpp)
        target_compile_features(IDWDAWRemoteWireFrameTests PRIVATE cxx_std_17)
        add_test(NAME IDWDAWRemoteWireFrame COMMAND IDWDAWRemoteWireFrameTests)
        add_executable(IDWDAWRemoteStreamDecoderTests DAW/tests/RemoteStreamDecoderTests.cpp)
        target_compile_features(IDWDAWRemoteStreamDecoderTests PRIVATE cxx_std_17)
        add_test(NAME IDWDAWRemoteStreamDecoder COMMAND IDWDAWRemoteStreamDecoderTests)
        add_executable(IDWDAWRemoteReceivePipelineTests DAW/tests/RemoteReceivePipelineTests.cpp)
        target_compile_features(IDWDAWRemoteReceivePipelineTests PRIVATE cxx_std_17)
        add_test(NAME IDWDAWRemoteReceivePipeline COMMAND IDWDAWRemoteReceivePipelineTests)
        add_executable(IDWDAWRemoteParserTests DAW/tests/RemoteControlCommandParserTests.cpp)
        target_compile_features(IDWDAWRemoteParserTests PRIVATE cxx_std_17)
        add_test(NAME IDWDAWRemoteParser COMMAND IDWDAWRemoteParserTests)
        add_executable(IDWDAWValidatedReceiverTests DAW/tests/ValidatedControlReceiverTests.cpp)
        target_compile_features(IDWDAWValidatedReceiverTests PRIVATE cxx_std_17)
        add_test(NAME IDWDAWValidatedReceiver COMMAND IDWDAWValidatedReceiverTests)
        add_executable(IDWDAWHostApprovalTests DAW/tests/RemoteHostApprovalQueueTests.cpp)
        target_compile_features(IDWDAWHostApprovalTests PRIVATE cxx_std_17)
        add_test(NAME IDWDAWHostApproval COMMAND IDWDAWHostApprovalTests)
        add_executable(IDWDAWControllerViewTests DAW/tests/RemoteControllerViewStateTests.cpp)
        target_compile_features(IDWDAWControllerViewTests PRIVATE cxx_std_17)
        add_test(NAME IDWDAWControllerView COMMAND IDWDAWControllerViewTests)
        add_executable(IDWDAWRemoteReceiptTests DAW/tests/RemoteCommandReceiptTests.cpp)
        target_compile_features(IDWDAWRemoteReceiptTests PRIVATE cxx_std_17)
        add_test(NAME IDWDAWRemoteReceipt COMMAND IDWDAWRemoteReceiptTests)
        add_executable(IDWDAWHostReviewControllerTests DAW/tests/RemoteHostReviewControllerTests.cpp)
        target_compile_features(IDWDAWHostReviewControllerTests PRIVATE cxx_std_17)
        add_test(NAME IDWDAWHostReviewController COMMAND IDWDAWHostReviewControllerTests)
        add_executable(IDWDAWSessionReviewCoordinatorTests DAW/tests/RemoteSessionReviewCoordinatorTests.cpp)
        target_compile_features(IDWDAWSessionReviewCoordinatorTests PRIVATE cxx_std_17)
        add_test(NAME IDWDAWSessionReviewCoordinator COMMAND IDWDAWSessionReviewCoordinatorTests)
        # Compile-check the experimental JUCE review panel independently of the DAW UI.
        add_executable(IDWDAWHostReviewPanelCompile DAW/tests/RemoteHostReviewPanelCompile.cpp)
        target_compile_features(IDWDAWHostReviewPanelCompile PRIVATE cxx_std_17)
        target_include_directories(IDWDAWHostReviewPanelCompile PRIVATE DAW)
        target_link_libraries(IDWDAWHostReviewPanelCompile PRIVATE juce::juce_gui_basics)
        target_compile_definitions(IDWDAWHostReviewPanelCompile PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
        add_test(NAME IDWDAWHostReviewPanelCompile COMMAND IDWDAWHostReviewPanelCompile)
        add_executable(IDWDAWBounceTests DAW/BounceTests.cpp)
        target_include_directories(IDWDAWBounceTests PRIVATE DAW)
        target_link_libraries(IDWDAWBounceTests PRIVATE juce::juce_core juce::juce_audio_formats)
        target_compile_definitions(IDWDAWBounceTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
        add_test(NAME IDWDAWBounce COMMAND IDWDAWBounceTests)
    endif()
endif()

