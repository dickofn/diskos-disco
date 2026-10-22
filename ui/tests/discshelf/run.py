#!/usr/bin/env python3
"""Real SQLite/HTTP regression tests, reused from standalone Disc Shelf."""
from pathlib import Path
import os,subprocess,tempfile
here=Path(__file__).resolve().parent;ui=here.parents[1]
with tempfile.TemporaryDirectory() as d:
 p=Path(d);env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
 sqlite=ui/'sqlite3.o'
 if not sqlite.is_file():
  sqlite=p/'sqlite.o';subprocess.run(['cc','-O0','-DSQLITE_THREADSAFE=1','-DSQLITE_OMIT_LOAD_EXTENSION','-c',str(ui/'sqlite3.c'),'-o',str(sqlite)],check=True)
 for name in ['engine','paging','recovery','parser']:
  out=p/name;files=[here/(name+'_driver.c'),ui/'discshelf/json.c']
  if name!='parser':files += [here/'context.c',ui/'discshelf/engine.c',ui/'discshelf/net.c',sqlite]
  subprocess.run(['cc','-g','-pthread','-fsanitize=address,undefined','-I'+str(ui),'-I'+str(ui/'discshelf'),*[str(f) for f in files],'-lm','-ldl','-o',str(out)],check=True)
  env['SHELF_'+name.upper()+'_TEST_BIN']=str(out)
 for name in ['parser_test','mock_server','failure_test','paging_test','recovery_test']:
  subprocess.run(['python3',str(here/(name+'.py'))],env=env,check=True,timeout=180)
