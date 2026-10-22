#include "shelf.h"
#include "context.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void wait_job(void) {
  for (int i = 0; i < 600; i++) {
    usleep(100000);
    pthread_mutex_lock(&ds_shared.mu);
    int busy = ds_shared.busy;
    pthread_mutex_unlock(&ds_shared.mu);
    if (!busy)
      return;
  }
  fprintf(stderr, "Timed out\n");
  exit(3);
}
static void browse(int view, const char *id) {
  pthread_mutex_lock(&ds_shared.mu);
  ds_shared.req = (request_t){.view = view};
  if (id)
    snprintf(ds_shared.req.id, IDLEN, "%s", id);
  ds_shared.command = 1;
  pthread_cond_signal(&ds_shared.wake);
  ds_shared.busy = 1;
  pthread_mutex_unlock(&ds_shared.mu);
  wait_job();
  printf("BROWSE %d count=%d message=%s\n", view, ds_shared.count, ds_shared.message);
}
int main(int argc, char **argv) {
  test_context_init();
  char err[256];
  if (!shelf_init(argv[1], err, sizeof err)) {
    fprintf(stderr, "%s\n", err);
    return 1;
  }
  pthread_t t;
  ds_shared.busy = 1;
  pthread_create(&t, NULL, shelf_worker, NULL);
  wait_job();
  browse(ARTISTS, 0);
  browse(ALBUMS, 0);
  browse(ARTIST_ALBUMS, "artist1");
  browse(TRACKS, "album1");
  pthread_mutex_lock(&ds_shared.mu);
  ds_shared.picks[0] = (item_t){.kind = argc > 2 ? ALBUMS : ARTISTS};
  strcpy(ds_shared.picks[0].id, argc > 2 ? "album1" : "artist1");
  ds_shared.npicks = 1;
  ds_shared.busy = 1;
  ds_shared.command = 2;
  pthread_cond_signal(&ds_shared.wake);
  pthread_mutex_unlock(&ds_shared.mu);
  if (argc > 2 && !strcmp(argv[2], "cancel")) {
    usleep(500000);
    atomic_store(&ds_abort_job, 1);
  }
  wait_job();
  printf("DOWNLOAD %s\n", ds_shared.message);
  atomic_store(&ds_stopping, 1);
  shelf_wake();
  pthread_join(t, NULL);
  return 0;
}
