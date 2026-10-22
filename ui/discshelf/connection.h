/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stddef.h>
#include <stdatomic.h>
enum { DS_NAVIDROME, DS_SMB };
typedef struct {char server[1024],user[256],password[256],dest[1024],share[256],domain[128];int remember;} ds_profile_t;
typedef struct {int source;ds_profile_t profile[2];} ds_settings_t;
extern atomic_int ds_source;
extern char ds_smb_share[256],ds_smb_domain[128];
void ds_secret_clear(void*,size_t);
void ds_session_clear(void);
int ds_settings_load(const char*,ds_settings_t*);
int ds_settings_save(const char*,const ds_settings_t*,char*,size_t);
int ds_settings_validate(const ds_settings_t*,int,char*,size_t);
