#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Is sound going out (2.6.1)? The kernel's playback state as independent evidence before Disco powers off or
interrupts music it merely didn't "see" playing. Production ui_alsa_running() on a fake /proc/asound tree."""
import subprocess, tempfile
from pathlib import Path
ui = Path(__file__).resolve().parents[1]; src = (ui / "main.c").read_text()
start = src.index("int ui_alsa_running(void){"); end = src.index("\n}\n", start) + 3
cases = [
    ("playing on the DAC", {"card0/pcm0p/sub0/status": "state: RUNNING\n"}, 1),
    ("stopped",            {"card0/pcm0p/sub0/status": "closed\n"}, 0),
    ("paused/prepared",    {"card0/pcm0p/sub0/status": "state: PREPARED\n"}, 0),
    ("only recording",     {"card0/pcm0c/sub0/status": "state: RUNNING\n", "card0/pcm0p/sub0/status": "closed\n"}, 0),
    ("second card plays",  {"card0/pcm0p/sub0/status": "closed\n", "card1/pcm0p/sub0/status": "state: RUNNING\n"}, 1),
    ("no playback device", {"card0/pcm0c/sub0/status": "closed\n"}, -1),
]
with tempfile.TemporaryDirectory() as d:
    d = Path(d)
    for i, (name, files, want) in enumerate(cases):
        root = d / ("c%d" % i)
        for rel, txt in files.items():
            f = root / rel; f.parent.mkdir(parents=True, exist_ok=True); f.write_text(txt)
        C = ('#include <dirent.h>\n#include <stdio.h>\n#include <string.h>\n#define ASOUND_DIR "%s"\n' % root) + src[start:end] + \
            '\nint main(void){ printf("%d\\n", ui_alsa_running()); return 0; }\n'
        (d / "t.c").write_text(C)
        subprocess.run(["cc", "-std=gnu11", "-o", str(d / "t"), str(d / "t.c")], check=True)
        got = int(subprocess.run([str(d / "t")], capture_output=True, text=True, check=True).stdout)
        assert got == want, (name, got, want); print("ok  %-20s -> %d" % (name, got))
print("PASS kernel playback state: running output found on any card; recording, paused and stopped don't count")
