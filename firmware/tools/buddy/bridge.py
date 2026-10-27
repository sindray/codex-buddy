#!/usr/bin/env python3
"""Local Codex rollout metadata -> USB or optional BLE. Never transmit prompts, answers or tool output."""
from __future__ import annotations
import argparse
import datetime as dt
import fcntl
import json
import os
from pathlib import Path
import re
import signal
import sys
import time
from dataclasses import dataclass, field

STATES = ('boot','connecting','idle','received','thinking','planning','reading','searching',
          'coding','building','testing','reviewing','waiting','approval','done','error',
          'cancelled','reconnecting','sleeping','waving')

def timestamp(value):
    try:
        return dt.datetime.fromisoformat(value.replace('Z','+00:00')).timestamp()
    except (ValueError, TypeError, AttributeError):
        return 0.0

def classify_tool(name, raw):
    name = (name or '').lower()
    raw = raw if isinstance(raw,str) else ''
    if 'request_user_input' in name or 'request_user_input' in raw:
        return 'waiting'
    if any(x in name for x in ('apply_patch','write_file','edit_file')) or 'tools.apply_patch(' in raw:
        return 'coding'
    if any(x in name for x in ('search','web','browse')) or 'tools.web__run(' in raw:
        return 'searching'
    if 'update_plan' in name:
        return 'planning'
    # Classify known operations, without inventing detail for arbitrary tool calls.
    if re.search(r'\b(?:pytest|ctest|unittest|vitest)\b|(?:npm|pnpm) (?:run )?test\b',raw):
        return 'testing'
    if re.search(r'\bidf\.py\b[^\n]{0,80}\b(?:build|reconfigure|merge-bin)\b|(?:npm|pnpm) run build|validate\.sh',raw):
        return 'building'
    if re.search(r'\bgit (?:diff|status)\b',raw):
        return 'reviewing'
    if re.search(r'\b(?:rg|cat|sed|head|tail|ls|find)\b',raw):
        return 'reading'
    return 'coding'

@dataclass
class Session:
    path: Path
    offset: int = 0
    buffer: bytes = b''
    inode: int = 0
    project: str = 'Codex task'
    state: str = 'idle'
    active: bool = False
    started: float = 0
    updated: float = 0
    turn: str = ''
    ignored: bool = False
    pending: dict = field(default_factory=dict)

    def consume(self, record, now):
        if not isinstance(record,dict):
            return
        kind = record.get('type')
        p = record.get('payload') or {}
        if not isinstance(p,dict):
            return
        stamp = timestamp(record.get('timestamp'))
        if kind == 'session_meta':
            source=p.get('source')
            self.ignored=isinstance(source,dict) and 'subagent' in source
            name=Path(p.get('cwd') or '').name
            self.project=name if name and re.fullmatch(r'[\x20-\x7e]{1,32}',name) else 'Codex task'
            return
        if self.ignored:
            return
        event = p.get('type')
        if kind == 'event_msg':
            if event == 'task_started':
                self.active=True; self.started=stamp or now; self.turn=p.get('turn_id','')
                self.state='received';self.updated=stamp or now;self.pending.clear()
            elif event in ('task_complete','turn_aborted'):
                # A late completion from an earlier turn must not end a newer one.
                if p.get('turn_id') and self.turn and p['turn_id']!=self.turn:
                    return
                self.active=False
                self.state='cancelled' if event=='turn_aborted' else 'error' if p.get('error') else 'done'
                self.updated=stamp or now;self.pending.clear()
        elif kind == 'response_item' and self.active:
            if event in ('function_call','custom_tool_call'):
                state=classify_tool(p.get('name'),p.get('arguments') or p.get('input'))
                self.pending[p.get('call_id','')]=state
                self.state=state;self.updated=stamp or now
            elif event in ('function_call_output','custom_tool_call_output'):
                self.pending.pop(p.get('call_id',''),None)
                self.state=next(reversed(self.pending.values())) if self.pending else 'thinking'
                self.updated=stamp or now
            elif event == 'reasoning':
                self.state=next(reversed(self.pending.values())) if self.pending else 'thinking'
                self.updated=stamp or now

    def read(self, now):
        try:
            info=self.path.stat()
            if self.inode!=info.st_ino or info.st_size<self.offset:
                self.offset=0;self.buffer=b'';self.inode=info.st_ino
                self.active=False;self.pending.clear()
            with self.path.open('rb') as stream:
                stream.seek(self.offset)
                # Bounded reads; retain partial JSONL records until the next poll.
                data=stream.read(4*1024*1024)
                self.offset=stream.tell()
            chunks=(self.buffer+data).split(b'\n');self.buffer=chunks.pop()
            if len(self.buffer)>8*1024*1024:
                self.buffer=b''
            for line in chunks:
                try: self.consume(json.loads(line),now)
                except (ValueError,UnicodeError,TypeError): continue
        except (FileNotFoundError,PermissionError,OSError):
            return

class Monitor:
    def __init__(self,root,project=None):
        self.root=Path(root);self.project=project;self.sessions={};self.last_scan=0
        self.started=time.time()

    def poll(self,now):
        if now-self.last_scan>=5:
            # Only recent local rollouts; include yesterday across midnight.
            dates=[dt.datetime.fromtimestamp(now,dt.timezone.utc).date()-dt.timedelta(days=i) for i in range(2)]
            for day in dates:
                folder=self.root/day.strftime('%Y/%m/%d')
                for path in folder.glob('*.jsonl'):
                    if path not in self.sessions:self.sessions[path]=Session(path)
            self.last_scan=now
        for s in self.sessions.values():s.read(now)
        items=[s for s in self.sessions.values() if not s.ignored and (not self.project or s.project==self.project)]
        # Do not replay yesterday's abandoned turn as a freshly started task.
        active=[s for s in items if s.active and now-s.updated<3600]
        priority={'waiting':5,'approval':6,'error':4}
        if active:
            selected=max(active,key=lambda s:(priority.get(s.state,0),s.updated))
            state=selected.state
            if state=='received' and now-selected.started>3:state='thinking'
        else:
            recent=[s for s in items if not s.active and s.updated>=self.started]
            selected=max(recent,key=lambda s:s.updated) if recent else None
            if selected and selected.state!='error' and now-selected.updated>=12:
                selected=None
            state=selected.state if selected else 'idle'
        return {'v':1,'state':state,'active':min(len(active),99),
                'elapsed':min(int(max(0,now-selected.started)),604800) if selected and active else 0,
                'task':selected.project if selected else 'Codex task'}

RUNNING=True

def stop(*_):
    global RUNNING
    RUNNING=False

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--transport',choices=('usb','ble'),default='usb')
    ap.add_argument('--ble-address',help='Optional BLE address or macOS peripheral UUID')
    ap.add_argument('--port',default='/dev/cu.usbmodem3101')
    ap.add_argument('--sessions',type=Path,default=Path.home()/'.codex/sessions')
    ap.add_argument('--project',help='Optional project basename filter')
    ap.add_argument('--dry-run',action='store_true')
    args=ap.parse_args()
    monitor=Monitor(args.sessions,args.project)
    if args.dry_run:
        print(json.dumps(monitor.poll(time.time()),ensure_ascii=True));return
    signal.signal(signal.SIGTERM,stop);signal.signal(signal.SIGINT,stop)
    if args.transport=='ble':
        import asyncio
        from ble_bridge import run
        # One local BLE sender, including discovery mode, avoids competing clients.
        lock=Path('/tmp/codex-buddy-ble.lock').open('a')
        try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:raise SystemExit('A BLE bridge is already running')
        asyncio.run(run(monitor,args.ble_address,lambda:RUNNING))
        return
    import serial
    # One sender per serial device. No service registration or global config changes.
    lock_path=Path('/tmp')/('codex-buddy-'+re.sub(r'[^A-Za-z0-9]','_',args.port)+'.lock')
    lock=lock_path.open('a')
    try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    except BlockingIOError:raise SystemExit('A bridge already owns this port')
    signal.signal(signal.SIGTERM,stop);signal.signal(signal.SIGINT,stop)
    port=None;seq=0;last_send=0;last_ack=0;last_state=None;receive=b'';confirmed=False
    print('Codex Buddy USB bridge started; sending state metadata only.',flush=True)
    while RUNNING:
        now=time.time();payload=monitor.poll(now)
        try:
            if port is None:
                port=serial.Serial(args.port,115200,timeout=0,write_timeout=1)
                last_send=0;last_ack=now
                confirmed=False
                print('USB connected: '+args.port,flush=True)
            data=port.read(min(port.in_waiting,4096))
            receive=(receive+data)[-16384:]
            lines=receive.split(b'\n');receive=lines.pop()
            for line in lines:
                try:
                    ack=json.loads(line)
                    if isinstance(ack,dict) and isinstance(ack.get('buddy_ack'),int):
                        last_ack=now
                        if not confirmed:
                            print('USB device acknowledgement confirmed.',flush=True)
                            confirmed=True
                except (ValueError,UnicodeError):pass
            if payload['state']!=last_state or now-last_send>=2:
                seq=(seq+1)%2147483647;payload['seq']=seq
                port.write((json.dumps(payload,separators=(',',':'))+'\n').encode('ascii'))
                if payload['state']!=last_state:
                    print(f"state={payload['state']} active={payload['active']}",flush=True)
                last_send=now;last_state=payload['state']
            if now-last_ack>20:
                raise serial.SerialException('Device acknowledgement timeout')
        except (serial.SerialException,OSError) as exc:
            print('USB retry: '+str(exc),flush=True)
            if port:
                try:port.close()
                except OSError:pass
            port=None;last_state=None;time.sleep(1)
        time.sleep(.25)
    if port:port.close()
    print('Bridge stopped; USB released.',flush=True)

if __name__=='__main__':main()
