# MiniTelnet Porting Notes

MiniTelnet starts from upstream DCTelnet as reference material, preserved in `reference/dctelnet/`.
The imported reference revision is `beefea2` from `https://github.com/bruno-frederic/dctelnet`.

## Upstream Dependencies Found

The upstream source and documentation still target Kickstart/Workbench 2.00+ (`V36`). A scan of `reference/dctelnet/` found these areas that are not suitable for a direct AmigaOS 1.3 build:

- `gadtools.library`: used by generated GUI code and option/requester helpers.
- Tag APIs: `OpenWindowTags`, `WA_*`, `GT_*`, `GTTX_*`, `GTIN_*`, `GTST_*`.
- `NewObject` and generated GUI support in `guis.c`.
- Public screen and iconify/AppIcon behavior referenced by docs and headers.
- ReqTools and bundled third-party trees aimed at later OS/library versions in some cases.
- XPR integration: upstream uses V36-era UI around the libraries, so MiniTelnet provides its own OS1.3 callbacks instead of compiling upstream code directly.

## MiniTelnet Approach

The MiniTelnet code does not compile upstream DCTelnet directly. It creates a new OS1.3-safe client in `src/` and keeps upstream as reference.

Current v0.14 choices:

- Plain Intuition `OpenWindow`, `NewWindow`, classic gadgets, and `IDCMP_VANILLAKEY`.
- Direct `bsdsocket.library` calls through explicit library-vector wrappers.
- Minimal Telnet IAC filtering: `DO` -> `WONT`, `WILL` -> `DONT`, `SB ... SE` skipped.
- Simple raw terminal rendering into the window RastPort.
- XPR ZModem download is implemented with MiniTelnet socket/file callbacks and `xprzmodem.library`.
- XEM IBM output was removed from the runtime path because the available XEM library requires `keymap.library`, which is not part of plain OS1.3.

## Deferred Work

- ReqTools connect requester.
- XPR ZModem upload through `xprzmodem.library`.
- Address book and persistent preferences.
- Workbench icon/tooltype integration.
