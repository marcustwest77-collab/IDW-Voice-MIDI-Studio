# V5 additions

See ../START-HERE.html for the performance view, channel-separated arrangement, voice profiles and recovery workflow. See V5-VALIDATION.md for current build status. The following earlier setup instructions still apply except that MIDI takes now survive editor closure.

# IDW V4 + FL Studio

## Try the standalone first

1. Extract the Windows ZIP. Run `Windows/Standalone/IDW Voice MIDI Studio.exe`.
2. In **Options > Audio/MIDI Settings**, select the microphone and headphone output. The standalone build supports Windows audio; your DAW may separately use its ASIO driver.
3. Enable **Preview sound**, then press **Test note**. You should hear a short C4 sine tone.
4. Stay quiet and click **Calibrate noise**. Sing a steady note: the pitch display and preview instrument should respond.
5. Keep **Hear microphone** off unless you want the raw vocal signal passed through.

## Use the VST3 in FL Studio

1. Close the DAW before replacing an installed plugin. Save a backup of the previous VST3 bundle.
2. Copy the entire `Windows/VST3/IDW Voice MIDI Studio.vst3` directory into a VST3 folder scanned by FL Studio. The standard system folder is `C:/Program Files/Common Files/VST3/` and may require Windows administrator permission. Do not copy only the inner binary.
3. In FL Studio, use **Options > Manage plugins > Find installed plugins** to rescan. This V4 keeps the same plugin identity as V3; avoid duplicate installations of both versions.
4. Put IDW on the Mixer insert receiving your microphone. Select **Input: Left / mono**, **Right**, or **L + R** to match the input.
5. In IDW's plugin wrapper Settings, assign **MIDI Output port = 10**.
6. Load the destination instrument in the Channel Rack. In its wrapper, assign **MIDI Input port = 10**.
7. Turn off IDW's Preview sound, then press Test note. It sends C4 on MIDI channel 1 independently of pitch detection.
8. Load Clean Vocal, calibrate while quiet, and sing. Match the receiving instrument's bend range to IDW's Bend Range.

The port number 10 is a routing connection, not MIDI channel 10. Melody normally uses MIDI channel 1; drums use MIDI channel 10. MPE uses member channels and needs a compatible instrument.

## If it is silent

| Indicator | Check |
|---|---|
| No input level | Microphone device / Mixer input / input channel |
| Input level but no accepted note | Noise gate, confidence, Melody enabled, voice in 65-1000 Hz range |
| Test note increments MIDI count but synth is silent | Wrapper ports, instrument input channel, instrument output routing |
| Preview works but external synth does not | MIDI routing; preview sound is local to IDW |
| Wrong pitch on slides | Matching pitch-bend range; some synths ignore RPN negotiation |
| Drums unresponsive after training | Enable Beatbox; train distinct pads; reduce hit threshold; reset profiles to basic mode |
| Held note | PANIC, then check the receiving instrument's channel settings |

## Record and export

Set IDW's BPM to your project tempo. Press Record MIDI before performing, stop the take, and Export MIDI. Drag the exported `.mid` file into FL Studio and assign your sounds. Takes survive editor closure; export a project copy. The take includes expression CC and pitch bends; how these import depends on the DAW/instrument.

## Standalone to FL Studio

Select an existing virtual MIDI output in standalone Audio/MIDI Settings, enable that same port as an input in FL Studio, then select the receiving instrument. IDW does not install a virtual MIDI driver. The VST3 route above avoids requiring one.
