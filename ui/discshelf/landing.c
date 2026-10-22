/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "app_theme.h"
#include <stdlib.h>
#include <string.h>
#include "shelf.h"
#include "landing.h"
static lv_obj_t *status_label,*browse_button,*home_exit;
static lv_obj_t *txt(lv_obj_t *p,const char *s,int x,int y,int w,const lv_font_t *f){
 lv_obj_t *l=lv_label_create(p);lv_obj_set_pos(l,x,y);lv_obj_set_width(l,w);lv_label_set_text(l,s);lv_obj_set_style_text_font(l,f,0);lv_obj_set_style_text_color(l,TC(TEXT_PRIMARY),0);lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);return l;
}
static lv_obj_t *box(lv_obj_t*p,int x,int y,int w,int h,lv_color_t c,int opa,int r){
 lv_obj_t *o=lv_obj_create(p);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_bg_color(o,c,0);lv_obj_set_style_bg_opa(o,opa,0);lv_obj_set_style_radius(o,r,0);lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);return o;
}
static lv_obj_t *pill(lv_obj_t*p,const char*s,int x,int y,int w,int h,const lv_font_t*f){
 lv_obj_t *b=lv_button_create(p);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,h);lv_obj_t *l=lv_label_create(b);lv_label_set_text(l,s);app_theme_button(b,0);lv_obj_set_style_radius(b,LV_RADIUS_CIRCLE,0);lv_obj_set_style_text_font(l,f,0);lv_obj_center(l);lv_obj_set_style_border_width(b,1,LV_STATE_PRESSED);lv_obj_set_style_border_color(b,TC(ACCENT_PRIMARY),LV_STATE_PRESSED);return b;
}
static void shelf(lv_obj_t*p){
 /* Static primitives only: a cropped tower with canvas gradients fading its edges. */
 lv_color_t ghost=lv_color_mix(TC(ACCENT_PRIMARY),TC(TEXT_SECONDARY),100);
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
lv_obj_t *shelf_home_create(lv_obj_t *parent,lv_event_cb_t browse,lv_event_cb_t exit){
 lv_obj_t *group=box(parent,0,0,360,360,TC(CANVAS),0,0);shelf(group);
 lv_obj_t *l=txt(group,"Disc Shelf",28,81,304,TF(UI_40));lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);
 l=txt(group,"Navidrome browser",40,136,280,TF(UI_20));lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_text_color(l,TC(TEXT_SECONDARY),0);
 browse_button=pill(group,"Browse collection",57,181,246,70,TF(UI_26));
 lv_obj_set_style_border_width(browse_button,2,0);lv_obj_set_style_border_color(browse_button,TC(ACCENT_PRIMARY),0);lv_obj_set_style_border_opa(browse_button,150,0);
 lv_obj_add_event_cb(browse_button,browse,LV_EVENT_SHORT_CLICKED,(void *)(intptr_t)ARTISTS);
 status_label=txt(group,"Preparing collection...",60,260,240,TF(UI_16));lv_obj_set_height(status_label,22);lv_obj_set_style_text_align(status_label,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_text_color(status_label,TC(TEXT_SECONDARY),0);
 home_exit=pill(group,"Exit",112,286,136,44,TF(UI_22));lv_obj_add_event_cb(home_exit,exit,LV_EVENT_SHORT_CLICKED,NULL);
 return group;
}
lv_obj_t *shelf_home_exit(void){return home_exit;}
void shelf_home_status(const char *text,int busy){
 if(strcmp(lv_label_get_text(status_label),text))lv_label_set_text(status_label,text);
 if(busy)lv_obj_add_state(browse_button,LV_STATE_DISABLED);else lv_obj_remove_state(browse_button,LV_STATE_DISABLED);
}
