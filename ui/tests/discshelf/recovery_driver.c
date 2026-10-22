#include "shelf.h"
#include "context.h"
#include "sqlite3.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
extern char ds_state_dir[1100];
static void wait_job(void) {
  for (int i = 0; i < 3000; i++) {
    usleep(10000);
    pthread_mutex_lock(&ds_shared.mu);
    int busy = ds_shared.busy;
    pthread_mutex_unlock(&ds_shared.mu);
    if (!busy)
      return;
  }
  assert(!"worker timeout");
}
static void browse(int view, const char *id) {
  pthread_mutex_lock(&ds_shared.mu);
  ds_shared.req = (request_t){.view = view};
  if (id)
    snprintf(ds_shared.req.id, IDLEN, "%s", id);
  ds_shared.count = 0;
  ds_shared.command = 1;
  ds_shared.busy = 1;
  pthread_cond_signal(&ds_shared.wake);
  pthread_mutex_unlock(&ds_shared.mu);
  wait_job();
  assert(!ds_shared.fatal);
}
int main(int argc, char **argv) {
  test_context_init();
  (void)argc;
  char err[256];
  assert(shelf_init(argv[1], err, sizeof err));
  pthread_t worker;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  assert(!pthread_attr_setstacksize(&attr, 256 * 1024));
  ds_shared.busy = 1;
  assert(!pthread_create(&worker, &attr, shelf_worker, NULL));
  pthread_attr_destroy(&attr);
  wait_job();
  assert(!ds_shared.fatal);
  /* Inspect the real private lookup populated with a 50,000-track fixture. */
  char path[1200];
  snprintf(path, sizeof path, "%s/catalog.sqlite", ds_state_dir);
  sqlite3 *inspect;
  sqlite3_stmt *q;
  assert(sqlite3_open_v2(path, &inspect, SQLITE_OPEN_READONLY, NULL) ==
         SQLITE_OK);
  assert(sqlite3_prepare_v2(inspect,
                            "EXPLAIN QUERY PLAN SELECT path FROM local WHERE "
                            "album='a' AND artist='b'",
                            -1, &q, NULL) == SQLITE_OK);
  assert(sqlite3_step(q) == SQLITE_ROW);
  const char *plan = (const char *)sqlite3_column_text(q, 3);
  assert(strstr(plan, "SEARCH") && strstr(plan, "local_lookup"));
  sqlite3_finalize(q);
  sqlite3_close(inspect);
  assert(ds_shared.count == 0); /* Startup preload deliberately failed. */
  browse(ARTISTS,NULL); /* Retry must not reuse an empty cache. */
  assert(ds_shared.count == 1);
  browse(ARTIST_ALBUMS, "artist1");
  assert(ds_shared.count == 2 && ds_shared.rows[0].kind == ALBUMS);
  browse(TRACKS, "album1"); /* Failed replacement of the ds_shared browse table. */
  assert(ds_shared.count == 0);
  browse(ARTIST_ALBUMS, "artist1"); /* Previous valid page must survive. */
  assert(ds_shared.count == 2 && ds_shared.rows[0].kind == ALBUMS);
  browse(TRACKS, "album1");
  assert(ds_shared.count == 2 && ds_shared.rows[0].kind == TRACKS);
  clockid_t worker_clock;
  struct timespec before, after;
  assert(!pthread_getcpuclockid(worker, &worker_clock));
  clock_gettime(worker_clock, &before);
  usleep(200000);
  clock_gettime(worker_clock, &after);
  long long idle_ns = (after.tv_sec - before.tv_sec) * 1000000000LL +
                      after.tv_nsec - before.tv_nsec;
  assert(idle_ns < 1000000); /* No periodic polling while idle. */
  atomic_store(&ds_stopping, 1);
  shelf_wake();
  pthread_join(worker, NULL);
  printf("PASS 50k local tracks use indexed album lookup, failed-response "
         "rollback/retry, 256 KiB worker stack, idle worker CPU=%lld ns over "
         "200 ms, idle Exit\n",
         idle_ns);
}
