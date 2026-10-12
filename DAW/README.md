# V14 DAW foundation

This is the start of a **new standalone DAW application**, separate from the shipping IDW voice-to-MIDI plugin.

Implemented: dark workstation window, ruler, audio/MIDI track rows, add-track controls, BPM adjustment, playhead display, prototype play/stop transport, and a host-independent session model with a regression test.

Not implemented: audio-engine transport, microphone recording, waveforms, MIDI clips, mixer, VST3 hosting, session save/load, editing or bounce/export. The animated playhead is driven by a UI timer, **not** an audio sample clock.

Build: configure the repository with `-DIDW_BUILD_DAW=ON` and compile target `IDWDAW`. Do not distribute as a production DAW. Next: audio-clock-based transport, file-backed session serialization, and track playback/recording.
