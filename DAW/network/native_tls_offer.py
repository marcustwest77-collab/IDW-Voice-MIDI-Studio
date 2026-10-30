"""Non-secret verified-bridge session offer. Encoding does not verify TLS."""
from dataclasses import dataclass
import re


@dataclass(frozen=True)
class NativeTLSOffer:
    epoch: int
    peer_sha256: str
    connection_id: str
    tls_version: str

    def encode(self):
        if (type(self.epoch) is not int or not 1 <= self.epoch <= 2**64 - 1 or
                any(not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{64}", value)
                    for value in (self.peer_sha256, self.connection_id)) or
                self.tls_version not in ("TLSv1.2", "TLSv1.3")):
            raise ValueError("invalid native TLS offer")
        return (b"IDWO\x01" + self.epoch.to_bytes(8, "big") + self.peer_sha256.encode("ascii") +
                self.connection_id.encode("ascii") + bytes([12 if self.tls_version == "TLSv1.2" else 13]))
