import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

spec=importlib.util.spec_from_file_location('bridge',Path(__file__).resolve().parents[1]/'tools/buddy/bridge.py')
b=importlib.util.module_from_spec(spec)
sys.modules[spec.name]=b
spec.loader.exec_module(b)

def record(kind,event,**kw):
    return {'type':kind,'payload':{'type':event,**kw}}

class BridgeTest(unittest.TestCase):
    def session(self):
        s=b.Session(Path('/unused'))
        s.consume(record('event_msg','task_started',turn_id='new'),100)
        return s

    def test_actual_completion_only(self):
        s=self.session()
        s.consume([],101)
        s.consume(record('response_item','message',role='assistant'),101)
        self.assertTrue(s.active)
        s.consume(record('event_msg','task_complete',turn_id='old'),102)
        self.assertTrue(s.active)
        s.consume(record('event_msg','task_complete',turn_id='new'),103)
        self.assertFalse(s.active)
        self.assertEqual(s.state,'done')

    def test_error_and_abort(self):
        for event,kw,state in [('task_complete',{'error':'failure'},'error'),('turn_aborted',{},'cancelled')]:
            s=self.session();s.consume(record('event_msg',event,**kw),101)
            self.assertEqual(s.state,state)
            self.assertFalse(s.active)

    def test_pending_tool_restoration(self):
        s=self.session()
        s.consume(record('response_item','function_call',name='request_user_input',call_id='a'),101)
        s.consume(record('response_item','function_call',name='web',call_id='b'),102)
        s.consume(record('response_item','function_call_output',call_id='b'),103)
        self.assertEqual(s.state,'waiting')
        s.consume(record('response_item','function_call_output',call_id='a'),104)
        self.assertEqual(s.state,'thinking')

    def test_partial_json_and_rotation(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'rollout.jsonl';s=b.Session(path)
            raw=json.dumps(record('event_msg','task_started',turn_id='a')).encode()
            path.write_bytes(raw[:13]);s.read(100);self.assertFalse(s.active)
            with path.open('ab') as f:f.write(raw[13:]+b'\n')
            s.read(101);self.assertTrue(s.active)
            s.read(102);self.assertEqual(s.started,101)
            path.write_bytes(b'{}\n');s.read(103);self.assertFalse(s.active)

    def test_metadata_and_subagent(self):
        s=b.Session(Path('/unused'))
        s.consume({'type':'session_meta','payload':{'cwd':'/tmp/中文','source':{'subagent':{}}}},100)
        s.consume(record('event_msg','task_started'),101)
        self.assertTrue(s.ignored);self.assertFalse(s.active)
        self.assertEqual(s.project,'Codex task')

    def test_priority_and_no_old_completion(self):
        m=b.Monitor('/unused');m.last_scan=200;m.started=190
        a=self.session();a.state='waiting';a.updated=195
        c=self.session();c.state='coding';c.updated=198
        # No filesystem I/O is required for selection; nonexistent files are tolerated.
        m.sessions={'a':a,'b':c}
        p=m.poll(200);self.assertEqual(p['state'],'waiting');self.assertEqual(p['active'],2)
        a.active=False;c.active=False;a.updated=189;c.updated=188
        self.assertEqual(m.poll(200)['state'],'idle')

    def test_tool_classification(self):
        for raw,state in [('idf.py build','building'),('pytest tests','testing'),('git diff','reviewing'),('rg text','reading'),('tools.apply_patch(x)','coding')]:
            self.assertEqual(b.classify_tool('exec',raw),state)

    def test_error_persists_until_newer_turn(self):
        m=b.Monitor('/unused');m.started=90;m.last_scan=200
        s=self.session();s.active=False;s.state='error';s.updated=100
        m.sessions={'a':s}
        self.assertEqual(m.poll(200)['state'],'error')
        s.state='done';s.updated=180
        self.assertEqual(m.poll(200)['state'],'idle')

if __name__=='__main__':unittest.main()
