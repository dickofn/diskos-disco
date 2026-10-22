#!/usr/bin/env python3
"""Production HTTP transfer: coalescing, tail flush and fault cleanup.
Optional baseline net.c gives a syscall-count comparison, not a device benchmark.
"""
from pathlib import Path
import http.server, os, subprocess, tempfile, threading, time
ui=Path(__file__).resolve().parents[2]
payload=b'fLaC'+bytes(range(256))*40960+b'tail-unaligned'
class Handler(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_GET(self):
  self.send_response(200);self.send_header('Content-Length',str(len(payload)));self.end_headers()
  try:
   for i in range(0,len(payload),4096):
    self.wfile.write(payload[i:i+4096])
    if i==0 and 'slow=1' in self.path:self.wfile.flush();time.sleep(.8)
  except (BrokenPipeError,ConnectionResetError):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
threading.Thread(target=server.serve_forever,daemon=True).start()
with tempfile.TemporaryDirectory(prefix='shelf-buffer-') as temp:
 root=Path(temp);conf=root/'shelf.conf';out=root/'audio.part'
 conf.write_text(f'server=http://127.0.0.1:{server.server_port}\nusername=test\npassword=password\ndownload_dir={root}\n')
 def build(net,name):
  exe=root/name
  subprocess.run(['cc','-O2','-pthread','-I'+str(ui),'-I'+str(ui/'discshelf'),str(ui/'tests/discshelf/transfer_driver.c'),str(net),str(ui/'discshelf/connection.c'),str(ui/'md5.c'),'-Wl,--wrap=write','-o',str(exe)],check=True)
  return exe
 def run(exe,mode):
  start=time.monotonic();r=subprocess.run([str(exe),str(conf),str(root),str(out),mode,str(len(payload))],text=True,capture_output=True,check=True,timeout=20)
  values=dict(x.split('=') for x in r.stdout.split());print(exe.name,mode,r.stdout.strip(),f'{time.monotonic()-start:.3f}s')
  if mode in ('normal','short','interrupt','slow'):assert values['ok']=='1' and out.read_bytes()==payload
  else:assert values['ok']=='0' and not out.exists()
  if mode=='slow':assert int(values['first_ms'])<650,values
  out.unlink(missing_ok=True);return int(values['writes'])
 current=build(ui/'discshelf/net.c','buffered')
 writes=run(current,'normal');assert writes<150
 for mode in ('short','interrupt','fail','cancel','resource','slow'):run(current,mode)
 baseline=os.environ.get('SHELF_BASELINE_NET')
 if baseline:
  old=run(build(Path(baseline),'baseline'),'normal');print(f'SD write calls: {old} -> {writes} ({100*(1-writes/old):.1f}% fewer)');assert writes<old/2
server.shutdown()
print('PASS: exact bytes, final partial buffer, short writes, EINTR, ENOSPC, cancellation and resource failure')
