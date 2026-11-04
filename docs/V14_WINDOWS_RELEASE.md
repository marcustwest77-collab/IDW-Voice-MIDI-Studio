# IDW DAW V14 Windows beta

This package builds the standalone V14 workstation, separate from the older
Voice MIDI Studio VST3/Standalone product. It does not install those plugins.

## Included

- Neo Studio interface with Neo Cyan, Royal Gold, Emerald Circuit and Purple Night.
- SKIN TOOLS for custom background, panel and accent colors, and named skin
  save/load/delete/import/export. Skin selection and colors persist between launches.
- Existing V14 audio recording, overdubbing, track mixing, timeline editing,
  project save/open, WAV export and recording diagnostics.
- Existing IDW Connect and IDW AI Studio panels.
- AI/idw_mcp_server.py: optional Python helper for an MCP-capable host.
  Requires a separately installed Python 3 interpreter and host configuration.
  It does not automatically connect ChatGPT or Claude. Proposals still require
  review and approval in the DAW. See DEVELOPMENT-NOTES.md for protocol details;
  that file is a chronological development log, not a list of release promises.

## Install or run portable

The setup EXE installs for the current Windows user without administrator rights.
The portable package can instead be extracted into a new folder you own.
Launch IDW-DAW-V14-Beta.exe. Keep your existing working beta folder.
Choose your microphone/interface and headphone output in audio settings.
The four skins are built into the application; no separate skin download is needed.

## Projects, skins and rollback

Use copies of projects when evaluating a newer build and save under a new name.
Keep source audio together with your project backups. Recordings are written under
Documents/IDW DAW/Recordings; skin preferences are under the user's application-data
folder in IDW Neo Studio. The installer does not erase either location or change
sound-device settings. Uninstall removes installed program files, not user projects.
To roll back, close the new app and open the previous beta with a backup project.
No automatic project migration, audio-device reconfiguration or Python installation occurs.

## Build identity and limits

BUILD-INFO.json records the source commit, CI run, file version and signing status.
SHA256.txt records staged-file checksums. The DAW file version is now 14.0.0.0;
legacy Voice MIDI Studio keeps its own version number.
This remains an unsigned beta unless BUILD-INFO.json reports otherwise.
The earlier audio acceptance passes were user-reported for the prior tested binary;
a rebuilt binary is not automatically covered by those passes.
Clean-Windows installation, uninstall/rollback and physical-device compatibility
remain release gates. CI compilation is not a substitute for those checks.
IDW Connect opens a browser meeting; it does not synchronize DAW sessions.
AI integration is an experimental approval workflow, not embedded provider chat.
