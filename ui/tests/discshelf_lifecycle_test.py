#!/usr/bin/env python3
"""Actual detached backend worker: failed admission/config, cancellation and repeated cleanup."""
from pathlib import Path
import subprocess,tempfile,os
ui=Path(__file__).resolve().parents[1]
c=r'''
#define _GNU_SOURCE
#include "discshelf/shelf.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
static atomic_int leases;
static int allow;
int sd_io_begin(void){if(!allow)return 0;atomic_fetch_add(&leases,1);return 1;}
void sd_io_end(void){atomic_fetch_sub(&leases,1);}
int sd_io_allowed(void){return allow;}
int main(void){
 for(int n=0;n<64;n++){
  allow=n%2;assert(ds_backend_start("/tmp/disco-shelf-no-such-private-config"));
  assert(!ds_backend_start("/tmp/second"));ds_backend_stop();
  int i;for(i=0;i<2000&&!ds_backend_collect();i++)usleep(1000);
  assert(i<2000&&!ds_context&&!ds_backend_active()&&atomic_load(&leases)==0);
 }
 printf("PASS 64 backend start/stop cycles, no overlapping workers, no retained context or SD lease; context=%zu bytes\n",sizeof(shared_t));
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.c').write_text(c);sqlite=ui/'sqlite3.o'
 if not sqlite.is_file():
  sqlite=p/'sqlite.o';subprocess.run(['cc','-O0','-DSQLITE_THREADSAFE=1','-DSQLITE_OMIT_LOAD_EXTENSION','-c',str(ui/'sqlite3.c'),'-o',str(sqlite)],check=True)
 subprocess.run(['cc','-g','-std=gnu11','-pthread','-fsanitize=address,undefined','-I'+str(ui),'-I'+str(ui/'vendor/libsmb2/include'),str(p/'t.c'),*[str(ui/'discshelf'/f) for f in ['engine.c','net.c','json.c','lifecycle.c','smb.c','connection.c']],str(ui/'md5.c'),str(sqlite),str(ui/'vendor/smb-build/libsmb2.a'),'-lm','-ldl','-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
