/* Exercises the production downloader; write faults are injected only here. */
#define _GNU_SOURCE
#include "shelf.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
shared_t *ds_context;
atomic_int ds_stopping, ds_abort_job;
extern char ds_state_dir[1100];
static const char *mode;
static unsigned calls, checks;
extern int ds_config_read(const char *,int,char *,size_t);
static uint64_t started,first_write;
static uint64_t millis(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
int shelf_sd_ready(void){return strcmp(mode,"resource") || ++checks < 4;}
ssize_t __real_write(int,const void*,size_t);
ssize_t __wrap_write(int fd,const void *p,size_t n){
 if(fd>2){
  calls++;if(!first_write)first_write=millis()-started;
  if(!strcmp(mode,"cancel")){atomic_store(&ds_abort_job,1);errno=EINTR;return -1;}
  if(!strcmp(mode,"fail") && calls==2){errno=ENOSPC;return -1;}
  if(!strcmp(mode,"interrupt") && calls==1){errno=EINTR;return -1;}
  if(!strcmp(mode,"short") && n>32749)n=32749;
 }
 return __real_write(fd,p,n);
}
int main(int argc,char **argv){
 if(argc!=6)return 2;mode=argv[4];
 ds_context=calloc(1,sizeof *ds_context);pthread_mutex_init(&ds_shared.mu,NULL);
 char error[256];if(!ds_config_read(argv[1],1,error,sizeof error)){fprintf(stderr,"%s\n",error);return 2;}
 snprintf(ds_state_dir,sizeof ds_state_dir,"%s",argv[2]);
 int64_t size=strtoll(argv[5],NULL,10);
 started=millis();
 int ok=ds_api_fetch("download",!strcmp(mode,"slow")?"id=fixture&slow=1":"id=fixture",argv[3],size,size,1);
 printf("ok=%d writes=%u bytes=%lld first_ms=%llu\n",ok,calls,(long long)ds_shared.byte_done,(unsigned long long)first_write);
 pthread_mutex_destroy(&ds_shared.mu);free(ds_context);return 0;
}
