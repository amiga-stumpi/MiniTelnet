# MiniTelnet

`MiniTelnet` is an experimental Kickstart/Workbench 1.3 Telnet/BBS client for TheWire13.

Version string:

```text
MiniTelnet v0.12 by Marcel Jaehne (c)2026
```

## Requirements

- AmigaOS 1.3 / 68000.
- TheWire13 running with `bsdsocket.library` installed/openable.
- A SANA-II network device configured for TheWire13.

Planned later integrations:

- `xprzmodem.library` in `LIBS:` for `Project -> ZModem Download`.
- Optional `xemibm.library` in `LIBS:` for `Settings -> XEM IBM Terminal`.
- `reqtools.library` for future requesters.

## Current v0.12 Scope

Implemented:

- Opens a plain OS1.3 Intuition window with a size gadget.
- Host and port string fields aligned on the top control row.
- Connect, Hangup, and Clear buttons on a second control row.
- A horizontal separator keeps terminal output visually below the controls.
- Dynamic terminal area that recalculates rows/columns when the window is resized.
- Dedicated terminal font, defaulting to `ruby.font` size 8 with `topaz.font` size 11 fallback.
- OS1.3 menu bar with Project and Settings menus.
- `Settings -> Save Settings` writes `minitelnet.conf` in the startup/current directory.
- `Project -> Info` opens a small OS1.3-safe MiniTelnet info dialog.
- `Project -> ZModem Download` starts an XPR receive through `xprzmodem.library`.
- `Settings -> XEM IBM Terminal` toggles optional `xemibm.library` output.
- Startup automatically loads `minitelnet.conf` when present.
- Batched dirty-row terminal rendering for faster Telnet/BBS output.
- Direct `bsdsocket.library` DNS/connect/send/recv path.
- Minimal Telnet IAC negotiation.
- Initial ANSI/VT100 parsing for common BBS sequences.
- Default `ANSI_IBM` terminal mode with CP437-to-Amiga fallback mapping for BBS line art.
- Incoming text rendered in the window.
- Keyboard input sent with `IDCMP_VANILLAKEY`.

Not implemented yet:

- ZModem upload.
- XEM configuration and full VT340 replacement mode.
- ReqTools connect requester.
- Address book.
- Scrollback.
- Full ANSI color rendering and robust VT terminal emulation.

## Build

```sh
make
```

Output:

```text
build/MiniTelnet
```

## Test

1. Start TheWire13 and confirm DNS/TCP works.
2. Run `MiniTelnet` (public window title: MiniTelnet).
3. Leave the default host/port or enter:

```text
telehack.com
23
```

4. Click `Connect`.
5. Server output should appear in the terminal area.
6. Resize the window to change the visible terminal rows and columns.
7. Keyboard input is sent to the Telnet server.
8. Click `Hangup` or close the window to disconnect.

## Configuration

MiniTelnet v0.12 loads optional settings from `minitelnet.conf` in the drawer/current directory from which the program was started. Missing or invalid files are ignored and defaults are used. For Shell launch this is the current directory. Workbench-specific startup drawer handling is not yet implemented; launch from the desired drawer or save from the running program.

Use `Settings -> Save Settings` to write the current Host field, Port field, terminal font, font size, and terminal mode. The file format is simple `key=value` text:

```text
# MiniTelnet configuration
host=cpbbs.de
port=2323
font=ruby.font
font_size=8
terminal_mode=ANSI_IBM
```

Supported keys are `host`, `port`, `font`, `font_size`, and `terminal_mode`. Invalid ports fall back to 23. Missing font fields keep the normal font fallback chain. If a configured font cannot be opened, MiniTelnet keeps the default/fallback terminal font and continues startup.

## ZModem Download

`Project -> ZModem Download` opens `xprzmodem.library`, installs MiniTelnet socket and file callbacks, and starts XPR receive mode. Incoming ZModem bytes are read directly from the active Telnet socket, with Telnet `0xff` escaping collapsed before data is passed to XPR. Files are written in the current startup drawer because there is no ReqTools path requester yet.

Limitations for v0.12:

- Download only; upload is not enabled yet.
- The transfer runs synchronously and the main window is not fully interactive until it completes.
- `xprzmodem.library` must be copied to `LIBS:`.
- Start the download after the remote BBS has begun a ZModem send.

## XEM Terminal Library

`Settings -> XEM IBM Terminal` toggles an optional `xemibm.library` backend. When enabled, MiniTelnet initializes XEM on the current Intuition window and sends filtered Telnet payload bytes to `XEmulatorWrite()`. The internal ANSI_IBM renderer remains the default because it respects MiniTelnet's control-row layout and has been tested more heavily. If XEM drawing conflicts with the MiniTelnet controls on a given system, toggle the menu item again to return to the internal renderer.

This is the first XEM integration pass. It does not yet expose XEM options, preferences, macro keys, or a separate XEM-only terminal window.

## Notes

This is a new OS1.3 client, not a direct build of upstream DCTelnet. Upstream DCTelnet is kept under `reference/dctelnet/` as porting reference only.


## Terminal and ANSI Support

MiniTelnet v0.12 opens initially to the visible screen size, keeps the top control row fixed, and resizes the terminal area to the current window size. The terminal grid uses the active window font metrics and enforces a minimum of 20 columns by 5 rows. Existing visible text is preserved as far as practical during resize.

The ANSI parser handles common Telnet/BBS sequences:

- `CR`, `LF`, backspace, tab, and BEL ignore.
- `ESC[H`, `ESC[row;colH`, and `ESC[f`.
- `ESC[nA`, `ESC[nB`, `ESC[nC`, `ESC[nD`.
- `ESC[J`, `ESC[2J`, `ESC[K`, and `ESC[2K`.
- `ESC[0m`, `ESC[1m`, `ESC[7m`, foreground `30..37`, and background `40..47`.

Color and attribute sequences are parsed and stored, but internal drawing remains monochrome so it works on a plain OS1.3 Workbench screen. Unknown escape sequences are ignored instead of being printed raw.

The default terminal mode is `ANSI_IBM`. Printable bytes are interpreted as IBM Codepage 437 after Telnet IAC and ANSI escape filtering, so classic PC BBS line art no longer appears as random Amiga umlaut/math glyphs. The mapping is intentionally conservative: box drawing becomes `+`, `-`, and `|`, block/shade characters become `#`, `.`, or `:`, and unsupported extended characters become `?`. This is readable on stock Amiga fonts. Optional XEM IBM mode can be enabled from the Settings menu for systems with `xemibm.library`, but the internal renderer remains the default stable path.

Debug builds can enable parser diagnostics with:

```sh
make DCTELNET13_ANSI_DEBUG=1
make DCTELNET13_TERM_DEBUG=1
make DCTELNET13_RENDER_DEBUG=1
```


## Terminal Font

MiniTelnet uses a separate font for terminal drawing so the GUI labels, string gadgets, and buttons stay on the normal Workbench/window font. The default terminal font is:

```text
ruby.font size 8
```

The terminal font is opened with OS1.3-safe font APIs: MiniTelnet tries `OpenFont()` first and then `diskfont.library`/`OpenDiskFont()` for fonts found in `FONTS:`. Fonts are released with `CloseFont()`. If the requested font cannot be opened, MiniTelnet falls back to the current window font and continues running. The terminal grid uses the selected terminal font metrics (`tf_XSize`, `tf_YSize`, and `tf_Baseline`) when calculating columns, rows, cursor placement, and window resize behavior.

Runtime selection is available from `Settings -> Terminal Font...`. MiniTelnet opens a small OS1.3 Intuition list requester, scans `FONTS:` for entries ending in `.font`, and shows a font list plus an available size list. Sizes are read from the matching bitmap font drawer, for example `FONTS:ruby/8` for `ruby.font` or `FONTS:topaz/11` for `topaz.font`. `OK` applies the highlighted font and size to the terminal area for the current session; `Cancel` and the close gadget discard the selection. The host/port fields, buttons, menu text, and status line keep using the normal window font.

If `ruby.font` size 8 cannot be opened, MiniTelnet tries `topaz.font` size 11, then falls back to the current window font. If a selected font/size cannot be opened, the status line reports `Font not available` and the previous terminal font remains active. Font selection can be saved to `minitelnet.conf` in v0.12. This is a simple OS1.3 list requester, not a modern dropdown. Choose a monospaced font for ANSI/BBS output.

Build-time overrides are available:

```sh
make DCTELNET13_TERM_FONT_NAME=topaz.font DCTELNET13_TERM_FONT_SIZE=11
```

ANSI art readability depends on both the font metrics and the approximate CP437 mapping. The internal renderer is still monochrome and does not provide a full IBM VGA font. Optional XEM output is available separately through `Settings -> XEM IBM Terminal`.
