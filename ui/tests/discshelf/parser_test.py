import subprocess,json,tempfile,pathlib,os
cases=[
 ({'subsonic-response':{'status':'ok','albumList2':{'album':[{'id':'a','name':'Björk é 🎵','comment':'x'*100000}]}}},True),
 ({'subsonic-response':{'status':'failed','error':{'code':40,'message':'Wrong password'}}},False),
 ({'subsonic-response':{'status':'ok','albumList2':{'album':[{'id':'a','name':'x'*600}]}}},False),
]
for obj,ok in cases:
 p=pathlib.Path(tempfile.mktemp());p.write_text(json.dumps(obj));r=subprocess.run([os.environ.get('SHELF_PARSER_TEST_BIN','/tmp/test-parser'),str(p)],capture_output=True);assert (r.returncode==0)==ok,(r.stdout,r.stderr);p.unlink()
for raw in ['{"subsonic-response":{"status":"ok"', '{"status":"ok"}', '{"subsonic-response":{"status":"ok",}}', '{"subsonic-response":{"status":"ok","artist":[{"id":"a","name":"A"},]}}', '{"subsonic-response":{"status":"ok"}}garbage']:
 p=pathlib.Path(tempfile.mktemp());p.write_text(raw);assert subprocess.run([os.environ.get('SHELF_PARSER_TEST_BIN','/tmp/test-parser'),str(p)],capture_output=True).returncode!=0;p.unlink()
print('PASS unicode, large ignored fields, API errors, overlong labels, truncated JSON, malformed root, trailing garbage')
