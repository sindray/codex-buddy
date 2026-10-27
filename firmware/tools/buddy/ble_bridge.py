"""Optional BLE transport. Importing the USB bridge never requires Bleak."""
import asyncio
import json
import time

SERVICE = '6e400001-b5a3-f393-e0a9-e50e24dcca9e'
RX = '6e400002-b5a3-f393-e0a9-e50e24dcca9e'
TX = '6e400003-b5a3-f393-e0a9-e50e24dcca9e'

class Replies:
    def __init__(self):
        self.buffer = b''
        self.pending = set()
        self.last_ack = time.monotonic()
        self.confirmed = False
        self.busy = False

    def receive(self, _sender, data):
        self.buffer = (self.buffer + bytes(data))[-4096:]
        lines = self.buffer.split(b'\n')
        self.buffer = lines.pop()
        for line in lines:
            try:
                item = json.loads(line)
                if not isinstance(item, dict):
                    continue
                seq = item.get('buddy_ack', item.get('buddy_busy'))
                if type(seq) is not int or seq not in self.pending:
                    continue
                self.pending.remove(seq)
                self.last_ack = time.monotonic()
                busy = 'buddy_busy' in item
                if busy != self.busy or not self.confirmed:
                    print('USB has priority; BLE waiting.' if busy else
                          'BLE device acknowledgement confirmed.', flush=True)
                self.busy = busy
                self.confirmed = True
            except (ValueError, UnicodeError):
                continue

async def write_packet(client, payload):
    data = (json.dumps(payload, separators=(',', ':')) + '\n').encode('ascii')
    # Works with the default ATT MTU of 23; no large-MTU assumption.
    for offset in range(0, len(data), 20):
        await client.write_gatt_char(RX, data[offset:offset+20], response=True)

async def run(monitor, address, running):
    try:
        from bleak import BleakClient, BleakScanner
        from bleak.exc import BleakError
    except ImportError as exc:
        raise SystemExit('BLE needs the optional tools/buddy/requirements-ble.txt dependencies') from exc
    print('Codex Buddy BLE bridge started; sending state metadata only.', flush=True)
    seq = 0
    while running():
        try:
            if address:
                device = await BleakScanner.find_device_by_address(address, timeout=8)
            else:
                found = await BleakScanner.discover(timeout=5, return_adv=True,
                                                    service_uuids=[SERVICE])
                devices = [d for d, adv in found.values()
                           if SERVICE in [u.lower() for u in adv.service_uuids]]
                if len(devices) > 1:
                    raise SystemExit('Multiple CodexBuddy devices; specify --ble-address (macOS UUID).')
                device = devices[0] if devices else None
            if not running():
                break
            if device is None:
                raise OSError('CodexBuddy not found; check power and Bluetooth permission')
            async with BleakClient(device, timeout=12) as client:
                replies = Replies()
                await client.start_notify(TX, replies.receive)
                last_state = None
                last_send = 0
                while running() and client.is_connected:
                    now = time.monotonic()
                    payload = monitor.poll(time.time())
                    if payload['state'] != last_state or now-last_send >= 2:
                        seq = (seq+1) % 2147483647
                        payload['seq'] = seq
                        replies.pending.add(seq)
                        replies.pending = {s for s in replies.pending if s >= seq-16}
                        await write_packet(client, payload)
                        last_send = now
                        last_state = payload['state']
                    if now-replies.last_ack > 20:
                        raise OSError('BLE acknowledgement timeout')
                    await asyncio.sleep(.25)
        except (BleakError, OSError, asyncio.TimeoutError) as exc:
            print('BLE retry: '+str(exc), flush=True)
            if running():
                await asyncio.sleep(1)
    print('Bridge stopped; BLE released.', flush=True)
