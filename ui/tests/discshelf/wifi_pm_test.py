#!/usr/bin/env python3
"""Exercise the production PM policy with a fake wl; never touches host Wi-Fi."""
from pathlib import Path
import os,subprocess,tempfile,time
ui=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='shelf-pm-') as temp:
 p=Path(temp);fake=p/'wl';state=p/'state';log=p/'log'
 fake.write_text('''#!/bin/sh
printf '%s\\n' "$*" >> "$PM_TEST_LOG"
if [ "$#" = 3 ]; then
 case "$PM_TEST_CASE" in
 bad) echo unsupported; exit 0;;
 error) exit 1;;
 slow) sleep 5;;
 esac
 cat "$PM_TEST_STATE"
else
 if [ "$PM_TEST_CASE" = setfail ] && [ "$4" = 0 ]; then exit 1; fi
 printf '%s\\n' "$4" > "$PM_TEST_STATE"
 if [ "$PM_TEST_CASE" = setslow ] && [ "$4" = 0 ]; then sleep 5; fi
fi
''');fake.chmod(0o700)
 driver=p/'driver.c';driver.write_text('''#include "shelf.h"
#include <stdio.h>
int main(void){int previous=ds_wifi_copy_begin();printf("%d\\n",previous);ds_wifi_copy_end(previous);return 0;}
''')
 exe=p/'test';subprocess.run(['cc','-O1','-pthread','-ffunction-sections','-fdata-sections','-I'+str(ui),'-I'+str(ui/'discshelf'),'-DDS_WL_PATH="'+str(fake)+'"',str(driver),str(ui/'discshelf/net.c'),'-Wl,--gc-sections','-o',str(exe)],check=True)
 for original,case,token,commands in [(0,'normal',-1,1),(1,'normal',1,3),(2,'normal',2,3),(2,'bad',-1,1),(2,'error',-1,1),(2,'slow',-1,1),(2,'setfail',2,3),(2,'setslow',2,3)]:
  state.write_text(str(original)+'\n');log.write_text('');start=time.monotonic()
  r=subprocess.run([str(exe)],env=dict(os.environ,PM_TEST_STATE=str(state),PM_TEST_LOG=str(log),PM_TEST_CASE=case),text=True,capture_output=True,check=True,timeout=4)
  elapsed=time.monotonic()-start;assert int(r.stdout)==token,(case,r.stdout)
  lines=log.read_text().splitlines();assert len(lines)==commands,(case,lines)
  assert int(state.read_text())==original,(case,state.read_text())
  if token>0:assert lines==['-i wlan0 PM','-i wlan0 PM 0',f'-i wlan0 PM {original}']
  assert elapsed<2.5,(case,elapsed)
  print(f'PASS PM={original} {case}: {commands} commands, restored/unchanged, {elapsed:.2f}s')
 fake.unlink();state.write_text('2\n');r=subprocess.run([str(exe)],capture_output=True,text=True,check=True);assert int(r.stdout)==-1 and state.read_text()=='2\n'
 print('PASS missing utility: no change; no background watcher or persistent settings')
