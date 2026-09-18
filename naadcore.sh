#!/usr/bin/env bash
# naadcore.sh — interactive NaadCore launcher:
#   1) build the CLI and plugins (if needed)
#   2) detect ALSA MIDI sources and let the user pick one
#   3) detect built plugins and let the user pick one
#   4) pick an audio driver, then launch naadcore-cli
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CLI="$ROOT/build/apps/naadcore-cli/naadcore-cli"
PLUGINS_DIR="$ROOT/build/plugins"
DEFAULT_SOUNDFONT="/home/nil/harmonium-companion/harmonium.sf2"

C_G=$'\033[0;32m'; C_Y=$'\033[0;33m'; C_R=$'\033[0;31m'; C_B=$'\033[1;34m'; C_0=$'\033[0m'
msg()  { printf '%s==>%s %s\n'  "$C_B" "$C_0" "$*"; }
ok()   { printf '%s  ok %s %s\n' "$C_G" "$C_0" "$*"; }
warn() { printf '%s  ! %s %s\n' "$C_Y" "$C_0" "$*"; }
err()  { printf '%s  x %s %s\n' "$C_R" "$C_0" "$*" >&2; }
die()  { err "$*"; exit 1; }

banner() {
cat <<EOF
${C_B}+-------------------------------------------+
|   NaadCore - interactive launcher         |
|   build -> MIDI -> plugin -> play         |
+-------------------------------------------+${C_0}
EOF
}

# ---------------------------------------------------------------- build
build_step() {
  if [[ -x "$CLI" ]]; then
    read -rp "CLI already built. Rebuild? [y/N] " a
    case "$a" in Y|y) ;; *) ok "Using existing build."; return 0 ;; esac
  else
    msg "CLI not built yet - building."
  fi
  for dep in cmake g++ pkg-config aconnect; do
    command -v "$dep" >/dev/null 2>&1 || die "Missing dependency: $dep (sudo apt install build-essential cmake pkg-config alsa-utils)"
  done
  pkg-config --exists fluidsynth 2>/dev/null || die "Missing libfluidsynth-dev (sudo apt install libfluidsynth-dev)"
  pkg-config --exists alsa 2>/dev/null || die "Missing libasound2-dev (sudo apt install libasound2-dev)"
  msg "Configuring (cmake -B build)..."
  cmake -B "$ROOT/build" >/dev/null || die "cmake configure failed"
  msg "Building with $(nproc) jobs..."
  cmake --build "$ROOT/build" -j"$(nproc)" || die "Build failed"
  [[ -x "$CLI" ]] || die "CLI binary not found after build: $CLI"
  ok "Build complete."
}

# ---------------------------------------------------------------- MIDI
scan_midi() {
  MIDI_ADDRS=(); MIDI_LABELS=()
  local cnum="" cname="" line pnum pname
  while IFS= read -r line; do
    if [[ $line =~ ^client\ ([0-9]+):\ \'([^\']*)\'(.*) ]]; then
      cnum="${BASH_REMATCH[1]}"
      cname="${BASH_REMATCH[2]}"
      [[ "$cnum" == "0" ]] && cnum=""    # skip System (Timer/Announce)
      continue
    fi
    [[ -z "$cnum" ]] && continue
    if [[ $line =~ ^[[:space:]]*([0-9]+)\ \'([^\']*)\' ]]; then
      pnum="${BASH_REMATCH[1]}"
      pname="${BASH_REMATCH[2]}"
      MIDI_ADDRS+=("$cnum:$pnum")
      MIDI_LABELS+=("$(printf '%-9s %-16s %s' "$cnum:$pnum" "$cname" "$pname")")
    fi
  done < <(aconnect -o 2>/dev/null)
}

choose_midi() {
  while true; do
    msg "Scanning ALSA MIDI sources (aconnect -o)..."
    scan_midi
    if ((${#MIDI_ADDRS[@]})); then
      PS3=$'\nSelect MIDI input source: '
      select sel in "${MIDI_LABELS[@]}" "Run without MIDI input"; do
        [[ -z "$sel" ]] && { warn "Invalid choice."; continue; }
        if (( REPLY == ${#MIDI_ADDRS[@]} + 1 )); then
          MIDI_ADDR=""
          ok "Running without MIDI input."
        else
          MIDI_ADDR="${MIDI_ADDRS[$((REPLY-1))]}"
          ok "MIDI source: $MIDI_ADDR"
        fi
        return 0
      done
    fi
    warn "No ALSA MIDI sources found (is the keyboard plugged in?)."
    if ! read -rp "Rescan? [Y/n] " a; then
      MIDI_ADDR=""
      return 0
    fi
    case "$a" in N|n) MIDI_ADDR=""; return 0 ;; esac
  done
}

# ---------------------------------------------------------------- plugin
choose_plugin() {
  msg "Looking for plugins in build/plugins/..."
  local plugins=() f
  while IFS= read -r f; do plugins+=("$f"); done < <(find "$PLUGINS_DIR" -maxdepth 1 -name '*.so' 2>/dev/null | sort)
  if ((${#plugins[@]})); then
    local labels=()
    for f in "${plugins[@]}"; do labels+=("$(basename "$f")"); done
    PS3=$'\nSelect plugin to play: '
    select sel in "${labels[@]}" "Enter a custom path"; do
      [[ -z "$sel" ]] && { warn "Invalid choice."; continue; }
      if [[ "$sel" == "Enter a custom path" ]]; then
        read -rp "Plugin .so path: " PLUGIN_PATH
      else
        PLUGIN_PATH="${plugins[$((REPLY-1))]}"
      fi
      [[ -f "$PLUGIN_PATH" ]] || die "No such file: $PLUGIN_PATH"
      ok "Plugin: $PLUGIN_PATH"
      return 0
    done
  fi
  warn "No plugin .so found in $PLUGINS_DIR (build first? ./naadcore.sh rebuilds)."
  read -rp "Enter a plugin path manually (blank to abort): " PLUGIN_PATH
  [[ -n "$PLUGIN_PATH" && -f "$PLUGIN_PATH" ]] || die "No plugin selected."
  ok "Plugin: $PLUGIN_PATH"
}

# ---------------------------------------------------------------- driver
choose_driver() {
  PS3=$'\nSelect audio driver: '
  select sel in "alsa (default, PipeWire-proxied)" \
                "pulseaudio (PipeWire-proxied)" \
                "pipewire (native, may fail on this machine)"; do
    [[ -z "$sel" ]] && { warn "Invalid choice."; continue; }
    case "$REPLY" in
      1) AUDIO_DRIVER="alsa" ;;
      2) AUDIO_DRIVER="pulseaudio" ;;
      3) AUDIO_DRIVER="pipewire" ;;
    esac
    ok "Audio driver: $AUDIO_DRIVER"
    return 0
  done
  AUDIO_DRIVER="${AUDIO_DRIVER:-alsa}"   # EOF safety: default
}

# ---------------------------------------------------------------- main
main() {
  banner
  build_step
  choose_midi
  choose_plugin
  choose_driver
  [[ -n "${PLUGIN_PATH:-}" ]] || die "No plugin selected."
  AUDIO_DRIVER="${AUDIO_DRIVER:-alsa}"
  if [[ "$PLUGIN_PATH" == *harmonium* && ! -f "$DEFAULT_SOUNDFONT" ]]; then
    warn "SoundFont not found: $DEFAULT_SOUNDFONT"
    warn "The harmonium plugin may fail to start. Override at configure time:"
    warn "  cmake -B build -DHARMONIUM_SOUNDFONT_PATH=/path/to/harmonium.sf2"
  fi
  echo
  msg "Launching naadcore-cli (press Ctrl+C to quit)..."
  local args=(--plugin "$PLUGIN_PATH" --audio-driver "$AUDIO_DRIVER")
  [[ -n "${MIDI_ADDR:-}" ]] && args+=(--midi "$MIDI_ADDR")
  exec "$CLI" "${args[@]}"
}

main "$@"
