/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Loopback-only test fixture using upstream's SMB3 server API. Not firmware code. */
#define _GNU_SOURCE
#include <time.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include "smb2/smb2.h"
#include "smb2/libsmb2.h"
#include "smb2/libsmb2-raw.h"
#include <sys/socket.h>
#include "libsmb2-private.h"
void smb2_derive_key(uint8_t*,uint32_t,const char*,uint32_t,const char*,uint32_t,uint8_t*);
static struct {int fd;DIR*dir;char path[2048];} h[64];
static const char*root;static int version=0x300;
static void*listing;static char*names[16];static void*last_info;
static int slot(const uint8_t*id){int n;memcpy(&n,id,4);return n>0&&n<64?n:-1;}
static int auth(struct smb2_server*s,struct smb2_context*c,const char*u,const char*d,const char*w){(void)s;(void)d;(void)w;if(!u||strcmp(u,"root"))return -1;smb2_set_password(c,"DiscShelfTest42!");return 0;}
static int session(struct smb2_server*s,struct smb2_context*c){(void)s;
 /* Upstream client encryption helpers use client-direction keys. The fixture
  * supplies the server-direction keys explicitly; firmware does not use this. */
 if(smb2_get_dialect(c)>=0x300){smb2_derive_key(c->session_key,c->session_key_size,"SMB2AESCCM",11,"ServerIn ",10,c->serverout_key);smb2_derive_key(c->session_key,c->session_key_size,"SMB2AESCCM",11,"ServerOut",10,c->serverin_key);}
 fprintf(stderr,"fixture dialect=%x\n",smb2_get_dialect(c));return 0;}
static int tree(struct smb2_server*s,struct smb2_context*c,struct smb2_tree_connect_request*q,struct smb2_tree_connect_reply*r){(void)s;(void)c;(void)q;r->share_type=SMB2_SHARE_TYPE_DISK;r->maximal_access=0x101f01ff;return 0;}
static int create(struct smb2_server*s,struct smb2_context*c,struct smb2_create_request*q,struct smb2_create_reply*r){
 (void)s;(void)c;int n;for(n=1;n<64&&h[n].fd>=0;n++);if(n==64)return -1;
 char p[1024];snprintf(p,sizeof p,"%s",q->name?q->name:"");for(char*x=p;*x;x++)if(*x=='\\')*x='/';if(*p=='/'||strstr(p,".."))return -1;
 snprintf(h[n].path,sizeof h[n].path,"%s/%s",root,p);h[n].fd=open(h[n].path,O_RDONLY|O_NOFOLLOW);if(h[n].fd<0)return -1;struct stat st;if(fstat(h[n].fd,&st))return -1;if(S_ISDIR(st.st_mode))h[n].dir=opendir(h[n].path);
 memcpy(r->file_id,&n,4);r->file_attributes=S_ISDIR(st.st_mode)?SMB2_FILE_ATTRIBUTE_DIRECTORY:SMB2_FILE_ATTRIBUTE_NORMAL;r->end_of_file=st.st_size;return 0;
}
static int close_file(struct smb2_server*s,struct smb2_context*c,struct smb2_close_request*q,struct smb2_close_reply*r){(void)s;(void)c;(void)r;int n=slot(q->file_id);if(n<0)return -1;if(h[n].dir)closedir(h[n].dir);h[n].dir=NULL;if(h[n].fd>=0)close(h[n].fd);h[n].fd=-1;return 0;}
static int read_file(struct smb2_server*s,struct smb2_context*c,struct smb2_read_request*q,struct smb2_read_reply*r){(void)s;(void)c;int n=slot(q->file_id);if(n<0||q->length>65536)return -1;r->data=malloc(q->length);if(!r->data)return -1;ssize_t got=pread(h[n].fd,r->data,q->length,q->offset);if(got<0){free(r->data);return -1;}r->data_length=got;return 0;}
static int query_dir(struct smb2_server*s,struct smb2_context*c,struct smb2_query_directory_request*q,struct smb2_query_directory_reply*r){
 (void)s;(void)c;int n=slot(q->file_id);if(n<0||!h[n].dir)return -1;
 for(int i=0;i<16;i++){free(names[i]);names[i]=NULL;}free(listing);size_t stride=(sizeof(struct smb2_fileidbothdirectoryinformation)+7)&~7;listing=calloc(16,stride);if(!listing)return -1;
 int k=0;struct dirent*e;while(k<1&&(e=readdir(h[n].dir))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;struct stat st;if(fstatat(h[n].fd,e->d_name,&st,AT_SYMLINK_NOFOLLOW))continue;
 struct smb2_fileidbothdirectoryinformation*f=(void*)((char*)listing+k*stride);names[k]=strdup(e->d_name);f->name=names[k];f->file_id=st.st_ino;f->end_of_file=st.st_size;f->file_attributes=S_ISDIR(st.st_mode)?SMB2_FILE_ATTRIBUTE_DIRECTORY:S_ISLNK(st.st_mode)?SMB2_FILE_ATTRIBUTE_REPARSE_POINT:SMB2_FILE_ATTRIBUTE_NORMAL;f->last_write_time.tv_sec=st.st_mtime;f->last_write_time.tv_usec=st.st_mtim.tv_nsec/1000;k++;}
 r->output_buffer=listing;r->output_buffer_length=k*stride;return 0;
}
static int query_info(struct smb2_server*s,struct smb2_context*c,struct smb2_query_info_request*q,struct smb2_query_info_reply*r){
 (void)s;(void)c;int n=slot(q->file_id);if(n<0)return -1;struct stat st;if(fstat(h[n].fd,&st))return -1;free(last_info);struct smb2_file_all_info*f=last_info=calloc(1,sizeof *f);if(!f)return -1;f->basic.file_attributes=S_ISDIR(st.st_mode)?SMB2_FILE_ATTRIBUTE_DIRECTORY:SMB2_FILE_ATTRIBUTE_NORMAL;f->basic.last_write_time.tv_sec=st.st_mtime;f->basic.last_write_time.tv_usec=st.st_mtim.tv_nsec/1000;f->standard.end_of_file=st.st_size;f->standard.directory=S_ISDIR(st.st_mode);f->index_number=st.st_ino;f->name=(uint8_t*)"";r->output_buffer=f;r->output_buffer_length=sizeof *f;return 0;
}
static struct smb2_server_request_handlers handlers={.authorize_user=auth,.session_established=session,.tree_connect_cmd=tree,.create_cmd=create,.close_cmd=close_file,.read_cmd=read_file,.query_directory_cmd=query_dir,.query_info_cmd=query_info};
static void wire_error(struct smb2_context*c,const char*err){(void)c;fprintf(stderr,"fixture error %s\n",err);}
static void accepted(struct smb2_context*c,void*p){(void)p;fprintf(stderr,"fixture accepted\n");smb2_register_error_callback(c,wire_error);smb2_set_version(c,version);if(version>=0x300){smb2_set_seal(c,1);smb2_set_sign(c,1);}smb2_set_password(c,"DiscShelfTest42!");}
int main(int argc,char**argv){if(argc<3)return 2;root=argv[1];if(argc>3)version=strtol(argv[3],NULL,0);for(int i=0;i<64;i++)h[i].fd=-1;struct smb2_server server={0};server.handlers=&handlers;server.signing_enabled=1;server.allow_anonymous=0;server.port=atoi(argv[2]);int rc=smb2_serve_port(&server,2,accepted,NULL);fprintf(stderr,"fixture exit %d\n",rc);return rc;}
