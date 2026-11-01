"""Bounded canonical records, exact decision scope and C++ interoperability."""
from dataclasses import replace
import os
import subprocess
import threading
import time
import unittest

from native_command_review import NativeCommandPrompt, NativeCommandReview
from native_command_approval import NativeCommandApprovalLedger
from native_tls_session import TLSConnectionIdentity
from native_command_review_wire import (encode_command_proposal,decode_command_proposal,
    encode_command_decision,match_command_decision,MAX_PROPOSAL_BYTES)

PROMPT=NativeCommandPrompt(7,TLSConnectionIdentity("guest",45,"a"*64,"b"*64,"TLSv1.3"),
    b"1|45|1|review|arm|1","c"*32)


class ReviewWireTests(unittest.TestCase):
    def test_exact_golden_record_and_both_explicit_decisions(self):
        offer=b"IDWO\x01"+(45).to_bytes(8,"big")+b"a"*64+b"b"*64+b"\x0d"
        expected=b"IDWQ\x01"+(7).to_bytes(8,"big")+b"c"*32+b"\x05guest"+offer+len(PROMPT.payload).to_bytes(2,"big")+PROMPT.payload
        self.assertEqual(encode_command_proposal(PROMPT),expected)
        self.assertEqual(decode_command_proposal(expected),PROMPT)
        for allow in (False,True):
            decision=encode_command_decision(PROMPT,allow)
            self.assertEqual(decision,b"IDWD\x01"+bytes([int(allow)])+expected)
            self.assertIs(match_command_decision(decision,PROMPT),allow)

    def test_every_truncation_trailing_byte_and_invalid_length_rejects(self):
        data=encode_command_proposal(PROMPT)
        for size in range(len(data)):
            with self.assertRaises(ValueError): decode_command_proposal(data[:size])
        cases=[data+b"\0",b"x"*(MAX_PROPOSAL_BYTES+1),bytearray(data)]
        for offset in (0,4,13,45,51,193,194):
            bad=bytearray(data);bad[offset]=255;cases.append(bytes(bad))
        for bad in cases:
            with self.assertRaises(ValueError): decode_command_proposal(bad)

    def test_any_substituted_decision_field_rejects(self):
        data=encode_command_decision(PROMPT,True)
        for offset in range(len(data)):
            if offset==5: continue
            bad=bytearray(data);bad[offset]^=1
            with self.assertRaises(ValueError): match_command_decision(bytes(bad),PROMPT)
        for bad in (data[:-1],data+b"x",data[:5]+b"\x02"+data[6:],bytearray(data)):
            with self.assertRaises(ValueError): match_command_decision(bad,PROMPT)

    def test_invalid_scope_and_noncanonical_commands_reject(self):
        changes=[dict(generation=v) for v in (0,True,2**64)]
        changes += [dict(review_owner=v) for v in ("bad","C"*32)]
        changes += [dict(identity=replace(PROMPT.identity,**values)) for values in
            (dict(epoch=True),dict(peer_id=""),dict(peer_sha256="A"*64),dict(connection_id="bad"),dict(tls_version="TLSv1"))]
        changes += [dict(payload=v) for v in (bytearray(PROMPT.payload),b"1|045|1|review|arm|1",
            b"1|45|01|review|arm|1",b"1|46|1|review|arm|1",b"1|45|1|review|record|1",b"1|45|1|review|arm|128")]
        for values in changes:
            with self.subTest(values=values),self.assertRaises(ValueError): encode_command_proposal(replace(PROMPT,**values))
        for allow in (1,None,"allow"):
            with self.assertRaises(ValueError): encode_command_decision(PROMPT,allow)

    def test_maximum_integer_and_identifier_fields_roundtrip(self):
        identity=replace(PROMPT.identity,epoch=2**64-1,peer_id="p"*64,tls_version="TLSv1.2")
        prompt=replace(PROMPT,generation=2**64-1,identity=identity,
            payload=b"1|18446744073709551615|18446744073709551615|"+b"r"*64+b"|select|127")
        data=encode_command_proposal(prompt)
        self.assertLessEqual(len(data),MAX_PROPOSAL_BYTES)
        self.assertEqual(decode_command_proposal(data),prompt)

    def test_decoding_allow_does_not_resolve_a_live_mailbox_or_write_approval(self):
        ledger=NativeCommandApprovalLedger();identity=PROMPT.identity
        scope=(identity.peer_id,identity.epoch,identity.connection_id);ledger.begin_session(*scope)
        review=NativeCommandReview(ledger);cancel=threading.Event();results=[]
        worker=threading.Thread(target=lambda:results.append(review.request(identity,PROMPT.payload,
            cancel=cancel,deadline=time.monotonic()+1)))
        worker.start()
        try:
            until=time.monotonic()+.8
            while review.pending() is None and time.monotonic()<until: time.sleep(.005)
            prompt=review.pending();self.assertIsNotNone(prompt)
            decoded=decode_command_proposal(encode_command_proposal(prompt))
            self.assertTrue(match_command_decision(encode_command_decision(decoded,True),prompt))
            self.assertEqual(review.pending(),prompt)
            self.assertFalse(ledger.consume(*scope,prompt.payload))
        finally:
            review.close();worker.join(timeout=2)
        self.assertFalse(worker.is_alive());self.assertEqual(results,[False])


@unittest.skipUnless(os.environ.get("IDW_COMMAND_REVIEW_WIRE_FIXTURE"),"C++ codec fixture required")
class ReviewWireInteropTests(unittest.TestCase):
    def native(self,mode,data):
        return subprocess.run([os.environ["IDW_COMMAND_REVIEW_WIRE_FIXTURE"],mode],input=data,
            capture_output=True,timeout=3,creationflags=getattr(subprocess,"CREATE_NO_WINDOW",0))

    def test_cpp_roundtrip_and_decisions_match_python_exactly(self):
        for prompt in (PROMPT,replace(PROMPT,identity=replace(PROMPT.identity,tls_version="TLSv1.2"),
            generation=2**64-1,payload=b"1|45|2|second|select|127")):
            data=encode_command_proposal(prompt)
            result=self.native("--proposal",data)
            self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(result.stdout,data)
            self.assertEqual(decode_command_proposal(result.stdout),prompt)
            for mode,allow in (("--allow",True),("--deny",False)):
                result=self.native(mode,data)
                self.assertEqual(result.returncode,0,result.stderr)
                self.assertEqual(result.stdout,encode_command_decision(prompt,allow))
                self.assertIs(match_command_decision(result.stdout,prompt),allow)
                result=self.native("--match",len(data).to_bytes(2,"big")+data+encode_command_decision(prompt,allow))
                self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(result.stdout,b"1" if allow else b"0")

    def test_cpp_rejects_malformed_and_oversized_records_without_output(self):
        data=encode_command_proposal(PROMPT)
        for bad in (data[:-1],data+b"x",b"x"*(MAX_PROPOSAL_BYTES+1),b"IDWQ\x02"+data[5:]):
            result=self.native("--allow",bad)
            self.assertNotEqual(result.returncode,0);self.assertEqual(result.stdout,b"")
        changed=encode_command_decision(replace(PROMPT,generation=PROMPT.generation+1),True)
        result=self.native("--match",len(data).to_bytes(2,"big")+data+changed)
        self.assertNotEqual(result.returncode,0);self.assertEqual(result.stdout,b"")


if __name__=="__main__": unittest.main()
