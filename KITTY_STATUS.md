# Kitty graphics in Windows Terminal: fork status

Personal fork (GraysonCAdams/terminal), branch `kitty-graphics` from tag v1.26.2734.0 (Preview).
Goal: Claude Code's inline images, i.e. the kitty graphics subset it uses, nothing more.
Local only; never PR upstream (AGENTS.md bans AI-authored contributions).

## Scope (what Claude Code sends)

- APC `ESC _ G <keys> ; <base64> ESC \`, chunked with `m=1`.
- `a=q` probe (answer `OK`), `a=t`/`a=T`/`a=p` with `U=1` (virtual placement), `a=d` delete.
- `f=100` PNG (WIC decode), `f=32`/`f=24` raw. `t=d` only; `t=f/t/s` answered with an error so
  clients fall back to bytes. `o=z` unsupported (error).
- Unicode placeholders: U+10EEEE, image id in the foreground colour (24-bit or 256), row and
  column in diacritics (kitty's rowcolumn-diacritics table), inherited from the left cell when
  omitted.
- Direct placement (no `U`): painted at the cursor, cursor moved past it.

## Design

- Parser: `StateMachine` collects APC text (it ignored it before) and hands it to the engine on
  ESC; `OutputStateMachineEngine` -> `ITermDispatch::KittyGraphicsCommand`.
- `AdaptDispatch` owns a `KittyGraphics` (src/terminal/adapter). Inside ConPTY it does nothing:
  conhost forwards the raw APC to the terminal and must not answer.
- Placeholders: `PrintString` splits text at U+10EEEE, writes a space per placeholder, then paints
  that cell's tile into the row's `ImageSlice` (cell size 20x40 virtual px; the renderer scales).
  Text written over a cell already erases its image pixels, so TUI redraws stay correct.

## State

- [ ] VS Community 2026 installed from the tag's .vsconfig (winget, started 2026-10-30 13:26)
- [x] Developer Mode on (needed to register the dev package)
- [ ] Build of CascadiaPackage succeeds
- [x] Kitty code written (KittyGraphics.cpp/.hpp, parser APC collection, PrintString hook)
- [ ] Build with kitty code, install as "Terminal Dev"
- [ ] Test script (plain + placeholder) draws
- [ ] Claude Code + show-your-work draws (force flag in profile env)
