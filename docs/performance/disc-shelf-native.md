# Native Disc Shelf integration: validation

Development branch based on public Disco 2.6.2 (`e8b3b3a`). No Bluetooth, power-button or timeout-lock changes.
This is a test build pending physical Disc acceptance, not a new published release/tag.

## Implemented

- Builtin All apps / navigation / shortcuts entry with the legacy `disc-shelf` identity, hiding old standalone duplicates.
- Native screen, theme-role colours/fonts, compact Library header and spacing, continuous 32-row viewport and position indicator.
- Shared Library swipe recognizer; remote-ID selections survive recycled windows. Only Download is offered. Back cancels selection before navigating.
- Existing local-presence colour; selected missing items show a check, selected local items an SD-card/check marker; partial albums remain distinguishable.
- Reused A-Z grid and letter circle; instance-safe ownership and destruction. Seeking jumps within the whole collection rather than hiding other letters.
- Separate first-use selection hint respecting the theme/global hint setting.
- Server setup with the native keyboard and masked password field; existing private shelf.conf is preserved until explicitly saved.
- Missing-only and explicit Download-again modes. Atomic replacement is restricted to an exact server/track-ID manifest, within the download root. Tag-only local matches are retained.
- Exact album membership cached after download/check; paths checked on worker-side browse. Incremental local-index updates avoid automatic full rescans. No two-way synchronization or periodic catalogue refresh.
- Backend starts only on open; on-demand context, 256 KiB worker stack and condition-variable idle wait. Cancel, close, shutdown and USB handover stop work and release the SD lease.
- No standalone LVGL initialization, display takeover, global signal changes or SQLITE_TMPDIR changes in the native module. The private fetch helper retains parent-death handling.

## Host checks

Run from the repository root:

```
python3 ui/tests/discshelf_backend_test.py
python3 ui/tests/discshelf_lifecycle_test.py
python3 ui/tests/discshelf_registration_test.py
python3 ui/tests/discshelf/run.py
make -C ui CROSS= -j2 host-render
```

The standalone-derived HTTP/SQLite suite covers JSON/Unicode limits, 10,003 artists and bounded viewport windows,
complete list cardinality and A-Z jump offsets, nested navigation, 50,000 local tracks and indexed lookup,
failed-response rollback/retry, streamed downloads, duplicate avoidance, repeated launch, short/error bodies and cancellation.
Its idle worker CPU check measured 0 ns over a 200 ms condition-variable wait on this host.

Additional production-function tests cover default skipping, missing tracks in partial albums, exact-ID replacement,
failed/cancelled replacement preserving the original, and refusing replacement of tag-only matches. The real lifecycle
harness completed 64 worker start/stop cycles with no remaining context or SD lease. `sizeof(shared_t)` is 612,448
bytes on the host; the context and separate 32-row UI array are allocated only while open. Actual peak Disc RSS is not measured.

Native LVGL tests use real touch input to select by swipe, verify a Download-only action, Back cancellation and repeated
preview teardown in Disco, Null and Squash. The existing letter-wheel tests pass in each theme (grid, bidirectional
scrub, wrap, held-touch suppression, 1-second release timeout, Back and option restoration). Real LVGL screenshots
cover browsing, selection, both pickers, confirmation, landing and server setup in the three themes and two appearances.

Existing Bluetooth hold/resume/bulk-favourites tests and queue tests are also exercised. LeakSanitizer is unavailable
in this host runtime; tests use `ASAN_OPTIONS=detect_leaks=0`. Address/undefined-behaviour instrumentation remains enabled
where specified by each harness. Lifecycle release is checked explicitly rather than claiming a working leak-sanitizer run.

## Scope and device acceptance

The version string remains 2.6.2; this is a Disc Shelf development build, not the public 2.6.2 payload. No tag was moved
and nothing was pushed. Build records and delivery notes identify the source commit and binary hashes.

On a Disc, test your real Navidrome catalogue, Wi-Fi failure/recovery, a partial album, an explicit redownload,
card removal/full-card handling, cancellation, repeated open/close, normal playback and switching to USB storage.
Confirm zero network children after exit and compare RSS/CPU before/after repeated opens. Host tests cannot establish
hardware battery consumption, peak runtime memory or physical SD-removal behaviour.

The old standalone archive/source remains available for older firmware; the complete ZIP no longer duplicates its
executable inside `apps/`. Existing device executables/configuration are not deleted, preserving rollback compatibility.

## Final device build size

GCC 11.2 repository toolchain build, static little-endian MIPS32r2/o32, FP64, NaN2008:

- Public 2.6.2 payload: 6,160,032 bytes.
- Native Disc Shelf test payload: 6,219,588 bytes.
- Increase: 59,556 bytes (58.16 KiB, about 0.97%).
- ELF BSS grows by 10,136 bytes; the large app buffers remain on-demand allocations.
- Exact binary/source identity is recorded in payload/mq_ui.build.json.

This does not measure peak device RSS or physical battery consumption.

## SMB3 development follow-up

Final SMB3 test payload: 6,373,628 bytes, versus 6,219,588 for the previous native Disc Shelf payload: +154,040 bytes (150.4 KiB). MIPS ELF text/data/BSS for the preceding equivalent link were 6,297,212 / 72,964 / 4,456,344 bytes, versus 6,146,656 / 71,668 / 4,430,512 for the native baseline. Fixed BSS increased about 25.2 KiB; actual peak RSS/current draw require hardware measurements. Source changes also include the reviewed layout/icon/typography fixes.

Protocol work is confined to the existing on-demand worker. Directory query buffers are bounded to 64 KiB, file reads to 32 KiB, and UI data stays at 32 rows. Idle SMB workers wait on the existing condition variable. There is no automatic discovery, keepalive, sync or refresh timer. Session/password and source-selection details are documented in `docs/plans/disc-shelf-smb3.md`.

## Download display and buffered Navidrome writes

- Download status uses at most three lines with ellipsis, smaller percentage plus
  a four-sample rolling speed, and a larger transferred/total MiB line. Native
  LVGL fixtures cover Disco, Null and Squash; displayed fixture speed is synthetic.
- Speed reads existing byte counters only while the screen is visible. A per-file
  serial resets its samples even for consecutive equal-size files. No new timer,
  network request, directory scan or idle worker is added.
- Navidrome now coalesces small wget pipe reads in a 128 KiB temporary heap buffer
  instead of writing each read immediately (previous stack buffer: 32 KiB).
  Partial buffers flush after a 250 ms threshold (observed within 306 ms in the
  stalled-sender test, plus possible scheduler/storage delay) and at EOF.
  The allocation is freed on all transfer exits. No increased worker stack.
- Resource checks remain at the existing 1 MiB cadence, cancellation is checked
  before each loop and partial write, expected-size/media checks remain, and final
  fsync/atomic publication are unchanged. Progress publication is limited to about
  five updates per second plus completion, rather than every small read.
- `python3 ui/tests/discshelf/transfer_buffer_test.py` verifies byte-exact output
  including a short final buffer, short writes, EINTR, ENOSPC, cancellation,
  SD/resource failure and partial-buffer flushing while the sender stalls.
  Optional `SHELF_BASELINE_NET=/path/to/old/net.c` compares write-call counts.
- A 10 MiB + 18 byte localhost transfer made 81 SD-file write calls versus 326
  before (75.2% fewer in that run). Both runs took approximately 0.035 seconds:
  this proves batching, **not an on-device throughput improvement**. Actual speed,
  power and battery effects require measurement on the Disc with the same server,
  track and SD card. This patch adds no rate limit or parallel downloads, does not
  change SMB read sizes, and does not remove durability or space-reserve checks.

## Shared SongSphere guard

SongSphere now uses the same static guard layout as Disc Shelf, with its own
sphere vector icon, mood-map contours and explicit Open SongSphere button.
Disco/Null menu browsing, All apps and app shortcuts show SCR_APPLAND first;
only the button calls the existing sphere_ui_open(). Direct album/artist/song
tagging remains immediate. The guard creates no sphere engine, DB connection,
worker or animation timer. Native LVGL fixtures exercise all three themes,
preview switching/cleanup, menu and app dispatch, and a wrapped ssdb_open that
asserts zero opens before clicking and one attempt after clicking Open.
