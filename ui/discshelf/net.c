/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include "shelf.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
extern char **environ;
char ds_server[1024], ds_username[256], ds_password[256], ds_download_root[1024],
    ds_state_dir[1100];
static int net_test;
void ds_status_text(const char *fmt, ...) {
  pthread_mutex_lock(&ds_shared.mu);
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(ds_shared.message, sizeof ds_shared.message, fmt, ap);
  va_end(ap);
  pthread_mutex_unlock(&ds_shared.mu);
}
void ds_url_encode(const char *s, char *out, size_t cap) {
  const char *hex = "0123456789ABCDEF";
  size_t n = 0;
  for (const unsigned char *p = (void *)s; *p; p++) {
    int plain = (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                (*p >= '0' && *p <= '9') || strchr("-_.~", *p);
    if (n + (plain ? 1 : 3) >= cap) {
      out[0] = 0;
      return;
    }
    if (plain)
      out[n++] = *p;
    else {
      out[n++] = '%';
      out[n++] = hex[*p >> 4];
      out[n++] = hex[*p & 15];
    }
  }
  out[n] = 0;
}
int ds_available_memory(void) {
  FILE *f = fopen("/proc/meminfo", "r");
  if (!f)
    return 0;
  char line[200];
  unsigned long n;
  int ok = 0;
  while (fgets(line, sizeof line, f))
    if (sscanf(line, "MemAvailable: %lu", &n) == 1) {
      ok = n >= 8192;
      break;
    }
  fclose(f);
  return ok;
}
static int space_ok(const char *path, uint64_t extra) {
  struct statvfs s;
  return !statvfs(path, &s) &&
         (uint64_t)s.f_bavail * s.f_frsize >= 64ULL * 1024 * 1024 + extra;
}
int ds_storage_headroom(uint64_t extra) { return space_ok(ds_state_dir, extra); }
static time_t mono(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec;
}
static int magic_ok(const unsigned char *b, size_t n) {
  return n >= 12 &&
         (!memcmp(b, "fLaC", 4) || !memcmp(b, "ID3", 3) ||
          (b[0] == 255 && (b[1] & 0xe0) == 0xe0) || !memcmp(b, "OggS", 4) ||
          !memcmp(b, "RIFF", 4) || !memcmp(b, "FORM", 4) ||
          !memcmp(b + 4, "ftyp", 4) || !memcmp(b, "MAC ", 4) ||
          !memcmp(b, "DSD ", 4) || !memcmp(b, "FRM8", 4) ||
          !memcmp(b, "wvpk", 4) ||
          (b[0] == 0x30 && b[1] == 0x26 && b[2] == 0xb2 && b[3] == 0x75));
}
int ds_api_fetch(const char *endpoint, const char *args, const char *path,
              int64_t max, int64_t expected, int media) {
  if (atomic_load(&ds_stopping) || atomic_load(&ds_abort_job) || !shelf_sd_ready())
    return 0;
  char u[800], p[800], url[4096];
  ds_url_encode(ds_username, u, sizeof u);
  ds_url_encode(ds_password, p, sizeof p);
  if (!u[0])
    return 0;
  int len = snprintf(
      url, sizeof url,
      "%s/rest/%s.view?u=%s&p=%s&v=1.16.1&c=DiscShelf&f=json%s%s", ds_server,
      endpoint, u, p, args && *args ? "&" : "", args ? args : "");
  if (len < 0 || len >= (int)sizeof url)
    return 0;
  if (!ds_available_memory() ||
      !space_ok(ds_state_dir, expected > 0 ? (uint64_t)expected : 0)) {
    ds_status_text("Not enough RAM or SD space (64 MiB reserve).");
    return 0;
  }
  int fd =
      open(path, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (fd < 0)
    return 0;
  int pipes[2];
  if (pipe2(pipes, O_CLOEXEC)) {
    close(fd);
    unlink(path);
    return 0;
  }
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions, pipes[1], 1);
  posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0);
  posix_spawn_file_actions_addclose(&actions, pipes[0]);
  posix_spawn_file_actions_addclose(&actions, pipes[1]);
  char parent[32];
  snprintf(parent, sizeof parent, "%ld", (long)getpid());
  char *app_av[] = {"disc-shelf", "--shelf-fetch", parent, url, NULL};
  char *test_av[] = {"wget", "-q", "-O", "-", "-T", "15", url, NULL};
  pid_t child;
  int rc = net_test
               ? posix_spawnp(&child, "wget", &actions, NULL, test_av, environ)
               : posix_spawn(&child, "/proc/self/exe", &actions, NULL, app_av,
                             environ);
  posix_spawn_file_actions_destroy(&actions);
  close(pipes[1]);
  if (rc) {
    close(pipes[0]);
    close(fd);
    unlink(path);
    ds_status_text("wget unavailable: cannot connect.");
    return 0;
  }
  unsigned char buffer[32768], head[16];
  size_t hn = 0;
  int64_t total = 0, checked = 0, reported = 0;
  pthread_mutex_lock(&ds_shared.mu);
  ds_shared.byte_done = 0;
  ds_shared.byte_total = expected;
  pthread_mutex_unlock(&ds_shared.mu);
  int ok = 1, status = 0, reaped = 0;
  time_t last = mono(), began = last;
  for (;;) {
    if (atomic_load(&ds_stopping) || atomic_load(&ds_abort_job) ||
        mono() - last > 20 || (!media && mono() - began > 120)) {
      ok = 0;
      break;
    }
    struct pollfd pollfd = {pipes[0], POLLIN, 0};
    int pr = poll(&pollfd, 1, 100);
    if (pr < 0) {
      if (errno == EINTR)
        continue;
      ok = 0;
      break;
    }
    if (!pr)
      continue;
    ssize_t n = read(pipes[0], buffer, sizeof buffer);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      ok = 0;
      break;
    }
    if (!n)
      break;
    last = mono();
    if (total + n > max || (expected > 0 && total + n > expected)) {
      ok = 0;
      break;
    }
    if (hn < sizeof head) {
      size_t take = (size_t)n < sizeof head - hn ? (size_t)n : sizeof head - hn;
      memcpy(head + hn, buffer, take);
      hn += take;
    }
    if (total - checked >= 1024 * 1024 || !total) {
      if (!shelf_sd_ready() || !space_ok(ds_state_dir, 0) || !ds_available_memory()) {
        ok = 0;
        ds_status_text("Stopped: SD unavailable or resources low.");
        break;
      }
      checked = total;
    }
    for (ssize_t off = 0; off < n;) {
      ssize_t w = write(fd, buffer + off, n - off);
      if (w < 0 && errno == EINTR)
        continue;
      if (w <= 0) {
        ok = 0;
        break;
      }
      off += w;
    }
    if (!ok)
      break;
    total += n;
    if (total - reported >= 256 * 1024 || total == expected) {
      pthread_mutex_lock(&ds_shared.mu);
      ds_shared.byte_done = total;
      pthread_mutex_unlock(&ds_shared.mu);
      reported = total;
    }
  }
  close(pipes[0]);
  if (!ok)
    kill(child, SIGKILL);
  for (int i = 0; i < 300; i++) {
    pid_t r = waitpid(child, &status, WNOHANG);
    if (r == child) {
      reaped = 1;
      break;
    }
    if (r < 0 && errno != EINTR) {
      ok = 0;
      break;
    }
    usleep(10000);
  }
  if (!reaped) {
    kill(child, SIGKILL);
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
    }
    ok = 0;
  }
  ok = ok && WIFEXITED(status) && WEXITSTATUS(status) == 0 && total > 0 &&
       (expected <= 0 || total == expected) &&
       (!media ||
        (media == 1 ? magic_ok(head, hn)
                    : (hn >= 8 && ((head[0] == 255 && head[1] == 216) ||
                                   !memcmp(head, "\211PNG\r\n\032\n", 8)))));
  if (ok && fsync(fd))
    ok = 0;
  if (close(fd))
    ok = 0;
  if (!ok)
    unlink(path);
  return ok;
}
int ds_config_read(const char *path, int test, char *err, size_t cap) {
  net_test = test;
  ds_server[0]=ds_username[0]=ds_password[0]=ds_download_root[0]=0;
  FILE *f = fopen(path, "r");
  if (!f) {
    snprintf(err, cap, "Open Server to set up Navidrome.");
    return 0;
  }
  char line[2048];
  while (fgets(line, sizeof line, f)) {
    if (!strchr(line, '\n') && !feof(f)) {
      fclose(f);
      snprintf(err, cap, "Config line too long.");
      return 0;
    }
    line[strcspn(line, "\r\n")] = 0;
    if (!*line || *line == '#')
      continue;
    char *eq = strchr(line, '=');
    if (!eq)
      continue;
    *eq++ = 0;
    char *dst = NULL;
    size_t n = 0;
#define CONF(k, v)                                                             \
  if (!strcmp(line, k)) {                                                      \
    dst = v;                                                                   \
    n = sizeof v;                                                              \
  }
    CONF("server", ds_server)
    CONF("username", ds_username)
    CONF("password", ds_password) CONF("download_dir", ds_download_root) if (dst) {
      if (strlen(eq) >= n) {
        fclose(f);
        snprintf(err, cap, "Config value too long.");
        return 0;
      }
      strcpy(dst, eq);
    }
  }
  fclose(f);
  size_t n = strlen(ds_server);
  while (n && ds_server[n - 1] == '/')
    ds_server[--n] = 0;
  if ((strncmp(ds_server, "http://", 7) && strncmp(ds_server, "https://", 8)) ||
      strpbrk(ds_server, " \t\r\n?#") || !ds_username[0] || !ds_password[0]) {
    snprintf(err, cap, "Set server, username, password\nin Server settings.");
    return 0;
  }
  if (!ds_download_root[0])
    strcpy(ds_download_root, "/tmp/sdcard/Navidrome");
  if ((!net_test && strncmp(ds_download_root, "/tmp/sdcard/", 12)) ||
      strstr(ds_download_root, "..") || strpbrk(ds_download_root, "\r\n") ||
      strlen(ds_download_root) > 900) {
    snprintf(err, cap, "download_dir must be on SD.");
    return 0;
  }
  return 1;
}
