import http.server,json,urllib.parse,sqlite3,os,tempfile,subprocess,threading,pathlib
root=pathlib.Path(tempfile.mkdtemp(prefix='shelf-test-'))
local=root/'library.db';con=sqlite3.connect(local);con.execute('CREATE TABLE SONG(TITLE,ARTIST,ALBUM,PATH,ALBUM_ARTIST)');existing=root/'existing.flac';existing.write_bytes(b'fLaC'+b'\0'*124);con.execute('INSERT INTO SONG VALUES(?,?,?,?,?)',('Existing','Guest Artist','Album One',str(existing),'Artist'));con.commit();con.close()
requests=[]
def album(n):return dict(id='album'+str(n),name='Album '+('One' if n==1 else 'Two'),artist='Artist',songCount=2,coverArt='cover'+str(n))
def track(n,j):return dict(id=f't{n}{j}',title='Existing' if n==1 and j==0 else f'Track {n}-{j}',artist='Artist',album=album(n)['name'],suffix='flac',size=131072,track=j+1,discNumber=1)
class Handler(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_GET(self):
  q=urllib.parse.parse_qs(urllib.parse.urlsplit(self.path).query);ep=self.path.split('/')[-1].split('.')[0];requests.append((ep,q.get('id',[''])[0]));data={'status':'ok'}
  if ep=='getArtists':data['artists']={'index':[{'name':'A','artist':[dict(id='artist1',name='Artist',albumCount=2)]}]}
  elif ep=='getAlbumList2':data['albumList2']={'album':[album(1),album(2)] if int(q.get('offset',['0'])[0])==0 else []}
  elif ep=='getArtist':data['artist']=dict(id='artist1',name='Artist',album=[album(1),album(2)])
  elif ep=='getAlbum':n=int(q['id'][0][-1]);data['album']=dict(album(n),song=[track(n,j) for j in range(2)])
  elif ep=='download':
   payload=b'fLaC'+b'\0'*(131072-4);self.send_response(200);self.send_header('Transfer-Encoding','chunked');self.end_headers()
   for i in range(0,len(payload),4096):b=payload[i:i+4096];self.wfile.write(f'{len(b):x}\r\n'.encode()+b+b'\r\n')
   self.wfile.write(b'0\r\n\r\n');return
  else:data={'status':'failed','error':{'code':70,'message':'No'}}
  b=json.dumps({'subsonic-response':data}).encode();self.send_response(200);self.send_header('Content-Length',str(len(b)));self.end_headers();self.wfile.write(b)
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);threading.Thread(target=server.serve_forever,daemon=True).start()
conf=root/'config';conf.write_text(f'server=http://127.0.0.1:{server.server_port}\nusername=test\npassword=password\ndownload_dir={root}/downloads\n')
env=dict(os.environ,SHELF_TEST='1',SHELF_DB=str(local),ASAN_OPTIONS='detect_leaks=0')
for run in range(2):
 p=subprocess.run([os.environ.get('SHELF_ENGINE_TEST_BIN','/tmp/shelf-test'),str(conf)],env=env,text=True,capture_output=True,timeout=90);print(p.stdout,p.stderr);assert p.returncode==0
 print('download calls:',[r for r in requests if r[0]=='download'])
files=list((root/'downloads').rglob('*.flac'));print('files',len(files));assert len(files)==3;assert len([r for r in requests if r[0]=='download'])==3
assert not list((root/'downloads').rglob('*.part'))
print('PASS existing-library duplicate, artist expansion, chunked transfer, repeat launch, manifest duplicate, cleanup')
server.shutdown()
