#!/usr/bin/env python3
"""Linux-only libsmb2 static client build using the firmware's selected compiler."""
import argparse, concurrent.futures, json, os, re, shlex, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--cc',default='gcc');p.add_argument('--flags',default='');p.add_argument('--out',required=True,type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[1]/'vendor/libsmb2';out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
cc=shlex.split(a.cc);flags=shlex.split(a.flags)+['-Os','-ffunction-sections','-fdata-sections','-D_FILE_OFFSET_BITS=64','-DHAVE_CONFIG_H','-D_GNU_SOURCE','-D_U_=__attribute__((unused))']
identity=json.dumps([cc,flags,subprocess.check_output(cc+['--version'],text=True).splitlines()[0]])
config=out/'config.h';key=out/'compiler.json'
if not key.exists() or key.read_text()!=identity:
 defines=['#define STDC_HEADERS 1','#define HAVE_SOCKADDR_STORAGE 1','#define HAVE_LINGER 1','#define LIBSMB2_SHARE_ENUM_V2 1']
 headers=['arpa/inet.h','dlfcn.h','fcntl.h','inttypes.h','netdb.h','netinet/in.h','netinet/tcp.h','poll.h','stdint.h','stdio.h','stdlib.h','strings.h','string.h','sys/ioctl.h','sys/poll.h','sys/socket.h','sys/stat.h','sys/types.h','sys/uio.h','sys/time.h','unistd.h','sys/unistd.h','errno.h','sys/errno.h','time.h']
 for h in headers:
  r=subprocess.run(cc+['-x','c','-c','-o',os.devnull,'-'],input='#include <'+h+'>\n',text=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  if r.returncode==0:defines.append('#define HAVE_'+re.sub('[^A-Z0-9]','_',h.upper())+' 1')
 config.write_text('\n'.join(defines)+'\n');key.write_text(identity)
cm=(root/'lib/CMakeLists.txt').read_text();sources=re.search(r'else\(\)\s*set\(SOURCES (.*?)\)\s*endif',cm,re.S).group(1)
names=re.findall(r'(?<![\w-])([\w-]+\.c)',sources)
incs=['-I'+str(out),'-I'+str(root/'include'),'-I'+str(root/'include/smb2'),'-I'+str(root/'lib')]
headers=list((root/'include').rglob('*.h'))+list((root/'lib').glob('*.h'))+[config]
newest=max(x.stat().st_mtime for x in headers)
def compile_one(name):
 src=root/'lib'/name;obj=out/(src.stem+'.o')
 if not obj.exists() or obj.stat().st_size==0 or obj.stat().st_mtime<max(src.stat().st_mtime,newest):subprocess.run(cc+flags+incs+['-c',str(src),'-o',str(obj)],check=True)
 return obj
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:objects=list(pool.map(compile_one,names))
lib=out/'libsmb2.a'
if not lib.exists() or any(o.stat().st_mtime>lib.stat().st_mtime for o in objects):
 ar=subprocess.check_output(cc+['-print-prog-name=ar'],text=True).strip();subprocess.run([ar,'rcs',str(lib),*[str(x) for x in objects]],check=True)
print(lib)
