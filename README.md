# PlotJugglerPro

PlotJugglerPro is a local workflow fork of PlotJuggler for reviewing vehicle
data. It keeps the useful PlotJuggler plugin/data model, but the product
direction is narrower: fast local file review, native map panels, lazy-loaded
large data, and a cleaner time-series layout model.

This is not intended to track every upstream distribution path. Unsupported
build and packaging systems should be removed or ignored instead of kept alive
as museum pieces.

## Current Workflow

- Native dock/split map panels in the main plot layout
- Lazy-loading data support, including MF4 work
- One master time-series column per tab
- Shared X viewport for all non-XY time-series plots in a tab
- X-only time-series navigation with automatic Y refit
- Deterministic Y-axis/canvas alignment across stacked time-series plots
- Per-tab timeline slider aligned under the time-series canvas
- Tracker/playback/map marker synchronization

## Build

The verified build path is local Windows:

```powershell
cmake --preset windows-vs2022-pro
cmake --build build\PlotJugglerPro --config Release --target install
.\install\bin\plotjuggler.exe
```

The release installer path is:

```powershell
.\tools\package_windows_release.ps1
```

See:

- `PLOTJUGGLER_PRO_BUILD.md`
- `PLOTJUGGLER_PRO_WINDOWS_RELEASE.md`
- `COMPILE.md`

## Architecture

- `plotjuggler_base`: core library used by the app and plugins. Despite the
  upstream name, this is not dead code.
- `plotjuggler_app`: GUI executable and Pro workflow code.
- `plotjuggler_plugins`: loaders, streamers, parsers, and tools. Keep the
  plugins used by the workflow; prune the rest deliberately.
- `installer` and `tools/package_windows_release.*`: current Windows packaging.

## Platform Policy

Supported now:

- Windows local build and installer

Planned:

- Native macOS app/DMG

Not maintained for this branch:

- Snap
- AppImage
- Nix
- Docker/AppImage build containers
- ROS package builds
- Conan workflows
- Public upstream release automation

## License

The original PlotJuggler code is MPL-2.0. Keep upstream license and attribution
files unless there is a deliberate legal review.
