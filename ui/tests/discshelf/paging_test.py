from pathlib import Path
exec(Path(__file__).with_name('mock_server.py').read_text().split('for run in range(2):')[0])
original=Handler.do_GET
def paging(self):
 q=urllib.parse.parse_qs(urllib.parse.urlsplit(self.path).query);ep=self.path.split('/')[-1].split('.')[0];data={'status':'ok'}
 albums=[dict(id=f'album{i}',name=f'A Album {i:04}',artist='Artist',songCount=40) for i in range(40)]+[dict(id='albumZ',name='Z Album',artist='Z Artist',songCount=40)]
 if ep=='getArtists':data['artists']={'index':[{'name':'A','artist':[dict(id=f'ar{i}',name=f'A Artist {i:05}',albumCount=1) for i in range(10000)]+[dict(id='arZ',name='Z Artist',albumCount=1),dict(id='arN',name='1 Artist',albumCount=1),dict(id='arS',name='★ Artist',albumCount=1)]}]}
 elif ep=='getAlbumList2':off=int(q.get('offset',['0'])[0]);data['albumList2']={'album':albums[off:off+32]}
 elif ep=='getArtist':data['artist']={'album':[dict(id=f'album{i}',name=f'Album {i:04}',artist='Artist') for i in range(70)]}
 elif ep=='getAlbum':data['album']={'song':[dict(id=f't{i}',title=f'Track {i:03}',artist='Artist',album='Album',suffix='flac',size=1000,track=i+1) for i in range(40)]}
 else:return original(self)
 b=json.dumps({'subsonic-response':data}).encode();self.send_response(200);self.send_header('Content-Length',str(len(b)));self.end_headers();self.wfile.write(b)
Handler.do_GET=paging
p=subprocess.run([os.environ.get('SHELF_PAGING_TEST_BIN','/tmp/shelf-paging'),str(conf)],env=env,text=True,capture_output=True,timeout=60);print(p.stdout,p.stderr);assert p.returncode==0
server.shutdown()
