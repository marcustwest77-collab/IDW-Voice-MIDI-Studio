"""Run the native Windows WSS client against a real loopback signalling server.
Uses HTTP only on 127.0.0.1, ephemeral test tickets, and a fake recording engine.
"""
import asyncio
import os
import sys
from aiohttp import web
from server import Rooms, create_app

async def main(executable):
    rooms = Rooms()
    # Bind first to discover an unused port, then fill the origin allowlist.
    origins = set()
    runner = web.AppRunner(create_app("test-only-creation-key-00000000000000", origins, rooms))
    await runner.setup()
    site = web.TCPSite(runner, "127.0.0.1", 0)
    await site.start()
    port = site._server.sockets[0].getsockname()[1]
    origin = f"http://127.0.0.1:{port}"
    origins.add(origin)
    invite = rooms.create()
    env = dict(os.environ, IDW_TEST_ORIGIN=origin, IDW_TEST_ROOM=invite["room"],
               IDW_TEST_HOST=invite["host_ticket"], IDW_TEST_GUEST=invite["guest_ticket"])
    process = None
    try:
        process = await asyncio.create_subprocess_exec(executable, env=env)
        return await asyncio.wait_for(process.wait(), timeout=45)
    finally:
        if process is not None and process.returncode is None:
            process.kill()
            await process.wait()
        await runner.cleanup()

if __name__ == "__main__":
    raise SystemExit(asyncio.run(main(sys.argv[1])))
