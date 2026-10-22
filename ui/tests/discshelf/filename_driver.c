/* Production naming/ownership logic with byte-sized fixture downloads. */
#define _GNU_SOURCE
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "discshelf/engine.c"
shared_t *ds_context;
char ds_password[256];
char ds_server[1024]="http://server",ds_download_root[1024],ds_state_dir[1100];
void ds_secret_clear(void*p,size_t n){memset(p,0,n);}
int sd_io_allowed(void){return 1;}
void ds_status_text(const char *fmt,...){(void)fmt;}
int ds_api_fetch(const char *e,const char *a,const char *p,int64_t max,int64_t expected,int media){FILE*f=fopen(p,"w");assert(f);fputs("fLaC0001",f);fclose(f);return 1;}
void ds_url_encode(const char*s,char*o,size_t n){snprintf(o,n,"%s",s);}
static void touch(const char*p,const char*s){FILE*f=fopen(p,"w");assert(f);fputs(s,f);fclose(f);}
static void check(const char*p,const char*s){char b[40]={0};FILE*f=fopen(p,"r");assert(f);fread(b,1,sizeof b-1,f);fclose(f);assert(!strcmp(b,s));}
int main(int argc,char**argv){
 ds_context=calloc(1,sizeof *ds_context);assert(ds_context);testmode=1;
 snprintf(ds_download_root,sizeof ds_download_root,"%s",argv[1]);assert(sqlite3_open(":memory:",&db)==SQLITE_OK);
 assert(execsql("CREATE TABLE downloads(server TEXT,id TEXT,path TEXT,size INTEGER,title TEXT,PRIMARY KEY(server,id));CREATE TABLE local(title TEXT,artist TEXT,album TEXT,path TEXT)"));
 item_t r={.kind=TRACKS,.size=8,.disc=1,.track=3,.year=2024};strcpy(r.id,"one");strcpy(r.artist,"Björk");strcpy(r.album,"An Album");strcpy(r.name,"A Song");strcpy(r.suffix,"flac");
 int saved=0,skipped=0;char dir[1300],p[1600];track_directory(&r,dir,sizeof dir);
 char expected[1300];snprintf(expected,sizeof expected,"%s/Björk/2024 - An Album",argv[1]);assert(!strcmp(dir,expected));
 assert(download_track(r,&saved,&skipped)&&saved==1);snprintf(p,sizeof p,"%s/03. A Song.flac",dir);check(p,"fLaC0001");
 /* Same metadata, distinct ID/disc: do not collapse into the first copy. */
 strcpy(r.id,"two");r.disc=2;assert(download_track(r,&saved,&skipped)&&saved==2);snprintf(p,sizeof p,"%s/03. A Song (Disc 2).flac",dir);check(p,"fLaC0001");
 /* Unowned same-sized files, and even unowned .part files, must be kept. */
 strcpy(r.id,"three");strcpy(r.name,"Occupied");r.track=4;snprintf(p,sizeof p,"%s/04. Occupied.flac",dir);touch(p,"KEEP ME!");
 assert(download_track(r,&saved,&skipped)&&saved==3);check(p,"KEEP ME!");snprintf(p,sizeof p,"%s/04. Occupied (Disc 2).flac",dir);check(p,"fLaC0001");
 strcpy(r.id,"four");strcpy(r.name,"Partial");r.disc=1;r.track=5;snprintf(p,sizeof p,"%s/05. Partial.flac.part",dir);touch(p,"MY PART");assert(download_track(r,&saved,&skipped));check(p,"MY PART");snprintf(p,sizeof p,"%s/05. Partial (2).flac",dir);check(p,"fLaC0001");
 /* Legacy manifest paths remain valid and are not renamed on redownload. */
 strcpy(r.id,"legacy");strcpy(r.name,"Legacy");char old[1600];snprintf(old,sizeof old,"%s/01-006 Legacy-0123456789abcdef.flac",dir);touch(old,"OLDOLD!!");assert(save_manifest(&r,old));ds_shared.replace=1;assert(download_track(r,&saved,&skipped));check(old,"fLaC0001");
 r.year=0;r.track=120;strcpy(r.id,"unknown");strcpy(r.album,"No Year");strcpy(r.name,"Slash / title");assert(download_track(r,&saved,&skipped));snprintf(p,sizeof p,"%s/Björk/0000 - No Year/120. Slash _ title.flac",argv[1]);check(p,"fLaC0001");
 shelf_close();free(ds_context);puts("PASS clean paths, Unicode, year fallback, 3-digit tracks, multidisc/collisions, unowned protection and legacy owned replacement");
}
