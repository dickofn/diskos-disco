#!/usr/bin/env python3
"""Exercise production Bluetooth capture with real subprocesses and bounded timeouts."""
from pathlib import Path
import subprocess
import tempfile
app=Path(__file__).resolve().parents[1]
s=(app/'bt.c').read_text()
def fn(sig):
 start=s.index(sig)
 while s[start:s.index('\n',start)].rstrip().split('/*')[0].rstrip().endswith(';'): start=s.index(sig,start+1)   # skip forward declarations
 opening=s.index('{',start);depth=1;end=opening+1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
h=r'''
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <spawn.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <sys/wait.h>
#include <pthread.h>
#include <time.h>
#define BT_STUCK_MAX 8
static pid_t g_bt_stuck[BT_STUCK_MAX];
static pthread_mutex_t g_bt_stuck_mu=PTHREAD_MUTEX_INITIALIZER;
static __thread int g_last_exit=-1;
static uint32_t lv_tick_get(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint32_t)(t.tv_sec*1000+t.tv_nsec/1000000);}
static uint32_t lv_tick_elaps(uint32_t t){return lv_tick_get()-t;}
static int spawn_error, launches, inject_window;
static pid_t injected;
static int test_spawn(pid_t *p,const char *path,const posix_spawn_file_actions_t *fa,const posix_spawnattr_t *at,char *const argv[],char *const env[]){
 launches++;if(spawn_error)return spawn_error;
 int rc=posix_spawn(p,path,fa,at,argv,env);
 if(inject_window && argv[2] && strstr(argv[2],"printf ready")){
  /* B05: another thread starts a helper while this helper's pipe writer is still open in the UI process */
  inject_window=0; char *a[]={"sh","-c","sleep 1",NULL}; posix_spawn(&injected,"/bin/sh",NULL,NULL,a,env);
 }
 return rc;
}
#define posix_spawn test_spawn
/* A probe must not regress to copying the UI address space. */
#define fork FORBIDDEN_fork
'''
for sig in ('static void bt_reap_stuck(', 'static void bt_reap_bounded(', 'static int run_cap_to(', 'static int run_cap_bounded('):h+='\n'+fn(sig)
h+=r'''
int main(void){
 char out[128];
 { /* B05 (2.5.9 Bluetooth audit): a helper started while another one's pipe is open must not inherit its writer */
   inject_window=1;
   uint32_t t0=lv_tick_get(); int n=run_cap_to("printf ready",out,sizeof out,800); uint32_t took=lv_tick_elaps(t0);
   waitpid(injected,NULL,0);
   assert(n==5 && !strcmp(out,"ready") && took<500);
   printf("PASS: a concurrent helper doesn't hold back another's end of output (%u ms)\n",took);
   /* B09: the exit status is kept, so a failing command is not taken for success */
   assert(run_cap_to("exit 1",out,sizeof out,1000)==0 && g_last_exit==1);
   assert(run_cap_to("true",out,sizeof out,1000)==0 && g_last_exit==0);
   puts("PASS: exit status recorded (failure 1, success 0)");
   launches=0;                       /* the checks below count their own launches */
 }
 for(int i=0;i<10;i++){
  assert(run_cap_bounded("printf 'Powered: yes\\n'",out,sizeof out,250)==13);
  assert(!strcmp(out,"Powered: yes\n"));
 }
 uint32_t t=lv_tick_get();
 assert(run_cap_bounded("sleep 2",out,sizeof out,80)==0);
 assert(lv_tick_elaps(t)<600 && out[0]==0);
 t=lv_tick_get();
 assert(run_cap_bounded("printf 'Powered: yes\\n'; sleep 2",out,sizeof out,80)==13);
 assert(lv_tick_elaps(t)<600 && !strcmp(out,"Powered: yes\n"));
 spawn_error=ENOMEM;assert(run_cap_bounded("printf invalid",out,sizeof out,250)==0 && out[0]==0);
 assert(launches==13);
 assert(waitpid(-1,NULL,WNOHANG)==-1 && errno==ECHILD);
 puts("PASS: readiness output; repeated posix_spawn probes; empty/partial-output deadlines; launch failure; direct children reaped");
}
'''
with tempfile.TemporaryDirectory(prefix='diskos-bt-capture-') as tmp:
 source=Path(tmp)/'test.c';source.write_text(h);binary=Path(tmp)/'test'
 subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-pthread',str(source),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True,timeout=5)
