# PlotJugglerPro Local Build and Launch Notes

This repository has a local Windows PlotJugglerPro build with the current branch work enabled: native map panels, lazy-loading data work, master time-series column behavior, aligned Y axes, and the per-tab timeline slider.

The configured build directory is `build\PlotJugglerPro`.

## Launching

Run the installed executable from the repository root:

```powershell
.\install\bin\plotjuggler.exe
```

Full path:

```powershell
C:\Users\caleb\EV\_TREV4\github\PlotJugglerPro\install\bin\plotjuggler.exe
```

`QtWebEngineProcess.exe` must sit next to the installed app for map WebEngine support, but it is not launched directly.

## Rebuilding

Configure with the preset:

```powershell
cmake --preset windows-vs2022-pro
```

Or configure explicitly:

```powershell
cmake -S . -B build\PlotJugglerPro `
  -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DCMAKE_INSTALL_PREFIX="$PWD\install" `
  -DQt5_DIR="C:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5" `
  -DQt5WebEngineWidgets_DIR="C:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5WebEngineWidgets"
```

Fast compile check:

```powershell
cmake --build build\PlotJugglerPro --config Release --target plotjuggler
```

Build and install:

```powershell
cmake --build build\PlotJugglerPro --config Release --target install
```

Equivalent install preset:

```powershell
cmake --build --preset windows-vs2022-pro-release-install
```

If `install\bin\plotjuggler.exe` is running, `--target install` can fail with permission denied when it tries to overwrite the executable.

## Current Branch Features

- Native `Add Map View` split/dock panels.
- XY-only `Convert to Map panel`.
- Map layout state persisted in dock XML.
- Map marker updates during tracker movement and playback.
- Simple latitude/longitude autodetection using `Latitude` and `Longitude` keywords.
- Time-series tabs act as one master column with a single shared X viewport.
- Time-series navigation is x-only; Y ranges are refit automatically per plot.
- Time-series Y axes are aligned by `PlotDocker` across the vertical stack.
- Right-side panels are XY/map panels, not additional time-series columns.
- Timeline slider is per tab and follows the visible time viewport.
- Lazy-loading data work is present, including MF4-related loading paths.

## Map Tile Configuration

The app does not default to `tile.openstreetmap.org`, to avoid tile policy and 403 issues.
Set a tile provider before launching if the map needs online tiles:

```powershell
$env:PJ_MAP_TILES_URL="https://your.tile.server/{z}/{x}/{y}.png"
$env:PJ_MAP_ATTRIBUTION="Your attribution text"
.\install\bin\plotjuggler.exe
```

## Useful Files

- `plotjuggler_app/plot_docker.h`
- `plotjuggler_app/plot_docker.cpp`
- `plotjuggler_app/plotwidget.h`
- `plotjuggler_app/plotwidget.cpp`
- `plotjuggler_app/mainwindow.cpp`
- `plotjuggler_app/mainwindow.ui`
- `plotjuggler_app/realslider.h`
- `plotjuggler_app/map_dock_panel.h`
- `plotjuggler_app/map_dock_panel.cpp`
- `plotjuggler_app/CMakeLists.txt`
