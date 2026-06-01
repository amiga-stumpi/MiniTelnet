# MiniTelnet

MiniTelnet is an experimental Kickstart/Workbench 1.3 Telnet/BBS client for
classic Amiga systems.

Version:

```text
MiniTelnet v0.12 by Marcel Jaehne (c)2026
```

It was split out of TheWire13 and remains designed for AmigaOS 1.3, 68000, and
the bebbo `m68k-amigaos-gcc` toolchain. Networking uses a classic
`bsdsocket.library` API directly; MiniTelnet does not call TheWire13 internal
stack APIs.

## Features

- Plain Intuition GUI, no GadTools/MUI/ReAction/ASL.
- Host/port connect controls and `minitelnet.conf` load/save.
- Telnet IAC filtering.
- Internal ANSI/VT100 parser with approximate CP437/IBM BBS character mapping.
- Runtime terminal font selection from `FONTS:`.
- Optional `xemibm.library` backend from `Settings -> XEM IBM Terminal`.
- XPR ZModem download via `Project -> ZModem Download` and
  `xprzmodem.library`.

## Requirements

- AmigaOS 1.3 / Kickstart 1.3.
- 68000-compatible build.
- `bsdsocket.library` at runtime.
- Optional: `xprzmodem.library` in `LIBS:` for ZModem download.
- Optional: `xemibm.library` in `LIBS:` for the XEM IBM terminal backend. MiniTelnet opens it without a minimum version so older OS1.3-compatible XEM builds are accepted.
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
font=ruby.font
font_size=8
terminal_mode=ANSI_IBM
```

## Current Limitations

- XPR support is currently download-only.
- XPR transfer is synchronous and blocks the UI while active.
- XEM support is optional and experimental; the internal renderer remains the
  default stable path.
- No address book yet.
- No ReqTools file/path requester yet.
