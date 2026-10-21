#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Start on the right codec (2.6.1): a play request made while the AAC/LDAC switch is still due waits and starts once
the switch is confirmed, so the music never starts on SBC and pauses a few seconds in. Production defer block from
main.c; the list-play itself is a stand-in that defers the same way the real one does."""
import subprocess, tempfile
from pathlib import Path
ui = Path(__file__).resolve().parents[1]; src = (ui / "main.c").read_text()
start = src.index("static struct { int active, running, kind, list_type"); end = src.index("\n}\n", src.index("int ui_play_defer_transport(")) + 3
block = src[start:end].replace("static int ui_play_list_ex(int list_type, const char *name, int pos1, int build_idx);\n", "")
C = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t now = 1000; static uint32_t lv_tick_get(void){ return now; } static uint32_t lv_tick_elaps(uint32_t t){ return now - t; }
static char g_route_mac[20] = "AA:BB:CC:DD:EE:FF"; static int g_playing, g_q_internal; static uint32_t g_hold_since;
static int pending = 1, toggles, plays, last_internal, toasts;
int bt_codec_upgrade_pending(void){ return pending; }
static int ipc_send_cmd(const char *f){ if(!strcmp(f, "0201000C0000")) toggles++; return 0; }
static void ui_toast(const char *m){ (void)m; toasts++; }
static int play_waits_for_codec(void);
static void play_defer_note(void);
static struct dp_fwd *unused;
static int ui_play_list_ex(int list_type, const char *name, int pos1, int build_idx);
''' + block + r'''
static int ui_play_list_ex(int list_type, const char *name, int pos1, int build_idx){
    if(play_waits_for_codec()){ g_dp.kind = 1; g_dp.list_type = list_type; g_dp.pos1 = pos1; g_dp.build_idx = build_idx;
        g_dp.q_internal = g_q_internal; snprintf(g_dp.name, sizeof g_dp.name, "%s", name); play_defer_note(); return 1; }
    plays++; last_internal = g_q_internal; return 1;
}
static void run(uint32_t until){ while(now < until){ now += 100; play_defer_tick(); } }
int main(void){
    g_q_internal = 1; assert(ui_play_list_ex(5, "", 3, -1) == 1 && plays == 0 && toasts == 1); g_q_internal = 0;   /* a queue play */
    run(3000); assert(plays == 0);                                   /* switch still due: waiting */
    pending = 0; run(3300); assert(plays == 1 && last_internal == 1); /* confirmed: starts once, still marked internal */
    puts("PASS a play waits for the codec switch and starts once it's confirmed (queue marker kept)");
    pending = 1; plays = 0; now = 10000; ui_play_list_ex(1, "Album", 1, -1); run(10500);
    assert(ui_play_defer_transport(1) == 1 && plays == 1);           /* second Play press: at once */
    puts("PASS a second Play press starts it at once");
    plays = 0; now = 20000; ui_play_list_ex(1, "Album", 1, -1); run(29900); assert(plays == 0); run(30200); assert(plays == 1);
    puts("PASS it starts after 10 s at the latest");
    plays = 0; now = 40000; ui_play_list_ex(1, "Album", 1, -1); g_route_mac[0] = 0; run(40200); assert(plays == 1);
    strcpy(g_route_mac, "AA:BB:CC:DD:EE:FF");
    puts("PASS it starts at once if the Bluetooth route goes away");
    toggles = 0; now = 50000; assert(ui_play_defer_transport(1) == 1 && toggles == 0);   /* Play on a paused song */
    pending = 0; run(50300); assert(toggles == 1);
    g_playing = 1; pending = 1; assert(ui_play_defer_transport(1) == 0);                   /* Pause while playing: untouched */
    puts("PASS Play on a paused song waits too; Pause is never held back");
    return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
    p = Path(d) / "t.c"; p.write_text(C)
    subprocess.run(["cc", "-std=gnu11", "-o", str(Path(d) / "t"), str(p)], check=True)
    print(subprocess.run([str(Path(d) / "t")], capture_output=True, text=True, check=True).stdout.strip())
