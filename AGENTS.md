PlotJugglerPro Branch Notes

Current Direction

- This branch is no longer just a map extension. It is a local PlotJugglerPro workflow branch with:
  - native dock/split map panels,
  - lazy-loading data support, including MF4 work,
  - x-only navigation for time-series plots,
  - auto-Y refit behavior,
  - one master time-series column per tab,
  - deterministic Y-axis/canvas alignment across stacked time-series plots,
  - a per-tab timeline slider aligned under the time-series canvas.
- Prefer simplifying old optional sync behavior instead of preserving compatibility. This is beta development.
- Reworks, refactors, and deletions are encouraged when they remove stale behavior or make the workflow clearer.

Time-Series Layout Model

- A `PlotDocker` tab owns one master time viewport for time-series plots.
- Non-XY time-series plots in the same tab must always share the same X range.
- Time-series plots are added vertically into the master column.
- Right-side panels are for XY plots or map views only.
- Adding a time-series signal preserves the tab X viewport and recalculates that plot's Y range.
- The first time-series signal in a fresh tab initializes the tab viewport from the full data range.
- Y-axis alignment is handled by `PlotDocker`, not by individual plots:
  - reset axis extents,
  - update/measure labels,
  - apply the max left/right Y extent to the vertical time-series stack,
  - use a minimum left-axis extent so short labels do not collapse the canvas.
- The old zoom link button and linked-zoom fanout were removed. Do not reintroduce `buttonLink`, `linkedZoomOut()`, or per-plot linked-zoom opt-outs.

Timeline and Tracker

- The timeline slider is now per `PlotDocker` tab, not the old global main-window slider.
- `MainWindow` remains the owner of absolute tracker/playback time.
- `PlotDocker` stores the time viewport in plot-relative coordinates, but exposes slider/playback bounds in absolute time by adding the plot time offset.
- Tracker movement, playback, and the tab timeline slider must stay in sync.
- After adding signals or changing layout/axis geometry, refresh shared time axes and tracker position so the vertical "now" line does not wait for a zoom/pan to correct itself.

Map Panel

- Map is a native dock/split panel in the main plot layout, not a toolbox window.
- Plot context menu includes `Add Map View` and XY-only `Convert to Map panel`.
- Map panel state, including latitude/longitude selections, is persisted in dock XML and restored with layouts.
- Map receives tracker/playback time updates and moves the marker in sync.
- Latitude/longitude autodetection is intentionally simple: keyword match for `Latitude` and `Longitude` only.
- `Fit to View` refreshes data, runs detection, and fits the route.
- Right-click inside the map is forwarded to app code; WebEngine's default context menu is suppressed.
- Map right-click menu includes:
  - `Fit to View` (`zoom_max.svg`)
  - `Split Horizontally` (`add_column.svg`)
  - `Split Vertically` (`add_row.svg`)
  - `Add Map View` (`scatter.svg`)

Build and Launch

- See `PLOTJUGGLER_PRO_BUILD.md` for local Windows build and launch notes.
- The configured build directory is `build\PlotJugglerPro`.
- Fast compile check:

```powershell
cmake --build build\PlotJugglerPro --config Release --target plotjuggler
```

- Full installed runnable layout:

```powershell
cmake --build build\PlotJugglerPro --config Release --target install
```

- If `install\bin\plotjuggler.exe` is running, the install target can fail with permission denied while copying the exe.

Key Files

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

Map Tile Notes

- WebEngine remains optional through `PJ_HAS_WEBENGINE`; the map panel shows informational fallback text when unavailable.
- Tile provider is configured with:
  - `PJ_MAP_TILES_URL`
  - `PJ_MAP_ATTRIBUTION`
- Avoid defaulting app code to `tile.openstreetmap.org`; it can violate policy or return 403s.
