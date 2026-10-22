/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "discshelf/connection.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
int main(int argc,char**argv){assert(argc==2);char path[1600],err[200];snprintf(path,sizeof path,"%s/shelf.conf",argv[1]);ds_settings_t c;
 assert(!ds_settings_load(path,&c)&&!c.profile[0].remember&&!c.profile[1].remember);c.source=DS_SMB;ds_profile_t*p=&c.profile[1];strcpy(p->server,"127.0.0.1");strcpy(p->share,"Music");strcpy(p->user,"test");strcpy(p->password,"private-test-secret");assert(ds_settings_save(path,&c,err,sizeof err));
 struct stat st;assert(!stat(path,&st)&&(st.st_mode&0777)==0600);assert(!stat(argv[1],&st)&&(st.st_mode&0777)==0700);
 FILE*f=fopen(path,"r");char data[8192];size_t n=fread(data,1,sizeof data-1,f);data[n]=0;fclose(f);assert(!strstr(data,"private-test-secret")&&!strstr(data,"password=private"));
 ds_settings_t loaded;assert(ds_settings_load(path,&loaded)&&loaded.source==DS_SMB&&!strcmp(loaded.profile[1].password,"private-test-secret"));ds_session_clear();assert(ds_settings_load(path,&loaded)&&!loaded.profile[1].password[0]);
 p->remember=1;assert(ds_settings_save(path,&c,err,sizeof err));assert(ds_settings_load(path,&loaded)&&!strcmp(loaded.profile[1].password,p->password));
 strcpy(p->share,"../Bad");assert(!ds_settings_save(path,&c,err,sizeof err));assert(ds_settings_load(path,&loaded)&&!strcmp(loaded.profile[1].share,"Music"));
 f=fopen(path,"w");fputs("server=http://127.0.0.1:4533\nusername=old\npassword=legacy\n",f);fclose(f);assert(ds_settings_load(path,&loaded)&&loaded.source==DS_NAVIDROME&&loaded.profile[0].remember&&!strcmp(loaded.profile[0].password,"legacy"));
 ds_secret_clear(&loaded,sizeof loaded);for(size_t i=0;i<sizeof loaded;i++)assert(!((unsigned char*)&loaded)[i]);puts("PASS source choice, legacy config, private file permissions, session-only secrets, clear-on-close and invalid-save preservation");return 0;}
