# Import the reusable mock fixture, without executing its successful download test.
from pathlib import Path
exec(Path(__file__).with_name('mock_server.py').read_text().split('for run in range(2):')[0])
import time
original=Handler.do_GET
mode='truncated'
def failure(self):
 ep=self.path.split('/')[-1].split('.')[0]
 if ep!='download':return original(self)
 self.send_response(200);self.send_header('Content-Length','131072');self.end_headers()
 try:
  if mode=='error':
   data=b'{"subsonic-response":{"status":"failed"}}';self.wfile.write(data+b' '*(131072-len(data)))
  elif mode=='cancel':
   self.wfile.write(b'fLaC'+b'\0'*4092);self.wfile.flush()
   for n in range(31):time.sleep(.1);self.wfile.write(b'\0'*4096);self.wfile.flush()
  else:self.wfile.write(b'fLaC'+b'\0'*124)
 except (BrokenPipeError,ConnectionResetError):pass
 self.close_connection=True
Handler.do_GET=failure
for mode in ['truncated','error','cancel']:
 p=subprocess.run([os.environ.get('SHELF_ENGINE_TEST_BIN','/tmp/shelf-test'),str(conf),mode],env=env,text=True,capture_output=True,timeout=30)
 print(mode,p.stdout.split('DOWNLOAD')[-1].strip());assert p.returncode==0,p.stderr
 assert not list((root/'downloads').rglob('*.flac'))
 assert not list((root/'downloads').rglob('*.part'))
print('PASS short transfer, API-error-as-audio, cancellation: no published audio or leftover parts')
server.shutdown()
