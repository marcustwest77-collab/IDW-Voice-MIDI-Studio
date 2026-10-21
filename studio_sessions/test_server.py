import unittest
from aiohttp import WSMsgType
from aiohttp.test_utils import AioHTTPTestCase
from server import Rooms, create_app, valid_signal

KEY = "test-only-service-key-000000000000000000"
ORIGIN = "https://studio.example.test"


class AdmissionTests(unittest.TestCase):
    def test_exactly_two_seats_and_reconnect(self):
        rooms = Rooms(); invite = rooms.create()
        a, b, c = object(), object(), object()
        self.assertEqual(rooms.claim(invite["room"], invite["host_ticket"], a), "host")
        self.assertEqual(rooms.claim(invite["room"], invite["guest_ticket"], b), "guest")
        for ticket in (invite["host_ticket"], invite["guest_ticket"], "wrong"):
            with self.assertRaises(ValueError): rooms.claim(invite["room"], ticket, c)
        self.assertFalse(rooms.release(invite["room"], "guest", c))
        self.assertTrue(rooms.release(invite["room"], "guest", b))
        self.assertEqual(rooms.claim(invite["room"], invite["guest_ticket"], c), "guest")

    def test_expiry_and_capacity(self):
        clock = [0]; rooms = Rooms(clock=lambda:clock[0], ttl=5, capacity=1)
        invite = rooms.create()
        with self.assertRaises(ValueError): rooms.create()
        clock[0] = 5
        with self.assertRaises(ValueError): rooms.claim(invite["room"], invite["host_ticket"], object())
        self.assertNotEqual(rooms.create()["room"], invite["room"])

    def test_cross_room_ticket(self):
        rooms = Rooms(); a, b = rooms.create(), rooms.create()
        with self.assertRaises(ValueError): rooms.claim(a["room"], b["guest_ticket"], object())

    def test_signal_validation(self):
        self.assertTrue(valid_signal({"type":"offer","sdp":"test"}))
        self.assertTrue(valid_signal({"type":"candidate","candidate":None}))
        for payload in ([], {}, {"type":"offer","sdp":"x","from":"host"},
                        {"type":"offer","sdp":1}, {"type":"candidate","candidate":{"candidate":"x","sdpMLineIndex":True}}):
            self.assertFalse(valid_signal(payload))


class ProtocolTests(AioHTTPTestCase):
    async def get_application(self):
        return create_app(KEY, {ORIGIN})

    async def room(self):
        response = await self.client.post("/v1/rooms", headers={"Authorization":"Bearer "+KEY})
        self.assertEqual(response.status, 201)
        self.assertEqual(response.headers["Cache-Control"], "no-store")
        return await response.json()

    async def join(self, invite, role):
        ws = await self.client.ws_connect("/v1/connect", headers={"Origin":ORIGIN})
        await ws.send_json({"type":"join","room":invite["room"],"ticket":invite[role+"_ticket"]})
        return ws

    async def test_create_requires_service_auth(self):
        response = await self.client.post("/v1/rooms")
        self.assertEqual(response.status, 401)

    async def test_relay_leave_and_reconnect(self):
        invite = await self.room()
        host = await self.join(invite, "host")
        self.assertEqual((await host.receive_json())["role"], "host")
        guest = await self.join(invite, "guest")
        self.assertTrue((await guest.receive_json())["peer_present"])
        self.assertEqual((await host.receive_json())["type"], "peer_joined")
        await host.send_json({"type":"offer","sdp":"test-offer"})
        self.assertEqual(await guest.receive_json(), {"type":"offer","sdp":"test-offer","from":"host"})
        await guest.send_json({"type":"answer","sdp":"test-answer"})
        self.assertEqual((await host.receive_json())["from"], "guest")
        await guest.close()
        self.assertEqual((await host.receive_json())["type"], "peer_left")
        again = await self.join(invite, "guest")
        self.assertEqual((await again.receive_json())["role"], "guest")
        await again.close(); await host.close()

    async def test_third_connection_rejected(self):
        invite = await self.room()
        host = await self.join(invite, "host"); await host.receive_json()
        guest = await self.join(invite, "guest"); await guest.receive_json(); await host.receive_json()
        third = await self.join(invite, "guest")
        self.assertEqual((await third.receive()).type, WSMsgType.CLOSE)
        self.assertEqual(third.close_code, 1008)
        await guest.close(); await host.close()

    async def test_bad_origin_rejected(self):
        response = await self.client.get("/v1/connect", headers={"Origin":"https://other.example.test"})
        self.assertEqual(response.status, 403)

    async def test_no_cross_room_or_spoofed_sender(self):
        a, b = await self.room(), await self.room()
        a["guest_ticket"] = b["guest_ticket"]
        bad = await self.join(a, "guest")
        self.assertEqual((await bad.receive()).type, WSMsgType.CLOSE)
        host = await self.join(a, "host"); await host.receive_json()
        await host.send_json({"type":"offer","sdp":"x","from":"guest"})
        self.assertEqual((await host.receive()).type, WSMsgType.CLOSE)
        self.assertEqual(host.close_code, 1008)


if __name__ == "__main__":
    unittest.main()
