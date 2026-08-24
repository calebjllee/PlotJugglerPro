# PlotJugglerPro Agent Notes

## Operating Rules

- Do not use the Windows sandbox. It fails on this workspace and wastes time.
  Run needed commands with escalation when the sandbox runner fails.
- This is a beta workflow branch. Prefer deleting stale behavior and stale docs
  over preserving upstream compatibility by default.
- Keep changes focused, but do not protect cruft. If a thing is not part of the
  current Windows workflow or planned macOS workflow, call it out or remove it.
- Do not treat `plotjuggler_base` as legacy. It is the core library used by the
  Pro app and plugins.

## Current Product Direction

PlotJugglerPro is a local data-review workflow fork with:

- native dock/split map panels,
- lazy-loading data support, including MF4 work,
- x-only navigation for time-series plots,
- auto-Y refit behavior,
- one master time-series column per tab,
- deterministic Y-axis/canvas alignment across stacked time-series plots,
- a per-tab timeline slider aligned under the time-series canvas.

## Supported Build Path

The verified build is local Windows:

```powershell
cmake --preset windows-vs2022-pro
cmake --build build\PlotJugglerPro --config Release --target install
.\install\bin\plotjuggler.exe
```

Fast compile check:

```powershell
cmake --build build\PlotJugglerPro --config Release --target plotjuggler
```

Installer:

```powershell
.\tools\package_windows_release.ps1
```

If MSBuild emits repeated red lines from
`vcpkg\scripts\buildsystems\msbuild\applocal.ps1`, re-run
`cmake --preset windows-vs2022-pro`. The preset pins vcpkg's app-local copy step
to stable Windows PowerShell so Microsoft Store `pwsh.exe` version changes do
not break generated projects.

Do not use inherited Snap, AppImage, Nix, Docker, ROS package, or Conan build
instructions unless the task is explicitly to revive that platform.

## Architecture

- `plotjuggler_base`: core data model, plugin API, plot widget base, Qwt helpers,
  transforms, serializers.
- `plotjuggler_app`: GUI executable and PlotJugglerPro workflow logic.
- `plotjuggler_plugins`: loaders/parsers/tools. Keep plugins that support the
  workflow; prune unused upstream plugins deliberately.
- `installer` and `tools/package_windows_release.*`: current Windows release path.

## Time-Series Layout Model

- A `PlotDocker` tab owns one master time viewport for time-series plots.
- Non-XY time-series plots in the same tab must always share the same X range.
- Time-series plots are added vertically into the master column.
- Right-side panels are for XY plots or map views only.
- Adding a time-series signal preserves the tab X viewport and recalculates that
  plot's Y range.
- The first time-series signal in a fresh tab initializes the tab viewport from
  the full data range.
- Y-axis alignment is handled by `PlotDocker`, not by individual plots.
- The old zoom link button and linked-zoom fanout were removed. Do not
  reintroduce `buttonLink`, `linkedZoomOut()`, or per-plot linked-zoom opt-outs.

## Timeline and Tracker

- The timeline slider is per `PlotDocker` tab, not the old global main-window
  slider.
- `MainWindow` owns absolute tracker/playback time.
- `PlotDocker` stores the time viewport in plot-relative coordinates and exposes
  slider/playback bounds in absolute time by adding the plot time offset.
- Tracker movement, playback, and the tab timeline slider must stay in sync.
- After adding signals or changing layout/axis geometry, refresh shared time axes
  and tracker position so the vertical now line updates immediately.

## Map Panel

- Map is a native dock/split panel in the main plot layout, not a toolbox window.
- Plot context menu includes `Add Map View` and XY-only `Convert to Map panel`.
- Map panel state, including latitude/longitude selections, is persisted in dock
  XML and restored with layouts.
- Map receives tracker/playback time updates and moves the marker in sync.
- Latitude/longitude autodetection is intentionally simple: keyword match for
  `Latitude` and `Longitude` only.
- `Fit to View` refreshes data, runs detection, and fits the route.
- Right-click inside the map is forwarded to app code; WebEngine's default
  context menu is suppressed.
- Do not default app code to `tile.openstreetmap.org`; configure tiles with
  `PJ_MAP_TILES_URL` and `PJ_MAP_ATTRIBUTION`.

## Key Files

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
- `plotjuggler_base/src/plotwidget_base.cpp`
