#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Born on the right codec (2.6.2): BlueALSA's start arguments prefer the chosen codec for headphones known to support
it, so the link never starts on SBC and never switches; unknown or other headphones start normally, and connecting to
headphones that may lack the codec restarts BlueALSA with the normal order. Production functions from bt.c."""
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
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define BT_CODEC_N 5
#define BTC_AAC  2u
#define BTC_LDAC 4u
static int fw = 257, codec = 0, restarts; static char last[20], keys[8][32]; static int vals[8], nk;
static int fw_os_ver(void){ return fw; }
static const char *cfg_get_str(const char *k, const char *d){ return !strcmp(k, "bt_last") ? last : d; }
static int cfg_get_int(const char *k, int d){ if(!strcmp(k, "bt_codec")) return codec;
    for(int i = 0; i < nk; i++) if(!strcmp(keys[i], k)) return vals[i]; return d; }
static void cfg_set_int(const char *k, int v){ for(int i = 0; i < nk; i++) if(!strcmp(keys[i], k)){ vals[i] = v; return; }
    snprintf(keys[nk], 32, "%s", k); vals[nk++] = v; }
static char last_cmd[512]; static int system_(const char *c){ snprintf(last_cmd, sizeof last_cmd, "%s", c); if(strstr(c, "killall bluealsa")) restarts++; return 0; }
#define system system_
''' + fn("static unsigned bt_codec_need(") + "\n" + fn("int bt_codec_pick(") + "\n" + \
    "static int g_born;\n" + fn("static void bt_caps_key(") + "\n" + fn("static int bt_caps_get(") + "\n" + fn("static void bt_caps_set(") + "\n" + \
    fn("static int bt_born_for(") + "\n" + fn("static void bt_alsa_args(") + "\n" + fn("static void bt_codec_prepare_for(") + r'''
static int has(const char *a, const char *s){ return strstr(a, s) != NULL; }
int main(void){
    char a[256]; const char *hp = "AA:BB:CC:DD:EE:FF", *other = "11:22:33:44:55:66";
    snprintf(last, sizeof last, "%s", hp);
    codec = 1; assert(bt_born_for(hp) == 0);                                  /* unknown headphones: normal */
    bt_alsa_args(a, sizeof a, 0); assert(has(a, "--codec=sbc") && !has(a, "--codec=-sbc"));
    bt_caps_set(hp, BTC_AAC | BTC_LDAC);                                      /* learned at first connection */
    codec = 1; assert(bt_born_for(hp) == 1); bt_alsa_args(a, sizeof a, 1); assert(has(a, "--codec=-sbc --codec=-ldac"));
    codec = 4; assert(bt_born_for(hp) == 2); bt_alsa_args(a, sizeof a, 2); assert(has(a, "--codec=-sbc") && !has(a, "--codec=-ldac") && has(a, "--ldac-quality=high"));
    codec = 2; bt_alsa_args(a, sizeof a, 2); assert(has(a, "--ldac-quality=mobile"));
    bt_caps_set(hp, BTC_AAC); codec = 3; assert(bt_born_for(hp) == 1);       /* LDAC chosen, only AAC there: AAC */
    codec = 0; assert(bt_born_for(hp) == 0);                                  /* SBC chosen: normal */
    codec = 1; fw = 240; assert(bt_born_for(hp) == 0); fw = 257;              /* other firmware: normal */
    puts("PASS start arguments: born on AAC/LDAC (with the chosen LDAC quality) only for headphones known to support it");
    g_born = 1; bt_codec_prepare_for(hp); assert(restarts == 0 && g_born == 1);            /* the usual headphones */
    bt_codec_prepare_for(other); assert(restarts == 1 && g_born == 0 && has(last_cmd, "--codec=sbc"));   /* others */
    bt_codec_prepare_for(other); assert(restarts == 1);                                     /* already normal */
    puts("PASS connecting to other or unknown headphones restarts BlueALSA with the normal order; the usual ones don't");
    return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
    p = Path(d) / "t.c"; p.write_text(C)
    subprocess.run(["cc", "-std=gnu11", "-o", str(Path(d) / "t"), str(p)], check=True)
    print(subprocess.run([str(Path(d) / "t")], capture_output=True, text=True, check=True).stdout.strip())
