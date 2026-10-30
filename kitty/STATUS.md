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

- [x] VS Community 2026 installed from the tag's .vsconfig
- [x] Developer Mode on (needed to register the dev package)
- [x] Kitty code written (KittyGraphics.cpp/.hpp, parser APC collection, PrintString hook)
- [x] Build with kitty code, install as "Terminal Dev" (2026-10-30)
- [x] Test script (plain + placeholder) draws
- [x] Claude Code + show-your-work draws (force flag in profile env)
- [ ] Terminal Dev as the default terminal, sessions moved over
- [x] Scheduled rebase-and-rebuild on new Preview tags (kitty/update.ps1, task "WT Kitty Update", Sundays 3 AM)

## Building and updating

kitty/update.ps1, run weekly by the scheduled task "WT Kitty Update" under C:\src\tools\pwsh
(portable PowerShell 7.6.9): rebases onto the newest Preview release, builds, pushes, installs, and
toasts. `-Force` rebuilds without a new release. A loose package registration cannot be deferred
(`-DeferRegistrationWhenPackagesAreInUse` is not in the `-Register` parameter set), so while Terminal
Dev runs the build is left pending in C:\src\wt-dev\PENDING and the task "WT Kitty Install" runs
`update.ps1 -InstallPending` every 30 minutes until Terminal Dev is closed.
Not from WSL: WSL passes both PATH and Path, and MSBuild dies on the duplicate
(MSB6001). Not under the Store pwsh: build steps it starts cannot find pwsh.exe. Each build is
unpacked to C:\src\wt-dev\<tag>-<time> (three kept) and registered from there (Developer Mode). The
first build was registered from C:\src\wt-dev-loose by hand.

## Settings

Terminal Dev keeps its own settings (Packages\WindowsTerminalDev_8wekyb3d8bbwe\LocalState). They are
the Preview settings plus `profiles.defaults.environment.CLAUDE_CODE_FORCE_TERMINAL_IMAGES = "1"`.
WT itself adds profile environment names to WSLENV; setting WSLENV there drops WT_SESSION.
