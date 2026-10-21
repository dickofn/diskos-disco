#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Auto-route poll (2.6.0 regression): a link probe that TIMES OUT is "don't know" and changes nothing; only three
completed probes in a row that find no A2DP PCM forget the route. A re-route restarts the player's Bluetooth service
(06c1), so a false "link gone" dropped the headphones mid-song. Production bt_autoroute_poll_cb, probe stubbed."""
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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct lv_timer_t lv_timer_t;
static uint32_t now = 1000; static uint32_t lv_tick_get(void){ return now; }
static char g_bt_autorouted[20], g_disc_mac[20], route[20]; static uint32_t g_disc_until;
static int routes, forgets, cancels, mode;          /* mode: 1 link up, 0 completed empty, -1 time-out */
static int run_cap_to(const char *cmd, char *out, int cap, int ms){
    (void)cmd; (void)ms;
    if(mode < 0){ out[0] = 0; return -1; }
    if(mode == 0){ out[0] = 0; return 0; }
    return snprintf(out, cap, "/org/bluealsa/hci0/dev_AA_BB_CC_DD_EE_FF/a2dpsrc/sink\n");
}
#define run_cap_to_real run_cap_to
static const char *ui_route_mac(void){ return route; }
int ui_route_bt(const char *mac){ routes++; snprintf(route, sizeof route, "%s", mac); return 0; }
void ui_route_forget(const char *why){ (void)why; forgets++; route[0] = 0; }
void ui_bt_codec_upgrade_cancel(void){ cancels++; }
static void bt_remember_last(const char *mac){ (void)mac; }
''' + fn("static int run_cap_bounded(") + "\n" + fn("static int bt_mac_valid(") + "\n" + fn("static void bt_autoroute_poll_cb(") + r'''
static void poll(int m, int n){ mode = m; for(int i = 0; i < n; i++){ bt_autoroute_poll_cb(NULL); now += 3000; } }
int main(void){
    poll(1, 1); assert(routes == 1);                                  /* headphones connect: routed once */
    poll(-1, 10); poll(1, 3);                                         /* a run of slow probes, then fine again */
    assert(routes == 1 && forgets == 0 && cancels == 0);
    puts("PASS probes that time out change nothing: no forgotten route, no re-route (no Bluetooth restart)");
    poll(0, 2); poll(1, 2); assert(routes == 1 && forgets == 0);      /* two real empty answers: still nothing */
    puts("PASS one or two empty answers (a codec switch, a busy moment) don't re-route either");
    poll(0, 3); assert(forgets == 1 && cancels == 1);                 /* really gone: forgotten after three */
    poll(1, 1); assert(routes == 2);                                  /* back: full route again */
    puts("PASS a real disconnect (three completed empty probes) is forgotten and the headphones are routed afresh");
    return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
    p = Path(d) / "t.c"; p.write_text(C)
    subprocess.run(["cc", "-std=gnu11", "-o", str(Path(d) / "t"), str(p)], check=True)
    print(subprocess.run([str(Path(d) / "t")], capture_output=True, text=True, check=True).stdout.strip())
