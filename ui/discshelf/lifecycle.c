/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "shelf.h"
#include "sdio.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/prctl.h>
shared_t *ds_context;
static atomic_int finished;
static char config_path[1024];
static void *run(void *unused){
    (void)unused;
    char error[256]="";
    int lease=sd_io_begin();
    if(!lease || !shelf_init(config_path,error,sizeof error)){
        ds_status_text("%s",lease?error:"SD card is busy or unavailable");
        pthread_mutex_lock(&ds_shared.mu);ds_shared.fatal=1;ds_shared.busy=0;ds_shared.revision++;pthread_mutex_unlock(&ds_shared.mu);
        if(lease)shelf_close();
    }else shelf_worker(NULL);
    if(lease)sd_io_end();
    /* Last access to context: UI may reclaim it after observing this release. */
    atomic_store_explicit(&finished,1,memory_order_release);
    return NULL;
}
int ds_backend_start(const char *conf){
    if(ds_context || !conf || strlen(conf)>=sizeof config_path)return 0;
    ds_context=calloc(1,sizeof *ds_context);if(!ds_context)return 0;
    if(pthread_mutex_init(&ds_shared.mu,NULL)){free(ds_context);ds_context=NULL;return 0;}
    if(pthread_cond_init(&ds_shared.wake,NULL)){pthread_mutex_destroy(&ds_shared.mu);free(ds_context);ds_context=NULL;return 0;}
    strcpy(config_path,conf);atomic_store(&ds_stopping,0);atomic_store(&ds_abort_job,0);atomic_store(&finished,0);ds_shared.busy=1;
    pthread_attr_t a;pthread_attr_init(&a);int ok=pthread_attr_setstacksize(&a,256*1024)==0;
    if(ok)ok=pthread_attr_setdetachstate(&a,PTHREAD_CREATE_DETACHED)==0;
    pthread_t t;if(ok)ok=pthread_create(&t,&a,run,NULL)==0;pthread_attr_destroy(&a);
    if(!ok){atomic_store(&finished,1);ds_backend_collect();return 0;}return 1;
}
void ds_backend_stop(void){if(!ds_context)return;atomic_store(&ds_stopping,1);atomic_store(&ds_abort_job,1);shelf_wake();}
int ds_backend_active(void){return ds_context && !atomic_load_explicit(&finished,memory_order_acquire);}
int ds_backend_collect(void){
    if(!ds_context)return 1;
    if(!atomic_load_explicit(&finished,memory_order_acquire))return 0;
    pthread_cond_destroy(&ds_shared.wake);pthread_mutex_destroy(&ds_shared.mu);free(ds_context);ds_context=NULL;return 1;
}
int ds_fetch_entry(int argc,char **argv){
    if(argc!=4 || strcmp(argv[1],"--shelf-fetch"))return -1;
    char *end;long p=strtol(argv[2],&end,10);
    if(*end || p<=1 || prctl(PR_SET_PDEATHSIG,SIGKILL) || getppid()!=(pid_t)p)return 1;
    char *av[]={"wget","-q","-O","-","-T","15",argv[3],NULL};execvp("wget",av);return 1;
}
