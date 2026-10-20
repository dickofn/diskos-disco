# Plan: Offline mode

Status: planned (owner, 2026-10-20). To start once the 2.5.9-t2 crash investigation has settled.

## Goal

"Offline in spirit": one switch after which **nothing online can be started, even by accident**. It came from a user
who never used Weather or Last.fm but kept opening them by mistake. So the point is less "radio off" than "the online
parts are out of the way and can't do anything".

**Bluetooth is not part of it.** It stays visible and can be turned on at any time (headphones, Bluetooth DAC and
streaming modes all keep working).

## The switch

- **Settings > System > Offline mode**, On/Off. Explanation: "Hides Wi-Fi and everything online (weather, Last.fm,
  online lyrics and art, auto-tag, Music Assistant, AirPlay). Bluetooth stays available."
- Off by default. A How-to page and a first-use hint are not needed; the Settings explanation is enough.
- Small status hint while on: the Wi-Fi symbol in the status bar is simply absent (it already is when Wi-Fi is off),
  no new icon. Revisit if people get confused.

## What changes when it's on

Two layers, on purpose:

1. **Navigation hides** the online things (menus, shortcuts, Quick Settings, apps, working modes, Home/Music).
2. **Every online feature also refuses to run** while offline mode is on, in its own module. So an entry point that
   slipped through (a theme, a deep link, a forgotten shortcut) still can't go online. This second layer is what
   actually delivers "can't be triggered by accident".

| Feature | Navigation | Settings | Behaviour (module guard) | Code |
|---|---|---|---|---|
| Wi-Fi | Quick Settings tile hidden | Network > Wi-Fi dimmed: "Offline mode is on" | radio turned off; not started at boot; the keep-alive doesn't restart it; the user's own Wi-Fi on/off choice is remembered for later | wifi.c, quicksettings.c, settings.c, main.c (wifi_supervise) |
| Weather | hidden on Home and Music; Weather app, menu item and shortcut hidden | "Weather on Home" dimmed | no fetch, timer stopped; an open Weather screen goes back Home | weather.c, apps.c (SC_DEFAULT has weather), disco.c, null_theme.c, simple_theme.c, home widgets |
| Last.fm | app and menu item hidden | Last.fm settings dimmed | no scrobbling, nothing sent (queue decision below) | lastfm.c, apps.c |
| Online lyrics | - | "Online lyrics" dimmed | not fetched; lyrics files and embedded lyrics still show | lyrics.c |
| Online album art | - | "Online album art" dimmed | not fetched; embedded and folder covers still show | netart.c |
| Auto-tag | - | Playback > Auto-tag dimmed | no lookups | tagfix.c |
| Music Assistant (Sendspin) | app and menu item hidden | MA settings dimmed | player stopped; doesn't start | ma.c, sendspin.c, disco.c, null_theme.c |
| AirPlay | hidden in Working modes | - | can't be selected | modes.c |
| Debug Mode (SSH) | - | dimmed: "Turn off Offline mode first" (it needs Wi-Fi) | refused | debug_ui.c, settings.c |
| Online & cache page | - | dimmed as a whole | - | settings.c |
| Bluetooth | **unchanged** | **unchanged** | **unchanged** | - |

Clock: unaffected. The Disc has a battery-backed clock that Disco loads at start; internet time only corrects it
while Wi-Fi is on, so offline the clock just drifts slowly (seconds per week).

## Switching on and off

- **On:** remember whether Wi-Fi was on; turn Wi-Fi off; stop Music Assistant; cancel weather/lyrics/art/auto-tag work
  in flight; if an online screen is open (Weather, Last.fm, Music Assistant), go back to Home; refresh menus, Quick
  Settings and Home so the entries disappear at once.
- **Off:** turn Wi-Fi back on only if it was on before; every feature returns to its own setting (nothing that was off
  gets turned on); entries reappear.
- **At start-up with offline on:** Wi-Fi is not started (even if its own setting says on); Music Assistant doesn't
  start; no weather or other fetch is scheduled. Bluetooth starts exactly as it does today.

## Navigation details per theme

Hidden entries are filtered where each list is built, through one shared check, so the user's own menu and shortcut
choices are kept and come back when offline mode is turned off.

- **Disco:** navigation circle items (Weather, Sendspin, Last.fm via Apps), Shortcuts, Quick Settings Wi-Fi tile, Music
  screen weather.
- **Squash:** launcher items, Quick Settings tile, Home weather.
- **Null:** drawer items (weather, ma), Quick Settings, Home weather.
- **Ring, Braun, Stone:** Home weather, Quick Settings tile, Apps list.
- **All:** Apps list (Weather, Last.fm), Working modes (AirPlay), Shortcuts. A shortcut slot whose app is hidden shows
  nothing (no substitute), and returns when offline mode is off.

## Implementation steps

1. `offline_mode()` (config `offline`, default 0) and `net_allowed()` in one small module; on/off transitions.
2. Module guards first (the layer that matters most): Wi-Fi start and keep-alive, weather, Last.fm, lyrics, netart,
   tagfix, MA/Sendspin, Debug Mode, AirPlay mode. Each returns early with no side effects.
3. Navigation filters: menus, shortcuts, Quick Settings, Apps, Working modes, Home/Music weather, in all six themes.
4. Settings: the Offline mode switch; dimmed rows with "Offline mode is on" (the existing dimmed-row mechanism).
5. Start-up behaviour; the remembered Wi-Fi state.
6. Docs: How-to entry, README feature line, release notes, the switching guide ("Offline mode is on" as a reason
   something is missing).

## Testing

- Unit test: with offline on, each guarded module refuses (no fetch, no network call, no Wi-Fi start) and has no side
  effects; turning it off restores the remembered Wi-Fi state and nothing else.
- Renders, offline on and off, every theme: Home/Music without weather, Quick Settings without Wi-Fi, menus and
  launchers, Apps, Shortcuts, Working modes, Settings with dimmed rows.
- On the Disc: turn on with Wi-Fi on → Wi-Fi off, entries gone; restart → stays offline, Wi-Fi stays off; Bluetooth
  headphones connect and play; turn off → Wi-Fi back as before; Weather/Last.fm work again.

## Open decisions (owner)

1. **Last.fm while offline:** don't record scrobbles at all (my suggestion: "offline" should mean nothing is kept to
   send later), or keep queuing them to send when offline mode is turned off?
2. **Weather on Home:** empty space, or let the layout close the gap (theme by theme)?
3. **Name:** "Offline mode" (clear) or something softer like "Focus mode"? Offline is clearer about what it does.

## What it won't do

- Save space: the features stay in the one shared program. Saved lyrics and covers on the card stay too.
- Save battery beyond what Wi-Fi off already saves; the gain is that Wi-Fi reliably stays off.
- Touch Bluetooth.

Estimate: one to two sessions plus renders, then a test build on the Disc.
