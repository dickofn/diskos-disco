# diskOS Disco! - to do

Live list. Released: 2.6.1 (tag v2.6.1). Older notes: `todo-history.md`.
How we work: owner suggests -> Claude offers fixes -> host renders for big visual changes -> owner's go ->
batch -> one test build -> a few days on the Disc -> one release, one bundle, owner pushes + tags.
List words: "add to the list" pins an item; "major" / "minor" sorts it; "priority" puts it first.
Links always point to releases/latest. No upstream diskOS version shown to users.

## Released: 2.6.0 (2026-10-21)
- Fast-skipping work (pacing, cover settle, history after 1 s) and the crash log tested on the owner's Disc; the
  Bluetooth fixes (three audits: B01-B07, B09 part, P01, P02, N01-N04; codec LDAC -> AAC -> SBC) and the hint /
  How-to update checked with tests and renders only. Watch `/usr/data/diskos_crash.log` reports.

## Waiting on users
- #13 / Bluetooth LDAC (2.5.8: music waits for the codec switch; no device search during playback): never tested
  with LDAC/AAC headphones by us; the reporter to confirm.

## Next feature
- **Offline mode** (owner, 2026-10-20): Settings > System > Offline mode hides and blocks everything online (Wi-Fi,
  weather, Last.fm, online lyrics/art, auto-tag, Music Assistant, AirPlay, Debug Mode); Bluetooth untouched and
  available any time. Decided: nothing kept for Last.fm, the weather space stays empty, name "Offline mode".
  Plan: `docs/plans/offline-mode.md`.

## Polish (owner, 2026-10-20)
- **Shorter README:** move the "New in 2.0 ... 2.5" history to the changelog; keep what Disco is, how to install,
  the quick answers and the links.
- **Screenshots refresh:** tour, website and the Windows guide's install screen (no Version step yet); multi-select
  and the newer Squash/Null screens aren't pictured anywhere. Host renders, round format.
- **Consistent message wording:** the same phrasing for "try again", the same dashes, sentence case in every pop-up.
- **Two stability leftovers:** Bluetooth reconnect attempts stop when Bluetooth is turned off or another device is
  chosen (today they can overlap the delayed device search); a large "Add to Favourites" selection is written in one
  go (one transaction, off the screen's thread) instead of item by item.

## Major
- **Dyslexic-friendly font** (owner, 2026-10-19): OpenDyslexic 3 under Display > Layout & extras, overriding text
  fonts in every theme at every size; icons, clocks, big numbers and non-Latin scripts keep their fonts. LVGL's
  runtime TTF renderer (one ~0.1-0.15 MB font file). Needs renders of all themes and a real Disc test.
- **Artwork cancellation (R07):** parked on branch `r07-art-cancel`. The decoder change itself is clean (exhaustive
  stop-point test), but the build crashed under fast skipping on the Disc. Resume only with a crash log from that
  build (the crash recorder now captures it).
- **From the 2.5.8 change specification:** R02 fully asynchronous queue operations (no database waits on the
  screen's thread; 2.5.9 only bounds them); R03 paged playlist rows, R04 compact history IDs, R05 group buffers
  sized by group count (about 18.6 MB reserved up front at 30,000 songs today); R08/R10 one decode into a shared
  output bundle and cache writes off the foreground worker; R09 reuse matched accents; R11 negative no-art cache;
  R13 -O2 experiment (measure on the Disc first).

## Minor
- From the Bluetooth audit and the delayed-pause report, not done yet: richer command logging (origin, queued vs
  delivered, telemetry age) for the next device trace; B08 a persistent pairing agent (check on the Disc first whether FiiO's
  software already provides one; first pairings seem to work); B09 device details/forget off the screen's thread
  (they can still hold the screen up to 4-5 s); B06 confirm which codec was really selected before releasing the
  hold; remove the unused legacy bt_codec_apply_async(); ui/tests/transport_test.py no longer builds (stale harness).
- Korean/CJK in a second, larger size for Music titles (about +0.4 MB per script).

## Experiments
- **WinUSB Windows installer:** upstream's native 1.2.1 Windows installer repacked with the Disco payload (no WSL,
  no usbipd; upstream's key removed). Owner to test on the laptop; if it works, offer it as the recommended Windows
  download and build it with each release.

## Decided against
- Boot-script changes (restarting into Disco after a crash, upstream's S97 fixes): "don't poke the bear".
- A separate settings file for Disco vs upstream diskOS: too much headache; the docs explain the leftovers instead.
- Translation catch-up for messages: translations are a courtesy.
- A "+3" badge for waiting skips, a higher skip cap, a shorter cover settle (0.3 s matches the skip pacing).
