# fpp-midi

Trigger FPP Commands from MIDI messages, for [Falcon Player (FPP)](https://github.com/FalconChristmas/fpp).

Connect a MIDI keyboard, pad controller, or control surface and use it to drive FPP — start
playlists, set overlay model colours, adjust volume, or run any other FPP Command.

## Features

- Listens on any number of detected MIDI devices/ports, each individually enabled.
- Optional handling of System Exclusive (SysEx), Time Code, and Sense events — left off by default
  to keep processing overhead down.
- Per-event conditions to filter on the raw MIDI bytes, so one device can drive many different
  actions (e.g. respond to note-on but not note-off).
- Expression support, so a command argument can be computed from the MIDI message — for example
  scaling key velocity into a colour.
- A "Last Messages" panel showing the most recent 25 messages FPPD received, to help work out which
  bytes to match on.

## Installation

Install from **Content Setup → Plugins** in the FPP web UI, then restart FPPD.

The build needs `librtmidi`; `install_librtmidi.sh` is run automatically by the Makefile if the
headers aren't already present.

## Configuration

**Content Setup → MIDI** in the FPP web UI.

Detected devices appear at the top of the page and can be enabled individually. Each event you add
has:

- **Description** — a label for your own use; FPP ignores it.
- **Conditions** — filters on the bytes of the incoming message.
- **Command** — the FPP Command to run, with its arguments.

### Expressions in command arguments

An argument beginning with a single `=` is evaluated as a formula. An argument without a leading `=`
is treated as a string, but `%%name%%` placeholders are substituted — for example `Matrix-%%b1%%`.

Available variables:

| Variable | Meaning |
|----------|---------|
| `b1`–`b5` | First five bytes of the MIDI message (0 if not present) |
| `note` | Same as `b2` |
| `velocity` | Same as `b3` |
| `channel` | Lower 4 bits of `b1` (0–15) |
| `pitch` | Combined from `b3` and `b2`, range −8192 to 8191 |
| `control` | Same as `b2` |

Three helper functions are provided in addition to the standard operators:

- `rgb(r, g, b)` — r/g/b values 0–255 packed into a single colour integer.
- `hsv(h, s, v)` — hue/saturation/value 0–1 packed into a single colour integer.
- `if(cond, tExp, fExp)` — returns `tExp` when `cond` is non-zero, otherwise `fExp`.

For example, a red that scales with how hard the key was struck:

```
=rgb(velocity*2,0,0)
```

Expression evaluation uses the [TinyExpr](https://github.com/codeplea/tinyexpr) library.

## License

GPLv2 — see [LICENSE](LICENSE).
