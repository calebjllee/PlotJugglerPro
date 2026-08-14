# PlotJugglerPro Windows Release Notes

This document records the local Windows packaging flow used to create repeatable installers for PlotJugglerPro.

The goal is not a mass-public release pipeline. The goal is a practical standalone Windows installer that can be sent out after bug fixes and installed by users with the normal uninstall/reinstall flow.

## Recommended Path

For the current Windows release process, build and package locally on the Windows development machine.

GitHub Actions is useful later for repeatable public releases, but local packaging is simpler while the user group is small and releases are still being troubleshot directly.

## One-Command Release

From PowerShell, run:

```powershell
.\tools\package_windows_release.ps1
```

Or from Git Bash, run:

```bash
bash ./tools/package_windows_release.sh
```

The script:

1. Builds the Release `plotjuggler` target.
2. Stages `build\PlotJugglerPro\bin\Release` into the installer payload.
3. Removes stale version-named app executables such as `plotjuggler-3.15.exe`.
4. Renames the installed app executable to `PlotJugglerPro.exe`.
5. Records which Qt DLLs were present in the Release output for plugin dependencies.
6. Removes pre-existing Qt runtime DLLs/folders from the payload so the package does not mix incompatible Qt builds.
7. Copies license text files into the installed `licenses` folder.
8. Runs `windeployqt` using:

```powershell
C:\Qt\5.15.2\msvc2019_64\bin\windeployqt.exe
```

9. Restores plugin-needed Qt DLLs, such as `Qt5WebSockets.dll`, from the same `C:\Qt\5.15.2` install.
10. Runs Qt Installer Framework using:

```powershell
C:\Qt\Tools\QtInstallerFramework\4.10\bin\binarycreator.exe
```

The script also validates key runtime files after `windeployqt`, including `platforms\qwindows.dll`, `QtWebEngineProcess.exe`, and `Qt5WebEngineCore.dll`. It checks key Qt DLLs against the `C:\Qt\5.15.2` copies to catch mixed Qt runtime packages.

The installer filename comes from:

```bash
git describe --tags --always --dirty
```

Dots are changed to hyphens. For example:

```text
2026.8.14 -> PlotJugglerPro-2026-8-14-Windows-x64.exe
```

If the working tree is dirty, Git may add `-dirty` to the generated filename.

The manual steps below are kept for troubleshooting or for rebuilding the process by hand.

## Build Location

The configured local build directory is:

```powershell
build\PlotJugglerPro
```

The Release binaries used for packaging are expected at:

```powershell
build\PlotJugglerPro\bin\Release
```

The local installed runnable app, created by the CMake `install` target, is:

```powershell
install\bin\plotjuggler.exe
```

That installed executable is useful for local testing, but the artifact to distribute is the offline installer created with Qt Installer Framework.

## 1. Close PlotJuggler

Before rebuilding or reinstalling, close any running copy of:

```powershell
install\bin\plotjuggler.exe
```

If it is running, the install target can fail while copying the executable.

## 2. Build Release

From the repository root:

```powershell
cmake --build build\PlotJugglerPro --config Release --target plotjuggler
```

Optionally refresh the local installed copy too:

```powershell
cmake --build build\PlotJugglerPro --config Release --target install
```

## 3. Stage Installer Files

From the repository root:

```powershell
Remove-Item -Recurse -Force installer\io.plotjuggler.application\data -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path installer\io.plotjuggler.application\data
Copy-Item -Path build\PlotJugglerPro\bin\Release\* -Destination installer\io.plotjuggler.application\data -Recurse -Force
Remove-Item installer\io.plotjuggler.application\data\plotjuggler-*.exe -Force -ErrorAction SilentlyContinue
Move-Item installer\io.plotjuggler.application\data\plotjuggler.exe installer\io.plotjuggler.application\data\PlotJugglerPro.exe -Force
New-Item -ItemType Directory -Force -Path installer\io.plotjuggler.application\data\licenses
Copy-Item -Path installer\io.plotjuggler.application\meta\license_*.txt -Destination installer\io.plotjuggler.application\data\licenses -Force
```

This copies the built app, plugins, and dependency DLLs into the Qt Installer Framework package data directory.
It removes stale version-named executable copies, such as `plotjuggler-3.15.exe`, and renames the installed app executable to `PlotJugglerPro.exe`.
It also copies the license texts into the installed app folder without showing a blocking license agreement page during install.

## 4. Deploy Qt Runtime Files

Run this from the repository root, not from the Release folder:

```powershell
cmd /c installer\windeploy_pj.bat
```

If `windeployqt.exe` is not on `PATH`, pass the full Qt path:

```powershell
cmd /c installer\windeploy_pj.bat C:\Qt\5.15.2\msvc2019_64\bin\windeployqt.exe
```

The script runs `windeployqt` on `PlotJugglerPro.exe` in:

```powershell
installer\io.plotjuggler.application\data
```

This pulls in Qt DLLs, plugins, WebEngine files, and related runtime assets needed on users' machines.

## 5. Create Offline Installer

Find `binarycreator.exe`, usually somewhere under:

```powershell
C:\Qt\Tools\QtInstallerFramework
```

Search for it if needed:

```powershell
Get-ChildItem C:\Qt -Recurse -Filter binarycreator.exe | Select-Object -First 1 FullName
```

Then run, from the repository root:

```powershell
& "C:\Qt\Tools\QtInstallerFramework\4.10\bin\binarycreator.exe" --offline-only -c installer\config.xml -p installer PlotJugglerPro-YYYY-MM-DD-Windows-x64.exe
```

Adjust the `binarycreator.exe` path if your Qt Installer Framework version is different.

The final argument is only the output installer filename. It does not change the installed app name, install directory, Start Menu entry, version, or publisher metadata.

The installed application executable is staged as:

```powershell
C:\Program Files\PlotJugglerPro\PlotJugglerPro.exe
```

If run from the repository root, the output installer is created at:

```powershell
PlotJugglerPro-YYYY-MM-DD-Windows-x64.exe
```

Use any clear release identifier in the filename, such as a date, build number, or short version:

```powershell
PlotJugglerPro-2026-08-14-Windows-x64.exe
PlotJugglerPro-v1.0.3-Windows-x64.exe
PlotJugglerPro-fix-map-tracker-Windows-x64.exe
```

## Installer Metadata

The default install folder, installer title, Start Menu folder, and related visible names are controlled by:

```powershell
installer\config.xml
installer\io.plotjuggler.application\meta\package.xml
```

The installer also uses a plain light stylesheet to avoid unreadable white-on-white text on some Windows/Qt theme combinations:

```powershell
installer\installer.qss
```

For this branch, the installer defaults to:

```powershell
C:\Program Files\PlotJugglerPro
```

The key setting is:

```xml
<TargetDir>@ApplicationsDirX64@/PlotJugglerPro</TargetDir>
```

The installer creates shortcuts for:

```powershell
Start Menu\PlotJugglerPro\PlotJugglerPro.lnk
Desktop\PlotJugglerPro.lnk
```

The installer does not show a blocking license agreement page. The license text files are copied into:

```powershell
C:\Program Files\PlotJugglerPro\licenses
```

## What Gets Distributed

Send users the generated offline installer:

```powershell
PlotJugglerPro-YYYY-MM-DD-Windows-x64.exe
```

Do not send only:

```powershell
install\bin\plotjuggler.exe
```

The raw executable may work on the development machine because Qt, vcpkg, and build paths are already present. The installer is intended to carry the runtime files users need.

## Quick Smoke Test

Before sending the installer widely:

1. Run the installer.
2. Confirm the default path is `C:\Program Files\PlotJugglerPro`.
3. Launch PlotJugglerPro from the installed location.
4. Load a normal data file.
5. Try the branch-specific features users care about, especially map panels, MF4 loading, timeline slider behavior, tracker sync, and stacked time-series axis alignment.
6. Uninstall it from Windows Apps & Features.
7. Reinstall the same installer or the next generated installer.
8. Confirm the app launches after reinstall.

If possible, test on a second Windows machine or a clean VM.

## User Update Flow

For each bug-fix release:

1. Build the Release target.
2. Re-stage `installer\io.plotjuggler.application\data`.
3. Run `installer\windeploy_pj.bat`.
4. Run `binarycreator.exe` with a new output filename.
5. Ask users to uninstall their existing PlotJugglerPro from Windows Apps & Features.
6. Ask users to run the new installer.

Keeping one install directory, `C:\Program Files\PlotJugglerPro`, makes troubleshooting simpler.
