# IDW Voice MIDI Studio — V14 development

Experimental standalone JUCE workstation (separate from the V13 voice-to-MIDI plugin).

Current code includes multitrack audio playback, basic vocal capture, session persistence, offline WAV bounce, master EQ, resampling, and automated tests. These are **not** yet validated on all physical audio devices.

## IDW Connect — two-person video (first integration)

The **IDW CONNECT** button in the DAW opens a collaboration panel:
1. Choose **CREATE PRIVATE CODE** to generate a random room identifier.
2. Choose **COPY INVITE** and share it directly with one trusted collaborator.
3. Both participants choose **OPEN VIDEO ROOM**, or the recipient opens the supplied link.

The video meeting runs in the default browser through the third-party Jitsi Meet service. Camera and microphone permissions are handled by the browser. Do not share room codes publicly. Room identifiers are not passwords; meeting admission behavior and data handling depend on the external service.

**Scope and limitations:** IDW Connect currently provides invitation and launch UI only. It does not embed a camera view inside JUCE, enforce a two-participant maximum, stream DAW output, synchronize transports, transmit recordings, share project files, or provide end-to-end validated recording collaboration. Do not enable speaker-to-microphone monitoring without headphones: feedback or echo may occur. Production deployment should add access controls, service configuration, tests, and a privacy review.

## Build

Configure with `-DIDW_BUILD_DAW=ON` and compile `IDWDAW`. The V14 DAW workflow runs Windows/macOS build and regression tests on branch `v14-daw-foundation`.

## Remaining DAW priorities

- Test hardware microphone/monitor timing and track-recording latency.
- Test longer sessions and concurrent recording/export use cases.
- Improve timeline editing, project reliability, MIDI/audio routing, and plugin hosting.
- Add video embedded view, secure admission and synchronized collaboration as future, separately tested milestones.
