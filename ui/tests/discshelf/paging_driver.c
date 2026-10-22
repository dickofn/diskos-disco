#include "shelf.h"
#include "context.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static void wait_job(void) {
  for (int i = 0; i < 600; i++) {
    usleep(10000);
    pthread_mutex_lock(&ds_shared.mu);
    int b = ds_shared.busy;
    pthread_mutex_unlock(&ds_shared.mu);
    if (!b)
      return;
  }
  assert(0);
}
static void browse(int view, const char *id, int offset, char letter) {
  pthread_mutex_lock(&ds_shared.mu);
  ds_shared.req = (request_t){.view = view, .offset = offset, .letter = letter,.jump=letter!=0};
  if (id)
    snprintf(ds_shared.req.id, IDLEN, "%s", id);
  ds_shared.command = 1;
  pthread_cond_signal(&ds_shared.wake);
  ds_shared.busy = 1;
  pthread_mutex_unlock(&ds_shared.mu);
  wait_job();
  assert(!ds_shared.fatal);
}
int main(int argc, char **argv) {
  test_context_init();
  char err[256];
  assert(shelf_init(argv[1], err, sizeof err));
  pthread_t th;
  ds_shared.busy = 1;
  pthread_create(&th, 0, shelf_worker, 0);
  wait_job();
  browse(ARTISTS, 0, 0, 0);
  assert(ds_shared.count == 32 && ds_shared.more && ds_shared.total_count==10003);
  browse(ARTISTS,0,0,'A');assert(ds_shared.total_count==10003&&ds_shared.offset==1);
  browse(ARTISTS, 0, 0, 'Z');
  assert(!strcmp(ds_shared.rows[0].name, "Z Artist") && ds_shared.total_count==10003);
  browse(ARTISTS,0,0,'#');assert(ds_shared.total_count==10003 && ds_shared.offset==0);
  browse(ALBUMS, 0, 0, 0);
  assert(ds_shared.count == 32);
  browse(ALBUMS, 0, 32, 0);
  assert(ds_shared.count == 9);
  browse(ALBUMS, 0, 0, 'Z');
  assert(!strcmp(ds_shared.rows[0].name, "Z Album"));
  browse(ARTIST_ALBUMS, "artist1", 0, 0);
  assert(ds_shared.count == 32 && ds_shared.total_count==70);
  browse(TRACKS, "album1", 0, 0);
  assert(ds_shared.count == 32 && ds_shared.total_count==40);
  browse(TRACKS, "album1", 32, 0);
  assert(ds_shared.count == 8);
  browse(ARTIST_ALBUMS, "artist1", 32, 0);
  assert(ds_shared.count == 32 && ds_shared.rows[0].kind == ALBUMS);
  atomic_store(&ds_stopping, 1);
  shelf_wake();
  pthread_join(th, 0);
  puts("PASS large artist stream, complete cardinalities, bounded viewport windows, A-Z seek, nested back "
       "navigation");
}
