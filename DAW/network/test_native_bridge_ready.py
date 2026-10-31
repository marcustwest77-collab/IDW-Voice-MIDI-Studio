import unittest
from dataclasses import FrozenInstanceError
from native_bridge_ready import NativeBridgeReady


class ReadyCodecTests(unittest.TestCase):
    def test_exact_cpp_wire_fixture_roundtrip_and_immutable_metadata(self):
        ready = NativeBridgeReady(45, 12345)
        wire = b"IDWR\x01" + b"\x00" * 7 + b"\x2d\x30\x39"
        self.assertEqual(ready.encode(), wire)
        self.assertEqual(NativeBridgeReady.decode(wire), ready)
        self.assertEqual(NativeBridgeReady.decode(NativeBridgeReady(2**64-1, 65535).encode()),
                         NativeBridgeReady(2**64-1, 65535))
        with self.assertRaises(FrozenInstanceError): ready.tls_port = 1

    def test_malformed_or_mutable_records_and_out_of_range_fields_fail_closed(self):
        wire = NativeBridgeReady(45, 12345).encode()
        invalid = [wire[:-1], wire+b"x", bytearray(wire), "text", None,
                   b"IDWR\x01" + b"\x00"*8 + b"\x30\x39", wire[:-2]+b"\x00\x00"]
        for offset in range(5): invalid.append(wire[:offset]+b"\xff"+wire[offset+1:])
        for value in invalid:
            with self.assertRaises(ValueError): NativeBridgeReady.decode(value)
        for epoch, port in ((0, 1), (2**64, 1), (True, 1), (1, True), (1, 0), (1, 65536), (1, 1.0)):
            with self.assertRaises(ValueError): NativeBridgeReady(epoch, port)


if __name__ == "__main__": unittest.main()
