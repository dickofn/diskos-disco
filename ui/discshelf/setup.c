/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "setup.h"
#include "app_theme.h"
#include "screens.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#define CONFIG_DIR "/usr/data/apps/disc-shelf"
#define CONFIG_PATH CONFIG_DIR "/shelf.conf"
static struct {lv_obj_t *root,*value[4];char field[4][1024];int editing;void (*saved)(void);} *form;
static const char *keys[]={"server","username","password","download_dir"};
static const char *titles[]={"Server URL","Username","Password","Download folder"};
int ds_setup_close(void){if(!form)return 0;lv_obj_delete(form->root);memset(form->field,0,sizeof form->field);free(form);form=NULL;return 1;}
static void dismiss(lv_event_t*e){(void)e;ds_setup_close();}
static void update(void){for(int i=0;i<4;i++){char b[1150];snprintf(b,sizeof b,"%s: %s",titles[i],i==2?(form->field[i][0]?"********":"Not set"):form->field[i][0]?form->field[i]:"Not set");lv_label_set_text(form->value[i],b);}}
static void edited(const char*s){if(!form||!s)return;int i=form->editing;size_t max=i==1||i==2?255:i==3?900:1023;if(strlen(s)>max||strpbrk(s,"\r\n")){ui_toast("Value too long or contains a new line");return;}strcpy(form->field[i],s);update();}
static void edit(lv_event_t*e){if(!form)return;int i=(int)(intptr_t)lv_event_get_user_data(e);form->editing=i;if(i==2)kbinput_open_password(titles[i],form->field[i],edited);else kbinput_open(titles[i],form->field[i],edited);}
static void save(lv_event_t*e){
 (void)e;if(!form)return;
 if((strncmp(form->field[0],"http://",7)&&strncmp(form->field[0],"https://",8))||strpbrk(form->field[0]," \t?#")||!form->field[1][0]||!form->field[2][0]||strncmp(form->field[3],"/tmp/sdcard/",12)||strstr(form->field[3],"..")){ui_toast("Set server, login and an SD-card folder");return;}
 if(mkdir("/usr/data/apps",0755)&&errno!=EEXIST){ui_toast("Cannot create configuration folder");return;}
 if(mkdir(CONFIG_DIR,0700)&&errno!=EEXIST){ui_toast("Cannot create configuration folder");return;}
 struct stat st;if(lstat(CONFIG_DIR,&st)||!S_ISDIR(st.st_mode)){ui_toast("Unsafe configuration folder");return;}
 char tmp[]=CONFIG_DIR "/.shelf.conf-XXXXXX";int fd=mkstemp(tmp);if(fd<0){ui_toast("Cannot save configuration");return;}
 FILE*f=fdopen(fd,"w");int ok=f!=NULL;
 if(f){for(int i=0;i<4;i++)if(fprintf(f,"%s=%s\n",keys[i],form->field[i])<0)ok=0;if(fflush(f)||fsync(fd))ok=0;if(fclose(f))ok=0;}else close(fd);
 if(!ok||rename(tmp,CONFIG_PATH)){unlink(tmp);ui_toast("Could not save; previous settings kept");return;}
 int d=open(CONFIG_DIR,O_RDONLY|O_DIRECTORY|O_CLOEXEC);int durable=d>=0&&fsync(d)==0;if(d>=0)close(d);
 void(*done)(void)=form->saved;ds_setup_close();ui_toast(durable?"Server settings saved":"Saved; directory sync failed");if(done)done();
}
void ds_setup_open(void(*saved)(void)){
 if(form)return;form=calloc(1,sizeof *form);if(!form){ui_toast("Not enough memory");return;}form->saved=saved;
 strcpy(form->field[3],"/tmp/sdcard/Navidrome");FILE*f=fopen(CONFIG_PATH,"r");char line[2048];
 if(f){while(fgets(line,sizeof line,f)){line[strcspn(line,"\r\n")]=0;char*eq=strchr(line,'=');if(!eq)continue;*eq++=0;for(int i=0;i<4;i++)if(!strcmp(line,keys[i])&&strlen(eq)<sizeof form->field[i])strcpy(form->field[i],eq);}fclose(f);}
 form->root=lv_obj_create(screen_get_root(SCR_SHELF));lv_obj_remove_style_all(form->root);lv_obj_set_size(form->root,360,360);lv_obj_set_style_bg_color(form->root,TC(CANVAS),0);lv_obj_set_style_bg_opa(form->root,LV_OPA_COVER,0);lv_obj_remove_flag(form->root,LV_OBJ_FLAG_SCROLLABLE);kit_keep(form->root);
 ui_header_cb(form->root,"Navidrome",dismiss);
 for(int i=0;i<4;i++){
  lv_obj_t*b=lv_button_create(form->root);lv_obj_set_pos(b,48,72+i*45);lv_obj_set_size(b,264,40);lv_obj_add_event_cb(b,edit,LV_EVENT_SHORT_CLICKED,(void*)(intptr_t)i);
  form->value[i]=lv_label_create(b);lv_obj_set_width(form->value[i],240);lv_obj_set_height(form->value[i],24);lv_label_set_long_mode(form->value[i],LV_LABEL_LONG_DOT);lv_obj_center(form->value[i]);app_theme_button(b,0);lv_obj_set_style_text_font(form->value[i],TF(UI_16),0);
 }
 lv_obj_t*b=lv_button_create(form->root);lv_obj_set_pos(b,114,272);lv_obj_set_size(b,132,44);lv_obj_t*l=lv_label_create(b);lv_label_set_text(l,"Save");lv_obj_center(l);app_theme_button(b,0);lv_obj_add_event_cb(b,save,LV_EVENT_SHORT_CLICKED,NULL);update();
}
