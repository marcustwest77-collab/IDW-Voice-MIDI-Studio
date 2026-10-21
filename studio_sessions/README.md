# IDW two-person studio sessions: connection service prototype

Status: local protocol prototype, not deployed and not connected to the desktop
Connect panel. The existing installed Connect button still opens Jitsi in a
browser. This directory does not yet provide camera capture, audio/video transport,
DAW audio sharing, remote recording or synchronized playback.

## Intended in-DAW experience

- The host creates a session; a separate guest invitation admits one collaborator.
- The IDW Connect view contains both cameras, microphone/camera toggles and Leave.
- Talk microphone and DAW mix routing are separate. The guest's returned audio is
  excluded from the outgoing studio mix to prevent feedback.
- Closing a call releases its media devices. Leaving the video panel must have a
  clear, visible call-state indicator so a hidden view never obscures an active mic.
- Remote takes are a later phase: retain original local recordings and transfer
  them for alignment. Video-call audio is not the original recording.

## What is implemented

The service creates a room with separate random host/guest tickets. It stores
only ticket hashes and permits one connected socket per role. Tickets are sent
in the first WebSocket frame, not URL query strings. A disconnected role can
rejoin with the same ticket until room expiry; a copied ticket cannot occupy a
second simultaneous seat. Room IDs are not credentials. Invitations are bearer
credentials, not verified identities. Rooms expire after one hour by default.

Messages contain only offers, answers and connection candidates for WebRTC.
The service validates their shapes, limits frame size and signalling rate, assigns
sender identity itself and relays only to the other seat in that room. It never
receives recorded audio or camera frames. Signalling data can contain network
addresses, so transport security remains necessary.

## Local developer checks

Use Python 3.12 or newer in a virtual environment:

```sh
python -m pip install -r studio_sessions/requirements.txt
python -m unittest discover -s studio_sessions -p 'test_*.py' -v
```

For a local service, set `IDW_STUDIO_CREATE_KEY` to a random service secret of at
least 32 characters and `IDW_STUDIO_ORIGINS` to comma-separated exact client origins,
then run `python studio_sessions/server.py`. It binds only to 127.0.0.1:8787.
Never embed the service creation key in a shipped desktop app or public web page.

## Protocol

1. A trusted room broker calls POST `/v1/rooms` with the creation service key in
   the Bearer Authorization header. The response contains `room`, `host_ticket`,
   `guest_ticket` and `expires_in_seconds`. It must distribute only the appropriate
   role's ticket to each participant; the host ticket is not a guest invitation.
2. Each participant connects to `/v1/connect` with an allowed Origin and sends
   `{ "type": "join", "room": "...", "ticket": "..." }` within ten seconds.
3. The server replies `joined` with assigned `role` and `peer_present`, and notifies
   the other seat with `peer_joined`. The host initiates negotiation when both are
   present. On peer reconnection the client must discard the old peer connection
   and negotiate a new one; the server does not buffer old offers or candidates.
4. Relay `{ "type": "offer"|"answer", "sdp": "..." }` or
   `{ "type": "candidate", "candidate": <RTCIceCandidate.toJSON() or null> }`.
   The server adds `from`. Do not send a client-controlled `from` or destination.
5. Send `{ "type": "leave" }` or close the socket. The remaining participant
   receives `peer_left` and must close the old media connection.

## Next integration gates

1. Build the IDW-owned call page and embed it with Windows WebView2. Verify camera
   and microphone permission prompts, device selection, mute, camera-off, Leave,
   application shutdown, network loss and reconnect. No automatic media capture.
2. Add an authenticated room broker, HTTPS/WSS hosting, credential rotation,
   creation rate limits and short-lived TURN credentials. Creation service keys
   stay on the server. Origin validation is not a substitute for ticket auth.
3. Validate two real devices across different networks, including TURN relay,
   third-seat rejection, expired invitations and host/guest reconnects.
4. Add AudioBox-aware talk/mix routing with bounded audio queues outside the DAW
   callback and a mix-minus return path. Test headphones and speaker echo behavior.
5. Add remote take transfer/alignment separately. Do not promise latency-free
   shared recording from a working video call.

No hosting account, paid service, public endpoint or external invitation is
created by this prototype. Server capacity and production deployment remain open
engineering work. Local protocol tests are not a completed video-session test.
