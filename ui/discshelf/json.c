/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Streaming JSON reader: fixed depth, fixed strings, no document-sized
 * allocations. */
#include "shelf.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  FILE *f;
  int c, depth, error, ok;
  unsigned bytes;
  int (*emit)(const item_t *, void *);
  void *ctx;
} parser_t;
static void next(parser_t *p) {
  if ((++p->bytes & 4095) == 0 &&
      (atomic_load(&ds_stopping) || atomic_load(&ds_abort_job))) {
    p->error = 1;
    p->c = EOF;
    return;
  }
  /* The worker is the sole owner of this stream. Avoid a lock per JSON byte. */
  p->c = fgetc_unlocked(p->f);
}
static void ws(parser_t *p) {
  while (p->c != EOF && isspace((unsigned char)p->c))
    next(p);
}
static int hex(int c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}
static unsigned unicode(parser_t *p) {
  unsigned v = 0;
  for (int i = 0; i < 4; i++) {
    next(p);
    int h = hex(p->c);
    if (h < 0) {
      p->error = 1;
      return 0;
    }
    v = v * 16 + h;
  }
  next(p);
  return v;
}
static void append(parser_t *p, char *s, size_t cap, size_t *n, int c) {
  if (!s)
    return;
  if (*n + 1 >= cap) {
    p->error = 1;
    return;
  }
  s[(*n)++] = (char)c;
}
static void string(parser_t *p, char *s, size_t cap) {
  size_t n = 0;
  if (p->c != '"') {
    p->error = 1;
    return;
  }
  next(p);
  while (p->c != EOF && p->c != '"' && !p->error) {
    int c = p->c;
    next(p);
    if (c == '\\') {
      c = p->c;
      next(p);
      switch (c) {
      case '"':
      case '\\':
      case '/':
        break;
      case 'b':
        c = 8;
        break;
      case 'f':
        c = 12;
        break;
      case 'n':
        c = 10;
        break;
      case 'r':
        c = 13;
        break;
      case 't':
        c = 9;
        break;
      case 'u': { /* unicode() expects to advance from the u */
        ungetc(p->c, p->f);
        p->c = 'u';
        unsigned u = unicode(p);
        if (u >= 0xd800 && u <= 0xdbff) {
          if (p->c != '\\') {
            p->error = 1;
            break;
          }
          next(p);
          if (p->c != 'u') {
            p->error = 1;
            break;
          }
          unsigned lo = unicode(p);
          if (lo < 0xdc00 || lo > 0xdfff) {
            p->error = 1;
            break;
          }
          u = 0x10000 + ((u - 0xd800) << 10) + (lo - 0xdc00);
        } else if (u >= 0xdc00 && u <= 0xdfff) {
          p->error = 1;
          break;
        }
        if (!u) {
          p->error = 1;
          break;
        }
        if (u < 128)
          append(p, s, cap, &n, u);
        else if (u < 2048) {
          append(p, s, cap, &n, 0xc0 | (u >> 6));
          append(p, s, cap, &n, 0x80 | (u & 63));
        } else if (u < 65536) {
          append(p, s, cap, &n, 0xe0 | (u >> 12));
          append(p, s, cap, &n, 0x80 | ((u >> 6) & 63));
          append(p, s, cap, &n, 0x80 | (u & 63));
        } else {
          append(p, s, cap, &n, 0xf0 | (u >> 18));
          append(p, s, cap, &n, 0x80 | ((u >> 12) & 63));
          append(p, s, cap, &n, 0x80 | ((u >> 6) & 63));
          append(p, s, cap, &n, 0x80 | (u & 63));
        }
        continue;
      }
      default:
        p->error = 1;
        break;
      }
    } else if (c < 32) {
      p->error = 1;
      break;
    }
    append(p, s, cap, &n, c);
  }
  if (s)
    s[n] = 0;
  if (p->c != '"')
    p->error = 1;
  else
    next(p);
}
static int kind_for(const char *key) {
  if (!strcmp(key, "artist"))
    return ARTISTS;
  if (!strcmp(key, "album"))
    return ALBUMS;
  if (!strcmp(key, "song"))
    return TRACKS;
  return 0;
}
static void value(parser_t *, const char *, int);
static void field(parser_t *p, item_t *r, const char *k, const char *v) {
#define COPY(key, member)                                                      \
  if (!strcmp(k, key)) {                                                       \
    if (strlen(v) >= sizeof r->member) {                                       \
      p->error = 1;                                                            \
      return;                                                                  \
    }                                                                          \
    strcpy(r->member, v);                                                      \
    return;                                                                    \
  }
  COPY("id", id);
  COPY("name", name);
  COPY("title", name);
  COPY("artist", artist);
  COPY("album", album);
  COPY("suffix", suffix);
  COPY("coverArt", cover);
  if (!strcmp(k, "track") || !strcmp(k, "discNumber") ||
      !strcmp(k, "songCount") || !strcmp(k, "albumCount") ||
      !strcmp(k, "size") || !strcmp(k,"year")) {
    if(!strcmp(k,"year") && !strcmp(v,"null"))return;
    char *end;
    errno = 0;
    long long number = strtoll(v, &end, 10);
    if (errno || *end || end == v || number < 0 ||
        (strcmp(k, "size") && number > INT_MAX)) {
      p->error = 1;
      return;
    }
    if (!strcmp(k,"year"))
      r->year=number<=9999?(int)number:0;
    else if (!strcmp(k, "track"))
      r->track = (int)number;
    else if (!strcmp(k, "discNumber"))
      r->disc = (int)number;
    else if (!strcmp(k, "size"))
      r->size = number;
    else
      r->count = (int)number;
  }
}
static void scalar(parser_t *p, char *v, size_t cap) {
  if (p->c == '"')
    string(p, v, cap);
  else {
    size_t n = 0;
    while (p->c != EOF && !strchr(",]} \t\r\n", p->c) && !p->error) {
      append(p, v, cap, &n, p->c);
      next(p);
    }
    v[n] = 0;
    if (!n)
      p->error = 1;
  }
}
static void object(parser_t *p, int kind) {
  item_t r = {0};
  r.kind = kind;
  next(p);
  ws(p);
  while (p->c != '}' && !p->error) {
    char key[128], v[STR];
    string(p, key, sizeof key);
    ws(p);
    if (p->c != ':') {
      p->error = 1;
      break;
    }
    next(p);
    ws(p);
    if (p->c == '{' || p->c == '[')
      value(p, key, 0);
    else {
      int wanted = !strcmp(key, "status") || !strcmp(key, "id") ||
                   !strcmp(key, "name") || !strcmp(key, "title") ||
                   !strcmp(key, "artist") || !strcmp(key, "album") ||
                   !strcmp(key, "suffix") || !strcmp(key, "coverArt") ||
                   !strcmp(key, "track") || !strcmp(key, "discNumber") ||
                   !strcmp(key, "size") || !strcmp(key, "songCount") ||
                   !strcmp(key, "albumCount") || !strcmp(key,"year");
      if (!wanted && p->c == '"') {
        string(p, NULL, 0);
        v[0] = 0;
      } else
        scalar(p, v, sizeof v);
      field(p, &r, key, v);
      if (!strcmp(key, "status") && p->depth == 2)
        p->ok = !strcmp(v, "ok");
    }
    ws(p);
    if (p->c == ',') {
      next(p);
      ws(p);
      if (p->c == '}')
        p->error = 1;
    } else if (p->c != '}')
      p->error = 1;
  }
  if (p->c == '}')
    next(p);
  else
    p->error = 1;
  if (kind && r.id[0] && r.name[0] && !p->error && !p->emit(&r, p->ctx))
    p->error = 1;
}
static void value(parser_t *p, const char *key, int array_kind) {
  if (++p->depth > 32) {
    p->error = 1;
    return;
  }
  if (atomic_load(&ds_stopping) || atomic_load(&ds_abort_job)) {
    p->error = 1;
    return;
  }
  if (p->c == '{')
    object(p, array_kind?array_kind:!strcmp(key,"album")?ALBUMS:0);
  else if (p->c == '[') {
    int kind = kind_for(key);
    next(p);
    ws(p);
    while (p->c != ']' && !p->error) {
      value(p, key, kind);
      ws(p);
      if (p->c == ',') {
        next(p);
        ws(p);
        if (p->c == ']')
          p->error = 1;
      } else if (p->c != ']')
        p->error = 1;
    }
    if (p->c == ']')
      next(p);
    else
      p->error = 1;
  } else {
    char v[STR];
    scalar(p, v, sizeof v);
  }
  p->depth--;
}
int ds_parse_response(const char *path, int (*emit)(const item_t *, void *),
                   void *ctx) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return 0;
  char buffer[16384];
  setvbuf(f, buffer, _IOFBF, sizeof buffer);
  parser_t p = {.f = f, .emit = emit, .ctx = ctx};
  next(&p);
  ws(&p);
  value(&p, "", 0);
  ws(&p);
  int ok = !p.error && p.ok && p.c == EOF && !ferror(f);
  fclose(f);
  return ok;
}
