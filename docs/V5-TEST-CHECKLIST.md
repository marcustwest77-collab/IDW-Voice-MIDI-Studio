# V5 live test checklist

1. Extract and run the built EXE. Check that the heading says Version 5. Preview + Test note must produce the lead tone.
2. Choose the microphone in Options > Audio/MIDI Settings. In Studio controls, enable Melody, disable MPE, calibrate, sing, and verify a lead note.
3. Return to Performance view. Select Major, C, Triad and Bass. In a DAW instrument with per-part channel filtering, assign lead to 1, chords to 2, bass to 3. Singing middle C should generate chord C/E/G and bass C two octaves below. IDW's preview remains lead-only.
4. Change voicing to Seventh and check C/E/G/B. In C natural minor, check C/E-flat/G/B-flat. Change roots while holding a note; check old layer notes release. Turn harmony off and confirm lead continues alone.
5. Enable MPE in Studio: check the performance notice says layers are suppressed. Disable MPE to return to separate-channel arrangement. Test Panic and silence for hanging notes on all used channels.
6. Type a unique voice profile name. Learn range for 12 seconds, singing low and high. Save. Change gate/range, reload and confirm restored calibration. Confirm loading this profile does not change musical harmony settings. Test with two profiles for different microphones.
7. Record before singing. Close and reopen the plugin editor while still recording. The take must remain and recording must continue. Stop, export and verify the resulting notes/bends in the DAW.
8. Start another take; the previous take must be checkpointed. Stop and close the host. Reopen IDW, press Recover take, choose the dated MIDI backup, export and compare the tempo and notes. Backups remain until manually removed.
9. Open an older V4 session/preset in a copy of your DAW project. Harmony must default off, vocal range unrestricted; verify original sounds and mappings. Then save/reopen a new V5 session and verify all new parameters.
10. Inspect Performance and Studio views at default/minimum window sizes. Verify every visible control is reachable and the help overlay closes correctly.
