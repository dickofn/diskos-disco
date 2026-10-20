/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 diskOS contributors */
#ifndef CRASHLOG_H
#define CRASHLOG_H
/* A small crash log on the Disc: /usr/data/diskos_crash.log (kept under 64 KB). One line per Disco start, per player
 * restart seen by Disco, and per crash (signal, fault address, pc/ra/sp, thread, uptime), so a crash under heavy use
 * can be traced to a function later, without catching it live over SSH. */
void crashlog_install(void);                      /* once, early in main(): crash handlers + a start line */
void crashlog_note(const char *fmt, ...);         /* an ordinary line (not for use inside a signal handler) */
#endif
