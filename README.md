# MiniTelnet

MiniTelnet is an experimental Kickstart/Workbench 1.3 Telnet/BBS client for
classic Amiga systems.

Version:

```text
MiniTelnet v0.15 by Marcel Jaehne (c)2026
```

It was split out of TheWire13 and remains designed for AmigaOS 1.3, 68000, and
the bebbo `m68k-amigaos-gcc` toolchain. Networking uses a classic
`bsdsocket.library` API directly; MiniTelnet does not call TheWire13 internal
stack APIs.

## Features

- Plain Intuition GUI, no GadTools/MUI/ReAction/ASL.
- Menu-driven operation: `Project -> Connect` opens a Host/Port dialog; `minitelnet.conf` load/save persists settings.
- Telnet IAC filtering.
- Internal ANSI/VT100 parser with CP437/IBM BBS character handling and ANSI color rendering.
- Runtime terminal font selection from `FONTS:`.
- XPR ZModem download via `Project -> ZModem Download` and
  `xprzmodem.library`.

## Requirements

- AmigaOS 1.3 / Kickstart 1.3.
- 68000-compatible build.
- `bsdsocket.library` at runtime.
- Optional: `xprzmodem.library` in `LIBS:` for ZModem download.
- Toolchain: `/opt/amiga/bin/m68k-amigaos-gcc`.

## Build

```sh
make clean && make
```

Output:

```text
build/MiniTelnet
```

The public application/window name is MiniTelnet. The binary name is kept as
`MiniTelnet`.

Useful debug builds:

```sh
make clean && make DCTELNET13_NET_DEBUG=1
make clean && make DCTELNET13_ANSI_DEBUG=1
make clean && make DCTELNET13_RENDER_DEBUG=1
```

## Documentation

See:

- [docs/minitelnet.md](docs/minitelnet.md)
- [docs/porting.md](docs/porting.md)

## Configuration

MiniTelnet loads and saves `minitelnet.conf` in the current/startup drawer.

Example:

```text
host=cpbbs.de
port=2323
font=ibm.font
font_size=8
terminal_mode=ANSI_IBM
```

## Current Limitations

- XPR support is currently download-only.
- XPR transfer is synchronous and blocks the UI while active.
- XEM libraries are not used because the available XEM builds require `keymap.library`, which is not part of a plain OS1.3 setup.
- No address book yet.
- No ReqTools file/path requester yet.
