/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DISCO_SHELF_H
#define DISCO_SHELF_H
#include "lvgl/lvgl.h"
void discshelf_create(lv_obj_t *root);
void discshelf_open(void);
void discshelf_landing(void);
void discshelf_tick(void);
void discshelf_stop(void);
int discshelf_busy(void);
int discshelf_back_consumed(void);
void discshelf_preview(const char *scene);
#endif
