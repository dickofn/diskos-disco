#include "shelf.h"
#include <stdio.h>
atomic_int ds_stopping, ds_abort_job;
static int cb(const item_t *r, void *p) {
  printf("%d:%s:%s:%s\n", r->kind, r->id, r->name, r->artist);
  return 1;
}
int main(int argc, char **argv) {
  return ds_parse_response(argv[1], cb, 0) ? 0 : 1;
}
