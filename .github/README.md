# terminal-kitty-graphics

Windows Terminal Preview with enough of the [kitty graphics protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/)
for inline images in Claude Code. Not affiliated with or endorsed by Microsoft. Upstream is
[microsoft/terminal](https://github.com/microsoft/terminal); kitty graphics support there is tracked in
[#8389](https://github.com/microsoft/terminal/issues/8389).

The work lives on the `kitty-graphics` branch, rebased onto each Windows Terminal Preview release.
It adds:

- APC `G` commands: direct transmission (`t=d`) of PNG, RGB and RGBA, chunked with `m=1`
- `a=q` queries, `a=t` / `a=T` / `a=p` placements, `a=d` deletes
- Unicode placeholders (U+10EEEE, image id in the foreground colour, row and column in diacritics),
  which is how Claude Code draws images, plus direct placement at the cursor

File and shared-memory transmission, compression, animation and relative placement are not
supported; clients get an error reply and fall back to sending bytes. Images are stored in the same
per-row `ImageSlice` that Sixel uses, so text written over them erases them and they scroll and
reflow with the buffer. Inside ConPTY nothing changes: conhost passes the sequences through to the
terminal, which answers them.

The code is in `src/terminal/adapter/KittyGraphics.cpp`, with small changes to the parser
(`stateMachine.cpp`, which now collects APC strings) and `adaptDispatch.cpp`.

The branch also makes touch screens work in full-screen programs. Upstream, touch only scrolls the
terminal's own viewport, so a TUI that reads the mouse never sees a tap or a swipe. Here a tap is a
left click, a swipe sends wheel events when the program reads the mouse (and scrolls the viewport
otherwise), and a press held for 0.4 s before moving is a left-button drag that selects text
(`TermControl.cpp`, `_TouchPressed` and the two after it).

## Building

There are no released binaries. Build it yourself as a side-by-side "Terminal Dev" package, with
Visual Studio 2026 and the components in `.vsconfig`. `kitty/STATUS.md` covers the build, the
traps (never run MSBuild from a WSL shell), and the install. `kitty/update.ps1` rebases onto the
newest Preview, rebuilds and reinstalls; it is written for one machine and its paths, so read it
before running it.

Claude Code draws kitty images only for terminals it recognises, so set
`CLAUDE_CODE_FORCE_TERMINAL_IMAGES=1` in the profile's `environment`. Windows Terminal adds profile
variables to `WSLENV` itself.

MIT, like upstream.
