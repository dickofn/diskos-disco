#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Outgoing player queue (Bluetooth findings N01): frames made for a player session that has since restarted are
dropped instead of delivered; a cancelled operation can withdraw its waiting frames; settings still go out."""
import subprocess, tempfile
from pathlib import Path
ui = Path(__file__).resolve().parents[1]; src = (ui / "ipc.c").read_text()
start = src.index("#define TX_MAX 48"); end = src.index("\n}\n", src.index("int ipc_drop_queued(")) + 3
C = r'''
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static int g_send_err; static unsigned g_generation = 1; static int busy = 1;
static char delivered[16][64]; static int nd;
static int ipc_rx_frames(void){ return 1; }
static int tx_try(const char *f, int *err){ (void)err; if(busy) return 1; snprintf(delivered[nd++], 64, "%s", f); return 0; }
''' + src[start:end] + r'''
int main(void){
    /* the report's sequence: busy player, a toggle and a codec request accepted, session changes, player frees up */
    assert(ipc_send_internal("0201000C0000", 0) == 0 && ipc_send_internal("06b3001D0003AA:BB:CC:DD:EE:FF", 0) == 0);
    assert(ipc_send_internal("0715000C0030", 0) == 0);          /* a volume setting, also waiting */
    g_generation = 2; busy = 0; ipc_tx_pump();
    assert(nd == 1 && !strcmp(delivered[0], "0715000C0030"));   /* only the setting went out */
    puts("PASS frames for an earlier player session are dropped; settings still delivered");
    /* targeted withdrawal: a cancelled codec switch's frame leaves the line, the rest keeps its order */
    nd = 0; busy = 1;
    ipc_send_internal("0666000C0002", 0); ipc_send_internal("06b3001D0004AA:BB:CC:DD:EE:FF", 0); ipc_send_internal("0657000C0008", 0);
    assert(ipc_drop_queued("06b3001D0004") == 1);
    busy = 0; ipc_tx_pump();
    assert(nd == 2 && !strcmp(delivered[0], "0666000C0002") && !strcmp(delivered[1], "0657000C0008"));
    puts("PASS a cancelled operation withdraws its waiting frame; the others go out in order");
    return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
    p = Path(d) / "t.c"; p.write_text(C)
    subprocess.run(["cc", "-std=gnu11", "-pthread", "-o", str(Path(d) / "t"), str(p)], check=True)
    print(subprocess.run([str(Path(d) / "t")], capture_output=True, text=True, check=True).stdout.strip())
