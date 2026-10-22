"""Start one private, loopback-only room for native-controller testing.

Run: python studio_sessions/local_test_room.py
Open two IDW DAW instances on this same computer and use the separate seat tickets.
The printed credentials expire when this process exits or after one hour.
"""
import argparse
import secrets
from aiohttp import web
from server import Rooms, create_app


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8787)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("port must be between 1 and 65535")
    origin = f"http://127.0.0.1:{args.port}"
    rooms = Rooms(capacity=1)
    app = create_app(secrets.token_urlsafe(32), {origin}, rooms)

    async def ready(app):
        invite = rooms.create()
        print("\nIDW LOCAL RECORDING-CONTROL TEST", flush=True)
        print("Use two IDW windows on THIS computer. No public server is opened.", flush=True)
        print(f"Server: {origin}\nRoom ID: {invite['room']}", flush=True)
        print(f"Window A ticket: {invite['host_ticket']}", flush=True)
        print(f"Window B ticket: {invite['guest_ticket']}", flush=True)
        print("Keep tickets private. They expire in one hour; restart for a new room.", flush=True)
        print("Close this process with Ctrl+C to disconnect both participants.\n", flush=True)

    app.on_startup.append(ready)
    web.run_app(app, host="127.0.0.1", port=args.port, access_log=None)


if __name__ == "__main__":
    main()
