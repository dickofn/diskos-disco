/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "app_theme.h"
#include "null_theme.h"
void app_theme_button(lv_obj_t *b,int ball){(void)ball;
 lv_obj_set_style_bg_color(b,TC(SURFACE),0);lv_obj_set_style_bg_opa(b,LV_OPA_COVER,0);
 lv_obj_set_style_border_width(b,1,0);lv_obj_set_style_border_color(b,TC(ACCENT_PRIMARY),0);
 lv_obj_set_style_radius(b,th_null()?0:LV_RADIUS_CIRCLE,0);kit_keep(b);
 lv_obj_t*l=lv_obj_get_child(b,0);if(l){lv_obj_set_style_text_font(l,TF(UI_20),0);lv_obj_set_style_text_color(l,TC(TEXT_PRIMARY),0);kit_keep(l);}
}
