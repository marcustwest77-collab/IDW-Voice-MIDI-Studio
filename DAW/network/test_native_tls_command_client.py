"""Exact receipt, canonical requests, no retry and nonrenewable client budgets."""
from contextlib import contextmanager
from dataclasses import FrozenInstanceError
import time
import unittest
from unittest.mock import MagicMock, patch

from native_tls_command_client import NativeQueueReceipt, open_native_command_client

WIRE=b"1|45|1|client_request|arm|1"


class CommandClientTests(unittest.TestCase):
    def options(self, **changes):
        options=dict(epoch=45,approved_server_sha256="a"*64,server_ca_pem="owner PEM",
            certificate_file="owner certificate",private_key_file="owner key")
        options.update(changes);return options

    @contextmanager
    def client(self, **options):
        secure=MagicMock()
        secure.recv.side_effect=[len(WIRE).to_bytes(4,"big"),WIRE]
        @contextmanager
        def connected(*args,**kwargs): yield secure
        with patch("native_tls_command_client.connect_native_tls_client",side_effect=connected) as connect:
            with open_native_command_client(12345,**self.options(**options)) as client:
                yield client,secure,connect

    def test_invalid_scope_never_connects(self):
        cases=[dict(epoch=v) for v in (0,True,2**64)]
        cases += [dict(max_commands=v) for v in (0,True,33)]
        cases += [dict(command_timeout=v) for v in (0,True,6,float("nan"),float("inf"),10**1000)]
        cases += [dict(session_timeout=v) for v in (0,True,61,float("nan"))]
        with patch("native_tls_command_client.connect_native_tls_client") as connect:
            for changes in cases:
                with self.subTest(changes=changes),self.assertRaises(ValueError):
                    with open_native_command_client(12345,**self.options(**changes)): self.fail("connected")
            connect.assert_not_called()

    def test_no_automatic_command_and_exact_fragmented_queue_receipt(self):
        with self.client() as (client,secure,connect):
            secure.sendall.assert_not_called()
            header=len(WIRE).to_bytes(4,"big")
            secure.recv.side_effect=[bytes([x]) for x in header]+[WIRE[:3],WIRE[3:]]
            receipt=client.submit(WIRE)
            self.assertEqual(receipt,NativeQueueReceipt(45,1,"client_request"))
            with self.assertRaises(FrozenInstanceError): receipt.epoch=46
            self.assertFalse(hasattr(receipt,"executed"))
            secure.sendall.assert_called_once_with(header+WIRE)
            self.assertEqual(connect.call_args.args,(12345,))
        secure.close.assert_called_once()

    def test_invalid_local_command_has_no_io_and_does_not_poison_connection(self):
        cases=[bytearray(WIRE),b"broken",WIRE.replace(b"|45|",b"|46|"),WIRE.replace(b"|1|client",b"|01|client"),
            WIRE.replace(b"|arm|1",b"|record|1"),WIRE.replace(b"|arm|1",b"|arm|128")]
        with self.client() as (client,secure,_):
            for wire in cases:
                with self.assertRaises(ValueError): client.submit(wire)
            secure.sendall.assert_not_called();secure.recv.assert_not_called();secure.close.assert_not_called()
            self.assertEqual(client.submit(WIRE).request_id,"client_request")

    def test_replay_duplicate_request_and_capacity_reject_before_io(self):
        with self.client(max_commands=2) as (client,secure,_):
            client.submit(WIRE)
            for wire in (WIRE,WIRE.replace(b"|1|client",b"|2|client")):
                with self.assertRaises(ValueError): client.submit(wire)
            second=b"1|45|2|second|select|0"
            secure.recv.side_effect=[len(second).to_bytes(4,"big"),second]
            client.submit(second)
            with self.assertRaises(ValueError): client.submit(b"1|45|3|third|arm|1")
            self.assertEqual(secure.sendall.call_count,2)

    def test_any_uncertain_exchange_retires_connection_without_retry(self):
        for mode in ("send","eof","truncated","oversize","zero","mismatch","timeout"):
            with self.subTest(mode=mode),self.client() as (client,secure,_):
                if mode=="send": secure.sendall.side_effect=OSError("uncertain send")
                if mode=="eof": secure.recv.side_effect=[b""]
                if mode=="truncated": secure.recv.side_effect=[b"\0\0",b""]
                if mode=="oversize": secure.recv.side_effect=[(4097).to_bytes(4,"big")]
                if mode=="zero": secure.recv.side_effect=[b"\0"*4]
                if mode=="mismatch": secure.recv.side_effect=[len(WIRE).to_bytes(4,"big"),WIRE[:-1]+b"2"]
                if mode=="timeout": secure.recv.side_effect=TimeoutError("slow receipt")
                with self.assertRaises((OSError,ValueError)): client.submit(WIRE)
                secure.close.assert_called_once()
                with self.assertRaises(RuntimeError): client.submit(WIRE)
                secure.sendall.assert_called_once()

    def test_fragmented_receipt_cannot_renew_command_budget(self):
        with self.client(command_timeout=1) as (client,secure,_):
            with patch("native_tls_command_client.time.monotonic",side_effect=[10,10.1,10.8,11.1]):
                with self.assertRaises(TimeoutError): client.submit(WIRE)
            secure.close.assert_called_once()
            self.assertAlmostEqual(secure.settimeout.call_args_list[0].args[0],.9)
            self.assertAlmostEqual(secure.settimeout.call_args_list[1].args[0],.2)

    def test_session_deadline_is_not_renewed_between_commands(self):
        with self.client() as (client,secure,_):
            client.submit(WIRE)
            with patch("native_tls_command_client.time.monotonic",return_value=time.monotonic()+100):
                with self.assertRaises(TimeoutError): client.submit(b"1|45|2|second|arm|1")
            secure.sendall.assert_called_once();secure.close.assert_called_once()

    def test_connection_time_cannot_renew_session_budget(self):
        secure=MagicMock()
        @contextmanager
        def connected(*args,**kwargs): yield secure
        with patch("native_tls_command_client.connect_native_tls_client",side_effect=connected) as connect, \
                patch("native_tls_command_client.time.monotonic",side_effect=[10,11]):
            with self.assertRaises(TimeoutError):
                with open_native_command_client(12345,**self.options(session_timeout=.5)):
                    self.fail("expired session yielded")
            self.assertEqual(connect.call_args.kwargs['timeout'],.5)
            secure.sendall.assert_not_called();secure.close.assert_called_once()

    def test_close_or_caller_failure_retires_client(self):
        with self.client() as (client,secure,_):
            client.close();client.close()
            with self.assertRaises(RuntimeError): client.submit(WIRE)
            secure.sendall.assert_not_called()
        secure.close.assert_called_once()
        with self.assertRaisesRegex(RuntimeError,"caller failure"):
            with self.client() as (client,secure,_): raise RuntimeError("caller failure")
        secure.close.assert_called_once()


if __name__=="__main__": unittest.main()
