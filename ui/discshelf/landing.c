/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "app_theme.h"
#include <stdlib.h>
#include <string.h>
#include "shelf.h"
#include "landing.h"
#include "null_theme.h"
#include "connection.h"
void disco_vec_icon(lv_obj_t*,const char*,int);
lv_obj_t *shelf_brand_icon(lv_obj_t *p,int x,int y,int size){
 lv_obj_t *l=lv_label_create(p);lv_obj_set_style_text_color(l,TC(ACCENT_PRIMARY),0);kit_keep(l);
 disco_vec_icon(l,"@shelf",size);lv_obj_set_pos(l,x,y);return l;
}
static lv_obj_t *status_label,*browse_button,*home_exit,*source_label;
static lv_obj_t *txt(lv_obj_t *p,const char *s,int x,int y,int w,const lv_font_t *f){
 lv_obj_t *l=lv_label_create(p);lv_obj_set_pos(l,x,y);lv_obj_set_width(l,w);lv_label_set_text(l,s);lv_obj_set_style_text_font(l,f,0);lv_obj_set_style_text_color(l,TC(TEXT_PRIMARY),0);lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);return l;
}
static lv_obj_t *box(lv_obj_t*p,int x,int y,int w,int h,lv_color_t c,int opa,int r){
 lv_obj_t *o=lv_obj_create(p);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_bg_color(o,c,0);lv_obj_set_style_bg_opa(o,opa,0);lv_obj_set_style_radius(o,r,0);lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);return o;
}
static lv_obj_t *pill(lv_obj_t*p,const char*s,int x,int y,int w,int h,const lv_font_t*f){
 lv_obj_t *b=lv_button_create(p);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,h);
 lv_obj_t *l=lv_label_create(b);lv_label_set_text(l,s);app_theme_button(b,0);
 lv_obj_set_style_radius(b,th_null()?0:LV_RADIUS_CIRCLE,0);lv_obj_set_style_pad_all(b,12,0);
 /* Measure the final theme face against the real button, including its padding. */
 for(int i=0;i<8;i++){lv_point_t sz;lv_text_get_size(&sz,s,f,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
  if(sz.x<=w-28)break;const lv_font_t *next=theme_font_smaller(f);if(!next||next==f)break;f=next;}
 lv_obj_set_style_text_font(l,f,0);lv_obj_set_width(l,w-28);lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);
 lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(l);
 lv_obj_set_style_border_width(b,1,LV_STATE_PRESSED);lv_obj_set_style_border_color(b,TC(ACCENT_PRIMARY),LV_STATE_PRESSED);return b;
}
static void shelf(lv_obj_t*p){
 /* Static primitives only: a cropped tower with canvas gradients fading its edges. */
 lv_color_t ghost=TC(TEXT_SECONDARY);
 lv_obj_t *frame=box(p,252,-12,128,350,TC(SURFACE),12,16);
 lv_obj_set_style_border_width(frame,2,0);lv_obj_set_style_border_color(frame,ghost,0);lv_obj_set_style_border_opa(frame,85,0);
 for(int i=0;i<10;i++){
  int opa=65-abs(i-4)*6;
  lv_obj_t *spine=box(p,233+(i%3)*4,8+i*32,151,18,ghost,18,5);
  lv_obj_set_style_border_width(spine,1,0);lv_obj_set_style_border_color(spine,ghost,0);lv_obj_set_style_border_opa(spine,opa,0);
  box(spine,12,5,75+(i%3)*12,2,ghost,opa/2,1);
 }
 lv_obj_t *cd=box(p,192,10,110,110,TC(SURFACE),0,LV_RADIUS_CIRCLE);
 lv_obj_set_style_border_width(cd,2,0);lv_obj_set_style_border_color(cd,ghost,0);lv_obj_set_style_border_opa(cd,80,0);
 lv_obj_t *groove=box(cd,10,10,90,90,TC(SURFACE),0,LV_RADIUS_CIRCLE);
 lv_obj_set_style_border_width(groove,1,0);lv_obj_set_style_border_color(groove,ghost,0);lv_obj_set_style_border_opa(groove,45,0);
 box(cd,45,45,20,20,ghost,50,LV_RADIUS_CIRCLE);
 lv_obj_t *fade=box(p,172,0,188,360,TC(CANVAS),255,0);
 lv_obj_set_style_bg_grad_color(fade,TC(CANVAS),0);lv_obj_set_style_bg_grad_dir(fade,LV_GRAD_DIR_HOR,0);
 lv_obj_set_style_bg_main_opa(fade,220,0);lv_obj_set_style_bg_grad_opa(fade,10,0);
 fade=box(p,172,205,188,155,TC(CANVAS),255,0);
 lv_obj_set_style_bg_grad_color(fade,TC(CANVAS),0);lv_obj_set_style_bg_grad_dir(fade,LV_GRAD_DIR_VER,0);
 lv_obj_set_style_bg_main_opa(fade,0,0);lv_obj_set_style_bg_grad_opa(fade,255,0);
 fade=box(p,172,0,188,55,TC(CANVAS),255,0);
 lv_obj_set_style_bg_grad_color(fade,TC(CANVAS),0);lv_obj_set_style_bg_grad_dir(fade,LV_GRAD_DIR_VER,0);
 lv_obj_set_style_bg_main_opa(fade,160,0);lv_obj_set_style_bg_grad_opa(fade,0,0);
}
lv_obj_t *shelf_home_create(lv_obj_t *parent,lv_event_cb_t browse,lv_event_cb_t exit,lv_event_cb_t settings){
 lv_obj_t *group=box(parent,0,0,360,360,TC(CANVAS),0,0);shelf(group);
 shelf_brand_icon(group,158,32,44);
 lv_obj_t *l=txt(group,"Disc Shelf",28,88,304,TF(UI_40));lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);
 source_label=l=txt(group,"Navidrome browser",40,139,280,TF(UI_20));lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_text_color(l,TC(TEXT_SECONDARY),0);
 browse_button=pill(group,"Browse collection",57,181,246,54,TF(UI_24));
 lv_obj_set_style_border_width(browse_button,2,0);lv_obj_set_style_border_color(browse_button,TC(ACCENT_PRIMARY),0);lv_obj_set_style_border_opa(browse_button,150,0);
 lv_obj_add_event_cb(browse_button,browse,LV_EVENT_SHORT_CLICKED,(void *)(intptr_t)ARTISTS);
 status_label=txt(group,"Preparing collection...",54,244,252,TF(UI_16));lv_obj_set_height(status_label,22);lv_obj_set_style_text_align(status_label,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_text_color(status_label,TC(TEXT_SECONDARY),0);
 home_exit=pill(group,"Exit",66,276,108,42,TF(UI_20));lv_obj_add_event_cb(home_exit,exit,LV_EVENT_SHORT_CLICKED,NULL);
 lv_obj_t *server=pill(group,"Server",186,276,108,42,TF(UI_20));lv_obj_add_event_cb(server,settings,LV_EVENT_SHORT_CLICKED,NULL);
 return group;
}
lv_obj_t *shelf_home_exit(void){return home_exit;}
void shelf_home_status(const char *text,int busy){
 int smb=atomic_load(&ds_source)==DS_SMB;const char*caption=smb?"SMB3 file browser":"Navidrome browser";
 if(strcmp(lv_label_get_text(source_label),caption))lv_label_set_text(source_label,caption);
 lv_obj_t*label=lv_obj_get_child(browse_button,0);const char*action=smb?"Browse files":"Browse collection";if(strcmp(lv_label_get_text(label),action))lv_label_set_text(label,action);
 if(strcmp(lv_label_get_text(status_label),text))lv_label_set_text(status_label,text);
 if(busy)lv_obj_add_state(browse_button,LV_STATE_DISABLED);else lv_obj_remove_state(browse_button,LV_STATE_DISABLED);
}

/* Menu preview only: no configuration read, backend, SD lease or network work. */
lv_obj_t *shelf_preview_create(lv_obj_t *parent,lv_event_cb_t launch){
 lv_obj_t *group=box(parent,0,0,360,360,TC(CANVAS),255,0);kit_keep(group);shelf(group);
 shelf_brand_icon(group,144,40,72);
 lv_obj_t *l=txt(group,"Disc Shelf",40,128,280,TF(UI_32));lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);
 l=txt(group,"Your music, to go.",44,180,272,TF(UI_18));lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_text_color(l,TC(ACCENT_PRIMARY),0);
 l=txt(group,"Navidrome or SMB3.",44,214,272,TF(UI_16));lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_text_color(l,TC(TEXT_SECONDARY),0);
 lv_obj_t *b=pill(group,"Open Disc Shelf",70,260,220,52,TF(UI_22));
 lv_obj_set_style_border_width(b,2,0);lv_obj_add_event_cb(b,launch,LV_EVENT_CLICKED,NULL);
 return group;
}
