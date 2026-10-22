/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#define PAGE 32
#define PICKS 256
#define STR 512
#define IDLEN 160
enum { HOME, ARTISTS, ALBUMS, TRACKS, ARTIST_ALBUMS, SMB_DIR, SMB_FILE, SMB_BROWSER };
typedef struct {
  int kind, track, disc, count, local;
  int64_t size;
  char id[IDLEN], name[STR], artist[STR], album[STR], suffix[24], cover[IDLEN];
} item_t;
typedef struct {
  int view, offset, scroll_y, jump;
  unsigned generation;
  char id[IDLEN], letter;
} request_t;
typedef struct {
  pthread_mutex_t mu;
  pthread_cond_t wake;
  item_t rows[PAGE], scratch[PAGE];
  int count, total_count, more, offset, view, revision, busy, done, fatal;
  unsigned result_generation;
  char letter;
  char message[256];
  item_t picks[PICKS];
  int npicks, replace;
  int64_t byte_done, byte_total;
  request_t req;
  int command;
} shared_t;
extern shared_t *ds_context;
#define ds_shared (*ds_context)
extern atomic_int ds_stopping, ds_abort_job;
int shelf_init(const char *conf, char *err, size_t cap);
void *shelf_worker(void *arg);
void shelf_close(void);
void shelf_wake(void);
int ds_api_fetch(const char *endpoint, const char *args, const char *file,
              int64_t max, int64_t expected, int media);
int ds_parse_response(const char *file, int (*emit)(const item_t *, void *),
                   void *ctx);
void ds_url_encode(const char *s, char *out, size_t cap);
void ds_status_text(const char *fmt, ...);
int ds_available_memory(void);
int ds_storage_headroom(uint64_t extra);
int shelf_sd_ready(void);

int ds_backend_start(const char *conf);
void ds_backend_stop(void);
int ds_backend_collect(void);
int ds_backend_active(void);
int ds_fetch_entry(int argc, char **argv);

#include "connection.h"
void *ds_smb_worker(void*);
