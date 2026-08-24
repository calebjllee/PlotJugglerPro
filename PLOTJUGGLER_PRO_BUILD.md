# PlotJugglerPro Local Build

This document describes the verified local Windows workflow. Other inherited
upstream build paths are not maintained for this branch.

## Requirements

- Visual Studio 2022
- CMake
- vcpkg at `C:\vcpkg`
- Qt 5.15.2 at `C:\Qt\5.15.2\msvc2019_64`
- Qt Installer Framework only when creating an installer

## Configure

```powershell
cmake --preset windows-vs2022-pro
```

The preset writes to:

```powershell
build\PlotJugglerPro
```

## Build

Fast compile check:

```powershell
cmake --build build\PlotJugglerPro --config Release --target plotjuggler
```

Build the runnable local install:

```powershell
cmake --build build\PlotJugglerPro --config Release --target install
```

Equivalent preset:

```powershell
cmake --build --preset windows-vs2022-pro-release-install
```

## Launch

```powershell
.\install\bin\plotjuggler.exe
```

`QtWebEngineProcess.exe` must sit beside the installed app for WebEngine map
support, but it is not launched directly.

## Troubleshooting

If `install\bin\plotjuggler.exe` is running, the install target can fail because
Windows will not overwrite the executable. Close PlotJuggler and rebuild.

If MSBuild prints repeated red lines from
`vcpkg\scripts\buildsystems\msbuild\applocal.ps1` with `The system cannot find
the path specified`, reconfigure with:

```powershell
cmake --preset windows-vs2022-pro
```

That failure usually means vcpkg cached a stale Microsoft Store `pwsh.exe`
version path. The Windows preset pins vcpkg app-local copying to stable Windows
PowerShell.

## Map Tiles

The app does not default to `tile.openstreetmap.org`. Set a provider before
launching if online map tiles are needed:

```powershell
$env:PJ_MAP_TILES_URL="https://your.tile.server/{z}/{x}/{y}.png"
$env:PJ_MAP_ATTRIBUTION="Your attribution text"
.\install\bin\plotjuggler.exe
```

## Current Feature Surface

- Native `Add Map View` split/dock panels
- XY-only `Convert to Map panel`
- Map layout state persisted in dock XML
- Map marker updates during tracker movement and playback
- Simple latitude/longitude autodetection using `Latitude` and `Longitude`
- One master time-series column per tab
- Shared X viewport across non-XY time-series plots in a tab
- X-only time-series navigation with automatic Y refit
- `PlotDocker`-owned Y-axis/canvas alignment
- Per-tab timeline slider following the visible time viewport
- Lazy-loading data work, including MF4 paths
