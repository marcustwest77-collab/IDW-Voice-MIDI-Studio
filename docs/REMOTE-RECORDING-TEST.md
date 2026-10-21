# Remote recording controller: first desktop test

This is an experimental Windows recording-control feature. It does not share
video, microphone audio, completed takes, mixer changes or project edits. The
separate Jitsi video button is unrelated to the recording connection.

Use a build containing the Remote Controller button. Native Windows build and
network checks must pass before testing real recording. Start with disposable
projects and leave any important recordings closed.

## One computer, two IDW windows

The first check needs no hosted server. Python is required separately; the beta
does not install it. Open a terminal in the installed beta folder (normally
`%LOCALAPPDATA%\Programs\IDW Combined Studio Beta`), the extracted portable beta
folder, or this source checkout, then start the included loopback server:

```sh
python -m pip install -r studio_sessions/requirements.txt
python studio_sessions/local_test_room.py
```

Keep that terminal open. It prints one server address, one room ID and two private
seat tickets. This server accepts connections only from this computer. Credentials
expire after one hour or when the process exits. Do not post the tickets in screenshots.

1. Open two IDW windows, A (controller) and B (performer). Do not try to open the
   same exclusive audio device from both windows. Only B needs the recording input.
2. In each, open **IDW Connect > Remote Controller**. Enter the printed server and
   room ID. Use the A ticket in A and the B ticket in B, then click **Join**.
3. In B, add an empty audio track, name it "Remote test", select the microphone
   input, and confirm that local recording works before testing remote commands.
4. In A, click **Request Control**. In B, open **My Permissions**, then choose
   **Allow selected track** or **Allow all tracks**. No command should run merely
   because permission was requested or granted.
5. In A, select "Remote test" from the track list and click **Arm**. B's track
   should become armed. The result in A should report success.
6. Click **Record** in A, speak a short phrase into B's microphone, then click
   **Stop** in A. Confirm that B saves/imports the take and plays it back audibly.
   A successful command result alone does not establish that usable audio was saved.

## Permission and disconnect checks

- With permission for one track, arm an additional unapproved track locally in B.
  Remote Record must be denied as a whole, with no new capture starting.
- Grant all tracks, then test the approved armed-track set using available inputs.
  Local recorder readiness rules still apply.
- While stopped, revoke permission in B. A's track list should clear and commands
  should be disabled. Request permission again to continue.
- During a disposable take, click **Leave** in A. B must retain the take and local
  Stop must still stop/import it. Disconnect does not automatically discard a take.
- Join again. Old permission must not resume; request and grant fresh permission.
- Open a different project in B. Existing permission must be revoked.
- Wait for permission to expire (up to 15 minutes). A should clear its controls.
- If A reports an unknown recording state after a command timeout, check B and use
  local Stop if necessary. The client does not automatically retry commands.

Closing the controller dialog does not leave the room. Use **Leave** explicitly.
The track selector is a snapshot from the latest grant; request fresh permission
after adding or renaming tracks. Only one remote command can await a result at a time.

## Two computers

The loopback address above will not connect two computers. That requires the
signalling service behind a configured HTTPS/WSS endpoint, with its exact origin
in the service allowlist, plus a way to issue separate room tickets. No such server
has been deployed by this project. Native clients reject plain HTTP beyond loopback.
Do not expose the room-creation service key in the DAW or send it to participants.

Record the build revision, Windows version, audio device, and the exact first step
that differs from these expectations. Keep tickets out of logs and screenshots.
