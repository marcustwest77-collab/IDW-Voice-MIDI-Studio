"""Non-secret private-pipe readiness codec. No I/O, TLS proof or native authority."""
from dataclasses import dataclass


@dataclass(frozen=True)
class NativeBridgeReady:
    epoch: int
    tls_port: int

    def __post_init__(self):
        if (type(self.epoch) is not int or not 1 <= self.epoch <= 2**64 - 1 or
                type(self.tls_port) is not int or not 1 <= self.tls_port <= 65535):
            raise ValueError("invalid bridge readiness metadata")

    def encode(self):
        return b"IDWR\x01" + self.epoch.to_bytes(8, "big") + self.tls_port.to_bytes(2, "big")

    @classmethod
    def decode(cls, data):
        if type(data) is not bytes or len(data) != 15 or data[:5] != b"IDWR\x01":
            raise ValueError("invalid bridge readiness wire record")
        return cls(int.from_bytes(data[5:13], "big"), int.from_bytes(data[13:15], "big"))
