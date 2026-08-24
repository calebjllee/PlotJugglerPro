# PlotJugglerPro Build Matrix

This branch supports one verified build path today:

- Windows 11
- Visual Studio 2022
- CMake preset `windows-vs2022-pro`
- vcpkg at `C:\vcpkg`
- Qt 5.15.2 at `C:\Qt\5.15.2\msvc2019_64`

The next intended platform is native macOS. Linux, Snap, AppImage, Nix, Docker,
ROS package builds, and Conan workflows are upstream inheritance and are not
currently maintained for PlotJugglerPro.

## Windows

Configure:

```powershell
cmake --preset windows-vs2022-pro
```

Build a runnable local install:

```powershell
cmake --build build\PlotJugglerPro --config Release --target install
```

Run:

```powershell
.\install\bin\plotjuggler.exe
```

Fast compile check:

```powershell
cmake --build build\PlotJugglerPro --config Release --target plotjuggler
```

Installer packaging:

```powershell
.\tools\package_windows_release.ps1
```

See `PLOTJUGGLER_PRO_BUILD.md` and `PLOTJUGGLER_PRO_WINDOWS_RELEASE.md` for the
current local workflow.

## macOS

macOS is planned but not yet verified for this Pro branch. The likely path is a
native CMake preset using Homebrew `qt@5`, followed by `macdeployqt` and DMG
packaging. Treat existing upstream macOS CI or Conan references as stale until
they are rebuilt around the Pro workflow.

## What Not To Use

Do not use old upstream commands that clone `facontidavide/PlotJuggler`, build in
`build/PlotJuggler`, or package Snap/AppImage/Docker artifacts. They do not
describe this repository's current build.
