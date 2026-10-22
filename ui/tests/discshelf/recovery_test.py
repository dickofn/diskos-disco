from pathlib import Path
exec(Path(__file__).with_name('mock_server.py').read_text().split('for run in range(2):')[0])
# Large existing library exercises the album lookup index and startup cancellation hooks.
con = sqlite3.connect(local)
con.executemany('INSERT INTO SONG VALUES(?,?,?,?,?)', ((f'Song {i}', f'Artist {i%1000}', f'Album {i%5000}', '/missing/track.flac', '') for i in range(50000)))
con.commit(); con.close()
original = Handler.do_GET
counts = {}
def broken_first(self):
    ep = self.path.split('/')[-1].split('.')[0]
    counts[ep] = counts.get(ep, 0) + 1
    if ep in ('getArtists', 'getAlbum') and counts[ep] == 1:
        # Deep ignored JSON also tests the bounded parser on the explicit worker stack.
        b = b'{"subsonic-response":{"status":"ok","ignored":' + b'{"n":'*27 + b'0' + b'}'*27 + b',"broken":'
        self.send_response(200); self.send_header('Content-Length', str(len(b))); self.end_headers(); self.wfile.write(b)
    else:
        original(self)
Handler.do_GET = broken_first
p = subprocess.run([os.environ['SHELF_RECOVERY_TEST_BIN'], str(conf)], env=env, capture_output=True, text=True, timeout=90)
print(p.stdout, p.stderr)
assert p.returncode == 0
assert counts['getArtists'] == 2
assert counts['getArtist'] == 1  # Previous page reused after failed replacement.
assert counts['getAlbum'] == 2
server.shutdown()
