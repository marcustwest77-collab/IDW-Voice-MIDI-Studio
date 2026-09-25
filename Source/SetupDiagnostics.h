#pragma once
// Host-independent decision logic; never claims delivery to an external synth.
namespace idw {
enum class SetupState { stopped, clipping, silent, training, disabled, belowGate, outsideRange, uncertain, tracking, listening };
inline SetupState diagnose(bool running, float peak, float rms, float gate,
                           bool training, bool melody, bool beatbox,
                           float confidence, float required, bool noteActive, bool inRange=true) {
    if (!running) return SetupState::stopped;
    if (peak >= 0.98f) return SetupState::clipping;
    if (peak < 0.00001f) return SetupState::silent;
    if (training) return SetupState::training;
    if (!melody && !beatbox) return SetupState::disabled;
    if (rms < gate && melody && !beatbox) return SetupState::belowGate;
    if (!inRange && melody) return SetupState::outsideRange;
    if (noteActive) return SetupState::tracking;
    if (melody && !beatbox && confidence < required) return SetupState::uncertain;
    return SetupState::listening;
}
inline const char* guidance(SetupState s) {
    switch (s) {
    case SetupState::stopped: return "AUDIO STOPPED: enable the device or host processing.";
    case SetupState::clipping: return "INPUT TOO HOT: lower your interface microphone gain.";
    case SetupState::silent: return "NO INPUT: check mic permission, device and input channel.";
    case SetupState::training: return "TRAINING: leave a gap between each of the five hits.";
    case SetupState::disabled: return "TRACKING OFF: enable Melody or Beatbox.";
    case SetupState::belowGate: return "BELOW GATE: sing closer, adjust gain or recalibrate.";
    case SetupState::outsideRange: return "OUTSIDE VOICE RANGE: widen limits or load another voice profile.";
    case SetupState::uncertain: return "PITCH UNCERTAIN: sing one steady note; reduce room noise.";
    case SetupState::tracking: return "VOICE TRACKED: if silent, test preview then MIDI routing.";
    default: return "LISTENING: sing or beatbox. Test note checks the output path.";
    }
}
}
