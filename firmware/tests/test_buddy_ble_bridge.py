import asyncio
import importlib.util
import json
from pathlib import Path
import unittest
spec=importlib.util.spec_from_file_location('ble_bridge',Path(__file__).resolve().parents[1]/'tools/buddy/ble_bridge.py')
ble=importlib.util.module_from_spec(spec)
spec.loader.exec_module(ble)

class BridgeTests(unittest.TestCase):
    def test_chunked_write(self):
        class Client:
            def __init__(self):self.chunks=[]
            async def write_gatt_char(self,uuid,data,response):
                self.assertions=(uuid==ble.RX and response)
                self.chunks.append(data)
        client=Client()
        payload={'v':1,'state':'thinking','seq':9,'task':'Codex task','active':1}
        asyncio.run(ble.write_packet(client,payload))
        self.assertTrue(client.assertions)
        self.assertTrue(all(len(c)<=20 for c in client.chunks))
        self.assertEqual(json.loads(b''.join(client.chunks)),payload)
        self.assertTrue(client.chunks[-1].endswith(b'\n'))

    def test_notification_fragments_and_matching_sequences(self):
        replies=ble.Replies();replies.pending.add(4)
        replies.receive(None,b'{"buddy_ack":')
        self.assertFalse(replies.confirmed)
        replies.receive(None,b'99}\n{"buddy_busy":4,"transport":"usb"}\n')
        self.assertTrue(replies.busy)
        self.assertNotIn(4,replies.pending)
        replies.pending.add(5)
        replies.receive(None,b'bad\n[]\n{"buddy_ack":true}\n{"buddy_ack":5}\n')
        self.assertFalse(replies.busy)
        self.assertNotIn(5,replies.pending)

if __name__=='__main__':unittest.main()
