# V14 experimental Host Review Panel

`DAW/RemoteHostReviewPanel.h` is a JUCE Component for the host's Approve/Deny UI. It displays a caller-supplied list from `HostReviewController::pending(grant)`. It does not authenticate or grant access, perform queue mutations, or dispatch recording operations on its own.

An owning DAW UI must call `showPending()` on the JUCE message thread, connect `onHostDecision` only to trusted *local host* interaction, and use `HostReviewController::approve` with a fresh `Grant` and `Request` reflecting live DAW state, or `deny`. After each decision, refresh the display. Approved commands must be revalidated in a future separately reviewed dispatch layer before any operation.

This header has **not yet been wired into the DAW main window or independently compiled**. No transport, video, or remote recording is operational from the panel.