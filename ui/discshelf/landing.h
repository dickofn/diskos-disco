/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "lvgl/lvgl.h"
lv_obj_t *shelf_home_create(lv_obj_t*,lv_event_cb_t,lv_event_cb_t);
lv_obj_t *shelf_home_exit(void);
void shelf_home_status(const char*,int);
