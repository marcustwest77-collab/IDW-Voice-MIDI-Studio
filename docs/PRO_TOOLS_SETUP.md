# Pro Tools Intro + IDW V10 on Windows

IDW connects to Pro Tools Intro through the standalone application and a virtual MIDI cable. The included plugin is VST3; Pro Tools uses AAX and therefore will not list the IDW VST3. V10 does not include an AAX build. For corrected vocal audio, use Audio Lab Tune Studio.

## Signal path

`Microphone -> AudioBox USB 96 -> IDW standalone -> virtual MIDI port -> Pro Tools Instrument track -> virtual instrument -> AudioBox headphones/outputs`

## One-time setup

1. Install the current PreSonus driver/Universal Control for the AudioBox USB 96.
2. Install a virtual MIDI cable such as loopMIDI. Create a port named `IDW Voice MIDI` and leave loopMIDI running.
3. Connect headphones to the AudioBox. Start with the input gain low. Raise it while singing, but keep IDW's meter out of red and avoid the interface Clip indication.
4. Enable the AudioBox **48V** switch only if the connected condenser microphone requires phantom power. Do not assume every microphone needs it.

## IDW standalone settings

1. Start **IDW Voice MIDI Studio V10**.
2. Open **Options > Audio/MIDI Settings**.
3. Select the AudioBox USB 96 ASIO device.
4. Enable the microphone socket you used, normally **Input 1**. Select **Main Out 1/2** for output.
5. Start at **48 kHz / 256 samples**. After the complete path works, try 128 samples for lower latency. If you hear pops or dropouts, return to 256 or raise the buffer.
6. Select `IDW Voice MIDI` as the MIDI output.
7. Open **Setup / Help**, choose **Pro Tools Intro / Windows**, and click **Mute IDW sounds / use DAW**.

## Pro Tools Intro settings

1. Open or create a session at the same sample rate selected for the AudioBox in IDW.
2. Go to **Setup > MIDI > Input Devices** and enable `IDW Voice MIDI`.
3. Create a **stereo Instrument track**.
4. Insert the virtual instrument you want to play.
5. Set the track's MIDI input to `IDW Voice MIDI`, channel 1 (or All if channel 1 is unavailable).
6. Record-arm the Instrument track and enable the monitoring/MIDI Thru behavior required by the session.
7. Return to IDW and press **Send test note**. The Pro Tools instrument should play C4.
8. Check **I heard this test note** only after the note is actually audible. Then calibrate noise in a quiet room and sing a steady note.

## Layer channels

| IDW part | MIDI channel |
|---|---:|
| Lead | 1 |
| Chords | 2 |
| Bass | 3 |
| Beatbox drums | 10 |

For a first test, use one Instrument track on channel 1, Harmony off and MPE off. Add separate receiving tracks for channels 2, 3 and 10 only after the lead route works.

## Correct a vocal with Tune Studio

Because Pro Tools Intro cannot load the IDW VST3, use this file workflow for Vocal FX:

1. Record a dry lead vocal through the AudioBox. Avoid printing reverb, chorus or delay.
2. Consolidate the vocal from the exact song start, then export it as 16-bit PCM WAV at the same sample rate as the session.
3. Open **IDW Audio Lab**, choose the WAV, and select **Tune Studio**.
4. Start with Natural or Smooth R&B. Choose the song root and scale for scale-safe tuning.
5. Click **Create corrected WAV copy**. The source is never overwritten and local tuning does not upload it.
6. Import the `-IDW-Tuned.wav` file onto a new Pro Tools audio track at the same session start.
7. Mute the dry track to compare. Keep it in the session as the editable original.

## Fast fault isolation

| What you see | Meaning | Next action |
|---|---|---|
| IDW says Audio stopped | IDW is not receiving callbacks | Reopen Audio/MIDI Settings and select the AudioBox ASIO device |
| Input stays near -100 dBFS | No microphone signal | Check Input 1/2, cable, mic power requirement, and AudioBox gain |
| Input moves but MIDI count stays zero | Voice is rejected | Enable Melody, calibrate noise, use Clean Vocal, then sing one steady note |
| Test note raises IDW MIDI count but Pro Tools sees nothing | Virtual MIDI route is broken | Confirm loopMIDI is running, the same port is selected in both apps, and it is enabled in Pro Tools Input Devices |
| Pro Tools MIDI meter moves but there is no sound | Instrument/output path is incomplete | Load an instrument, record-arm/monitor the track, and verify the AudioBox output |
| Sound crackles | Buffer is too small or devices disagree | Match sample rates and raise both applications to a 256- or 512-sample buffer |
| You hear doubled voice/echo | More than one monitor path is active | Turn off IDW mic monitoring or rebalance the AudioBox Mixer knob; keep one intended monitor path |

Use **Copy setup report** in IDW after selecting the Pro Tools profile. It copies the current signal readings, generated MIDI counts, confirmation state and these route instructions; it never copies microphone audio.
