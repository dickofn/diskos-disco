/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 diskOS contributors */
#include "crashlog.h"
#include "version.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#ifndef CRASHLOG_PATH
#define CRASHLOG_PATH "/usr/data/diskos_crash.log"
#endif
#define CRASHLOG_MAX (64 * 1024)                 /* larger: start again from empty (the newest lines matter) */

/* ---- inside the signal handler: only async-signal-safe calls (open/write/fstat/ftruncate/close, clock_gettime,
 * syscall, prctl), no stdio, no malloc; numbers are formatted by hand into a stack buffer ---- */
typedef struct { char b[400]; int n; } line_t;
static void put(line_t *l, const char *s){ while(*s && l->n < (int)sizeof l->b - 2) l->b[l->n++] = *s++; }
static void hex(line_t *l, unsigned long v){
    char t[20]; int k = 0;
    do { t[k++] = "0123456789abcdef"[v & 15]; v >>= 4; } while(v && k < 16);
    put(l, "0x"); while(k && l->n < (int)sizeof l->b - 2) l->b[l->n++] = t[--k];
}
static void dec(line_t *l, long v){
    char t[24]; int k = 0; int neg = v < 0; unsigned long u = neg ? (unsigned long)-v : (unsigned long)v;
    do { t[k++] = (char)('0' + u % 10); u /= 10; } while(u && k < 22);
    if(neg) put(l, "-");
    while(k && l->n < (int)sizeof l->b - 2) l->b[l->n++] = t[--k];
}
static void write_line(const line_t *l){
    int fd = open(CRASHLOG_PATH, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if(fd >= 0){
        struct stat st;
        if(fstat(fd, &st) == 0 && st.st_size > CRASHLOG_MAX && ftruncate(fd, 0) == 0) {}
        ssize_t w = write(fd, l->b, (size_t)l->n); (void)w;
        fsync(fd); close(fd);
    }
    ssize_t w2 = write(2, l->b, (size_t)l->n); (void)w2;   /* and to stderr, wherever that goes */
}
static volatile sig_atomic_t g_in_handler;
static void on_crash(int sig, siginfo_t *si, void *ucv){
    if(g_in_handler) return;                              /* a crash while recording: just let the default act */
    g_in_handler = 1;
    unsigned long pc = 0, ra = 0, sp = 0;
    ucontext_t *uc = ucv;
#if defined(__mips__)
    pc = (unsigned long)uc->uc_mcontext.pc;
    ra = (unsigned long)uc->uc_mcontext.gregs[31];
    sp = (unsigned long)uc->uc_mcontext.gregs[29];
#elif defined(__x86_64__)
    pc = (unsigned long)uc->uc_mcontext.gregs[REG_RIP];
    sp = (unsigned long)uc->uc_mcontext.gregs[REG_RSP];
#else
    (void)uc;
#endif
    struct timespec up; clock_gettime(CLOCK_MONOTONIC, &up);
    char tname[17] = {0}; prctl(PR_GET_NAME, (unsigned long)tname, 0, 0, 0);
    line_t l = { .n = 0 };
    put(&l, "CRASH v="); put(&l, DISCO_VERSION);
    put(&l, " sig="); dec(&l, sig);
    put(&l, " code="); dec(&l, si ? si->si_code : 0);
    put(&l, " addr="); hex(&l, si ? (unsigned long)si->si_addr : 0);
    put(&l, " pc="); hex(&l, pc);
    put(&l, " ra="); hex(&l, ra);
    put(&l, " sp="); hex(&l, sp);
    put(&l, " tid="); dec(&l, (long)syscall(SYS_gettid));
    put(&l, " thread="); put(&l, tname);
    put(&l, " uptime="); dec(&l, (long)up.tv_sec);
    put(&l, "\n");
    write_line(&l);
    /* SA_RESETHAND has put the default action back: returning re-runs the faulting instruction, which now ends the
     * process exactly as before (abort() raises again by itself). The watchdog then does what it always did. */
}
static char g_altstack[16384];                            /* a crash from a stack overflow still gets recorded */
void crashlog_install(void){
    stack_t ss = { .ss_sp = g_altstack, .ss_size = sizeof g_altstack, .ss_flags = 0 };
    sigaltstack(&ss, NULL);
    struct sigaction sa; memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_crash;
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    const int sigs[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT };
    for(unsigned i = 0; i < sizeof sigs / sizeof sigs[0]; i++) sigaction(sigs[i], &sa, NULL);
    struct timespec up; clock_gettime(CLOCK_MONOTONIC, &up);
    crashlog_note("start v=%s uptime=%ld", DISCO_VERSION, (long)up.tv_sec);
}
void crashlog_note(const char *fmt, ...){
    char msg[300]; va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof msg, fmt, ap); va_end(ap);
    time_t now = time(NULL); struct tm tm; char ts[24] = "?";
    if(now > 1600000000 && localtime_r(&now, &tm)) strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &tm);
    line_t l = { .n = 0 };
    put(&l, ts); put(&l, " "); put(&l, msg); put(&l, "\n");
    write_line(&l);
}
