#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILD_DIR="$REPO_ROOT/build/PlotJugglerPro"
BUILD_OUTPUT="$BUILD_DIR/bin/Release"
DATA_DIR="$REPO_ROOT/installer/io.plotjuggler.application/data"
LICENSE_SRC="$REPO_ROOT/installer/io.plotjuggler.application/meta"

WINDEPLOYQT_WIN='C:\Qt\5.15.2\msvc2019_64\bin\windeployqt.exe'
WINDEPLOYQT_POSIX='/c/Qt/5.15.2/msvc2019_64/bin/windeployqt.exe'
BINARYCREATOR='/c/Qt/Tools/QtInstallerFramework/4.10/bin/binarycreator.exe'

die() {
  echo "error: $*" >&2
  exit 1
}

log() {
  printf '[%s] %s\n' "$(date '+%H:%M:%S')" "$*"
}

require_file() {
  local path="$1"
  [[ -f "$path" ]] || die "missing required file: $path"
}

require_payload_file() {
  local relative_path="$1"
  require_file "$DATA_DIR/$relative_path"
}

copy_qt_dll_from_qt_install() {
  local dll_name="$1"
  local source_path="/c/Qt/5.15.2/msvc2019_64/bin/$dll_name"
  require_file "$source_path"
  if [[ ! -f "$DATA_DIR/$dll_name" ]]; then
    log "Copying plugin Qt dependency from C:\\Qt\\5.15.2: $dll_name"
    cp -f "$source_path" "$DATA_DIR/$dll_name"
  fi
}

require_matching_qt_dll() {
  local dll_name="$1"
  local source_path="/c/Qt/5.15.2/msvc2019_64/bin/$dll_name"
  local staged_path="$DATA_DIR/$dll_name"
  require_file "$source_path"
  require_file "$staged_path"

  local source_size staged_size
  source_size="$(stat -c '%s' "$source_path")"
  staged_size="$(stat -c '%s' "$staged_path")"
  [[ "$source_size" == "$staged_size" ]] ||
    die "$dll_name does not match C:\\Qt\\5.15.2 copy. staged=$staged_size expected=$source_size"
}

cd "$REPO_ROOT"

log "Repository: $REPO_ROOT"
log "Build directory: $BUILD_DIR"
log "Build output: $BUILD_OUTPUT"
log "Installer payload: $DATA_DIR"
log "Using windeployqt: $WINDEPLOYQT_WIN"
log "Using binarycreator: $BINARYCREATOR"

require_file "$WINDEPLOYQT_POSIX"
require_file "$BINARYCREATOR"

raw_version="$(git describe --tags --always --dirty)"
release_id="$(
  printf '%s' "$raw_version" |
    sed -E 's#^v##; s#/#-#g; s#\.#-#g; s#[^A-Za-z0-9_-]#-#g; s#-+#-#g; s#^-##; s#-$##'
)"
metadata_version="$(
  printf '%s' "$raw_version" |
    sed -E 's#^v##' |
    grep -Eo '^[0-9]+([.][0-9]+)*' || true
)"
release_date="$(date +%Y-%m-%d)"

[[ -n "$release_id" ]] || die "git describe produced an empty release id"
[[ -n "$metadata_version" ]] || metadata_version="0.0.0"

installer_name="PlotJugglerPro-${release_id}-Windows-x64.exe"
installer_path="$REPO_ROOT/$installer_name"

log "Git describe: $raw_version"
log "Installer filename: $installer_name"
log "Installer metadata version: $metadata_version"

log "Building PlotJugglerPro Release target..."
cmake --build "$BUILD_DIR" --config Release --target plotjuggler
log "Build finished"

require_file "$BUILD_OUTPUT/plotjuggler.exe"

log "Clearing old installer payload..."
rm -rf "$DATA_DIR"
mkdir -p "$DATA_DIR"
log "Copying Release output into installer payload..."
cp -a "$BUILD_OUTPUT"/. "$DATA_DIR"/
log "Copied $(find "$DATA_DIR" -type f | wc -l | tr -d ' ') files before cleanup"

mapfile -t build_qt_dlls < <(find "$BUILD_OUTPUT" -maxdepth 1 -type f -iname 'Qt5*.dll' -printf '%f\n' | sort)
if ((${#build_qt_dlls[@]} > 0)); then
  log "Release output Qt DLLs to redeploy from C:\\Qt\\5.15.2: ${build_qt_dlls[*]}"
else
  log "No Qt DLLs found in Release output before cleanup"
fi

log "Removing stale version-named executable copies..."
rm -f "$DATA_DIR"/plotjuggler-*.exe
log "Renaming installed executable to PlotJugglerPro.exe..."
mv -f "$DATA_DIR/plotjuggler.exe" "$DATA_DIR/PlotJugglerPro.exe"

log "Removing pre-existing Qt runtime files before windeployqt..."
rm -f "$DATA_DIR"/Qt5*.dll
rm -f "$DATA_DIR"/QtWebEngineProcess.exe
rm -f "$DATA_DIR"/D3Dcompiler_*.dll "$DATA_DIR"/libEGL.dll "$DATA_DIR"/libGLESv2.dll "$DATA_DIR"/opengl32sw.dll
rm -rf "$DATA_DIR"/bearer "$DATA_DIR"/iconengines "$DATA_DIR"/imageformats "$DATA_DIR"/platforms
rm -rf "$DATA_DIR"/position "$DATA_DIR"/printsupport "$DATA_DIR"/resources "$DATA_DIR"/styles "$DATA_DIR"/translations
rm -rf "$DATA_DIR"/plugins/bearer "$DATA_DIR"/plugins/iconengines "$DATA_DIR"/plugins/imageformats
rm -rf "$DATA_DIR"/plugins/platforms "$DATA_DIR"/plugins/position "$DATA_DIR"/plugins/printsupport
rm -rf "$DATA_DIR"/plugins/resources "$DATA_DIR"/plugins/styles "$DATA_DIR"/plugins/translations

log "Copying license files..."
mkdir -p "$DATA_DIR/licenses"
cp -f "$LICENSE_SRC"/license_*.txt "$DATA_DIR/licenses"/
log "Payload now has $(find "$DATA_DIR" -type f | wc -l | tr -d ' ') files"

log "Updating installer metadata..."
metadata_script="
\$config = [xml](Get-Content 'installer/config.xml');
\$config.Installer.Version = '$metadata_version';
\$config.Save((Resolve-Path 'installer/config.xml'));
\$package = [xml](Get-Content 'installer/io.plotjuggler.application/meta/package.xml');
\$package.Package.Version = '$metadata_version';
\$package.Package.ReleaseDate = '$release_date';
\$package.Save((Resolve-Path 'installer/io.plotjuggler.application/meta/package.xml'));
"
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$metadata_script"
log "Installer metadata updated"

log "Deploying Qt runtime files. This can take a minute..."
cmd.exe //c installer\\windeploy_pj.bat "$WINDEPLOYQT_WIN"
log "Qt runtime deployment finished"

if ((${#build_qt_dlls[@]} > 0)); then
  log "Restoring Qt DLLs needed by PlotJuggler plugins from C:\\Qt\\5.15.2..."
  for dll_name in "${build_qt_dlls[@]}"; do
    copy_qt_dll_from_qt_install "$dll_name"
  done
fi

log "Validating staged runtime files..."
require_payload_file "PlotJugglerPro.exe"
require_payload_file "platforms/qwindows.dll"
require_payload_file "Qt5Core.dll"
require_payload_file "Qt5Widgets.dll"
require_payload_file "Qt5Svg.dll"
require_payload_file "Qt5WebSockets.dll"
require_payload_file "QtWebEngineProcess.exe"
require_payload_file "Qt5WebEngineCore.dll"
require_matching_qt_dll "Qt5Core.dll"
require_matching_qt_dll "Qt5Gui.dll"
require_matching_qt_dll "Qt5Quick.dll"
require_matching_qt_dll "Qt5WebEngineCore.dll"
log "Runtime validation passed"

log "Creating offline installer. This can take a minute..."
rm -f "$installer_path"
"$BINARYCREATOR" --offline-only -c installer/config.xml -p installer "$installer_path"
log "binarycreator finished"

if [[ -f "$installer_path" ]]; then
  installer_size="$(du -h "$installer_path" | awk '{print $1}')"
  log "Done: $installer_path ($installer_size)"
else
  die "binarycreator completed, but installer was not found: $installer_path"
fi
