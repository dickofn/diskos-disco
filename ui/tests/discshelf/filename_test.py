#!/usr/bin/env python3
from pathlib import Path
import os,subprocess,tempfile
ui=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='shelf-names-') as d:
 p=Path(d);exe=p/'test'
 subprocess.run(['cc','-std=gnu11','-g','-pthread','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections','-I'+str(ui),str(ui/'tests/discshelf/filename_driver.c'),str(ui/'sqlite3.o'),'-lm','-ldl','-Wl,--gc-sections','-o',str(exe)],check=True)
 subprocess.run([str(exe),str(p)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
