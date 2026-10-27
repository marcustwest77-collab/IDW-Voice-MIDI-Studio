import unittest
from command_protocol import parse_command


class CommandProtocolTest(unittest.TestCase):
    def test_valid_data_only(self):
        result = parse_command(b"1|24|7|request_7|arm|2")
        self.assertEqual(result["track_index"], 2)
        self.assertEqual(result["action"], "arm")

    def test_invalid(self):
        for data in (
            b"1|24|7|request_7|delete|2",
            b"1|24|7|../bad|arm|2",
            b"1|24|7|request_7|arm|-2",
            b"1|24|0|request_7|arm|2",
            b"1|24|7|request_7|arm|2|extra",
            b"1|24|7|request_7|arm|18446744073709551616",
            b"1|24|7|request_7|arm|2\xff",
        ):
            with self.subTest(data=data):
                with self.assertRaises(ValueError):
                    parse_command(data)


if __name__ == "__main__":
    unittest.main()
