# V14 microphone recording acceptance checklist

Build status is not a substitute for a real microphone test.

1. Select the correct Windows/macOS microphone input and output devices; use headphones to avoid feedback.
2. Open a short WAV instrumental in an audio track. Confirm playback and stop.
3. Press RECORD VOCAL, speak or sing for 5 seconds, then press STOP RECORD. Verify a nonempty PCM WAV appears under Documents/IDW DAW/Recordings and a track clip appears.
4. Play the session from the start and confirm the take is audible and aligned approximately with its intended start beat.
5. Start playback of an instrumental, press RECORD VOCAL partway through, record a phrase, then press the main STOP button. Verify recording is finalized, the take file is readable, and its clip enters at the recorded start beat.
6. Inspect the resulting WAV with an audio editor: check channel count, duration, audible audio, no obvious clipping, and no truncated final frames.
7. Test microphone denial, device unplug, 44.1kHz/48kHz, buffers 128/512/1024, and repeated rapid Record/Stop cycles.
8. Confirm reopening / switching audio devices does not crash and the previous take remains intact.

Known limitations: no calibrated device latency compensation, punch-in/out, pre-roll, take lanes or saved DAW sessions. Audio graph edits during active recording and playback need further thread-safety review. Do not treat this as a certified recording release.
