#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bluetooth status worker (findings N04): a connected device anywhere in a long known-device list is found within a
few passes, each pass asking about at most 8 + the last connected one (production peer_worker, bluetoothctl stubbed)."""
import subprocess, tempfile
from pathlib import Path
ui = Path(__file__).resolve().parents[1]; src = (ui / "bt.c").read_text()
def fn(sig):
    start = src.index(sig)
    while src[start:src.index("\n", start)].split("/*")[0].rstrip().endswith(";"): start = src.index(sig, start + 1)
    o = src.index("{", start); d = 1; e = o + 1
    while d: d += (src[e] == "{") - (src[e] == "}"); e += 1
    return src[start:e]
C = r'''
#define _GNU_SOURCE
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static pthread_mutex_t g_pr_mu = PTHREAD_MUTEX_INITIALIZER;
static char g_pr_name[64]; static int g_pr_conn = 0, g_pr_running = 0; static uint32_t g_pr_at = 0, g_pr_have = 0;
static int NDEV = 12, CONNECTED = -1, asked;
static int run_cap_to(const char *cmd, char *out, int cap, int ms){
    (void)ms; out[0] = 0;
    if(!strncmp(cmd, "bluetoothctl devices", 20)){ int n = 0; for(int i = 0; i < NDEV; i++) n += snprintf(out + n, cap - n, "Device AA:BB:CC:DD:EE:%02X Phones %d\n", i, i); return n; }
    int i; if(sscanf(cmd, "bluetoothctl info AA:BB:CC:DD:EE:%x", &i) == 1){ asked++; return snprintf(out, cap, "Device x\n\tConnected: %s\n", i == CONNECTED ? "yes" : "no"); }
    return 0;
}
''' + fn("static int bt_mac_valid(") + "\n" + fn("static int bt_info_prop(") + "\n" + \
    src[src.index("static int g_pr_offset;"):src.index("\n}\n", src.index("static void *peer_worker(")) + 3] + r'''
int main(void){
    int positions[] = {0, 7, 8, 9, 11};
    for(unsigned p = 0; p < sizeof positions / sizeof *positions; p++){
        CONNECTED = positions[p]; g_pr_conn = 0; g_pr_offset = 0; g_pr_last[0] = 0; int passes = 0;
        while(!g_pr_conn && passes < 4){ asked = 0; peer_worker(NULL); assert(asked <= 9); passes++; }
        assert(g_pr_conn); printf("connected device at position %d found in %d pass(es)\n", CONNECTED + 1, passes);
        assert(passes <= 2);
    }
    asked = 0; peer_worker(NULL); assert(g_pr_conn && asked == 1);          /* next time: the last one is asked first */
    puts("PASS a connected device anywhere in a 12-device list is found within 2 passes (<= 9 lookups each); then at once");
    return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
    p = Path(d) / "t.c"; p.write_text(C)
    subprocess.run(["cc", "-std=gnu11", "-pthread", "-o", str(Path(d) / "t"), str(p)], check=True)
    print(subprocess.run([str(Path(d) / "t")], capture_output=True, text=True, check=True).stdout.strip())
