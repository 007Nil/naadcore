#!/usr/bin/env bash
# naadcore.sh — interactive NaadCore launcher:
#   1) build the CLI and plugins (if needed)
#   2) detect ALSA MIDI sources and let the user pick one
#   3) detect built plugins and let the user pick one
#   4) pick an audio driver
#   5) pick an audio output device (driver-dependent; skippable), then
#      launch naadcore-cli
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CLI="$ROOT/build/apps/naadcore-cli/naadcore-cli"
PLUGINS_DIR="$ROOT/build/plugins"
# The harmonium plugin's build-time default font (committed in-repo; the
# plugin is self-contained — no font outside the repository is needed).
DEFAULT_SOUNDFONT="$ROOT/plugins/harmonium/soundfonts/harmonium_v3.sf2"

C_G=$'\033[0;32m'; C_Y=$'\033[0;33m'; C_R=$'\033[0;31m'; C_B=$'\033[1;34m'; C_0=$'\033[0m'
msg()  { printf '%s==>%s %s\n'  "$C_B" "$C_0" "$*"; }
ok()   { printf '%s  ok %s %s\n' "$C_G" "$C_0" "$*"; }
warn() { printf '%s  ! %s %s\n' "$C_Y" "$C_0" "$*"; }
err()  { printf '%s  x %s %s\n' "$C_R" "$C_0" "$*" >&2; }
die()  { err "$*"; exit 1; }

banner() {
cat <<EOF
${C_B}+-------------------------------------------------------+
|   NaadCore - interactive launcher                     |
|   build -> MIDI -> plugin -> driver -> device -> play |
+-------------------------------------------------------+${C_0}
EOF
}

# ---------------------------------------------------------------- LFS
check_lfs_files() {
  # Some repo files (e.g. large SoundFonts) are tracked via git-lfs.
  # On a fresh clone without LFS initialised the checkout produces
  # ~140-byte pointer text files instead of the real binary.  Verify
  # each LFS-tracked file is a real binary; if not, install LFS and
  # pull the blobs so subsequent build/run steps see the correct data.
  local lfspatterns=()
  if [[ -f "$ROOT/.gitattributes" ]]; then
    while IFS= read -r p; do
      lfspatterns+=("$p")
    done < <(grep -E ' filter=lfs$' "$ROOT/.gitattributes" | awk '{print $1}')
  fi
  ((${#lfspatterns[@]})) || return 0   # nothing LFS-tracked

  local pointers=() real_files=()
  for pat in "${lfspatterns[@]}"; do
    local resolved
    # Resolve the .gitattributes glob to actual paths on disk
    local matches
    matches=($ROOT/$pat)
    [[ -e "${matches[0]}" ]] || continue
    for f in "${matches[@]}"; do
      if [[ -f "$f" ]]; then
        local fsize
        fsize=$(wc -c < "$f")
        if (( fsize < 500 )); then
          local header
          header=$(head -c 30 "$f")
          if [[ "$header" == "version https://git-lfs"* ]]; then
            pointers+=("$f")
          else
            real_files+=("$f")
          fi
        else
          real_files+=("$f")
        fi
      fi
    done
  done

  ((${#pointers[@]})) || return 0   # all LFS files are real

  # Need to pull LFS blobs
  if ! command -v git-lfs >/dev/null 2>&1; then
    die "git-lfs is not installed. Install it with: sudo apt install git-lfs && git lfs install"
  fi
  msg "Initialising git-lfs and pulling blob files..."
  git -C "$ROOT" lfs install >/dev/null 2>&1 || warn "git lfs install returned non-zero"
  git -C "$ROOT" lfs pull 2>/dev/null || die "git lfs pull failed — manual fix required:"
  warn "  cd $ROOT && git lfs install && git lfs pull"

  # Re-check
  pointers=()
  for pat in "${lfspatterns[@]}"; do
    local matches
    matches=($ROOT/$pat)
    [[ -e "${matches[0]}" ]] || continue
    for f in "${matches[@]}"; do
      if [[ -f "$f" ]]; then
        local fsize
        fsize=$(wc -c < "$f")
        if (( fsize < 500 )); then
          local header
          header=$(head -c 30 "$f")
          if [[ "$header" == "version https://git-lfs"* ]]; then
            pointers+=("$f")
          fi
        fi
      fi
    done
  done

  if ((${#pointers[@]})); then
    die "LFS pull incomplete — these files are still pointers:
  ${pointers[*]}
Manual fix: cd $ROOT && git lfs install && git lfs pull"
  fi
  ok "All git-lfs files verified."
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

# ---------------------------------------------------------------- device
# Audio OUTPUT DEVICE step (Phase B). Driver-dependent:
#   alsa       -> default + hardware devices (aplay -l) + PCM names (aplay -L)
#   pulseaudio -> default + sinks (pactl list sinks / list short sinks)
#   pipewire   -> skipped (FluidSynth's pipewire driver has no device setting)
# The final "Use plugin default" entry is also the EOF-safety default, and
# "default" is passed as NO flag (plugin default == default for these drivers).
scan_alsa_devices() {
  DEV_NAMES=(); DEV_LABELS=()
  local line cid cname dnum dname
  while IFS= read -r line; do
    if [[ $line =~ ^card\ [0-9]+:\ ([^,\[]+)\ \[([^\]]*)\],\ device\ ([0-9]+):\ ([^,\[]+)\ \[([^\]]*)\] ]]; then
      cid="${BASH_REMATCH[1]}"; cname="${BASH_REMATCH[2]}"
      dnum="${BASH_REMATCH[3]}"; dname="${BASH_REMATCH[5]}"
      ((${#DEV_NAMES[@]} >= 12)) && continue   # cap the list sensibly
      DEV_NAMES+=("plughw:CARD=$cid,DEV=$dnum")
      DEV_LABELS+=("$(printf '%-24s %s [dev %s: %s] (converted)' \
        "plughw:CARD=$cid,DEV=$dnum" "$cname" "$dnum" "$dname")")
      DEV_NAMES+=("hw:CARD=$cid,DEV=$dnum")
      DEV_LABELS+=("$(printf '%-24s %s [dev %s: %s] (raw/exclusive)' \
        "hw:CARD=$cid,DEV=$dnum" "$cname" "$dnum" "$dname")")
    fi
  done < <(aplay -l 2>/dev/null)
}

scan_alsa_pcms() {
  local line name="" desc
  while IFS= read -r line; do
    if [[ $line =~ ^(sysdefault|plughw|front|iec958|hdmi): ]]; then
      name="$line"; desc=""
    elif [[ -n "$name" && $line =~ ^[[:space:]]+([^[:space:]].*) ]]; then
      desc="${BASH_REMATCH[1]}"
      if [[ ! " ${DEV_NAMES[*]} " == *" $name "* ]] && ((${#DEV_NAMES[@]} < 12)); then
        DEV_NAMES+=("$name")
        DEV_LABELS+=("$(printf '%-24s %s (ALSA PCM)' "$name" "$desc")")
      fi
      name=""   # only the first indented line describes this PCM
    fi
  done < <(aplay -L 2>/dev/null)
}

scan_pulse_sinks() {
  PULSE_NAMES=(); PULSE_LABELS=()
  local line name="" desc idx pname
  while IFS= read -r line; do
    if [[ $line =~ ^[[:space:]]*Name:\ ([^[:space:]].*) ]]; then
      name="${BASH_REMATCH[1]}"; desc=""
    elif [[ $line =~ ^[[:space:]]*Description:\ (.*) && -n "$name" ]]; then
      desc="${BASH_REMATCH[1]}"
      PULSE_NAMES+=("$name")
      PULSE_LABELS+=("$(printf '%-46s %s' "$name" "$desc")")
      name=""
    fi
  done < <(pactl list sinks 2>/dev/null)
  if ((!${#PULSE_NAMES[@]})); then   # fallback: short form (no Description)
    while IFS=$'\t' read -r idx pname _; do
      [[ -n "$pname" ]] || continue
      PULSE_NAMES+=("$pname")
      PULSE_LABELS+=("$(printf '%-46s sink #%s' "$pname" "$idx")")
    done < <(pactl list short sinks 2>/dev/null)
  fi
}

choose_device_alsa() {
  msg "Scanning ALSA playback devices (aplay -l / aplay -L)..."
  scan_alsa_devices
  scan_alsa_pcms
  local labels=() i
  for ((i=0; i<${#DEV_NAMES[@]}; i++)); do labels+=("${DEV_LABELS[$i]}"); done
  warn "default routes through PipeWire; its mixer (pavucontrol / wpctl)"
  warn "  picks the destination port (speaker vs headphones)."
  warn "Direct hw:/plughw: bypasses PipeWire: exclusive access, no per-stream"
  warn "  volume, and it breaks the live-capture workflow (parecord --monitor-stream)."
  PS3=$'\nSelect audio output device: '
  select sel in "default (recommended - routes through PipeWire)" \
                "${labels[@]}" \
                "Enter a custom ALSA PCM name" \
                "Use plugin default (no --audio-device flag)"; do
    [[ -z "$sel" ]] && { warn "Invalid choice."; continue; }
    if [[ "$sel" == "Enter a custom ALSA PCM name" ]]; then
      read -rp "ALSA PCM name: " AUDIO_DEVICE || AUDIO_DEVICE=""
      [[ -n "$AUDIO_DEVICE" ]] || warn "Empty name - using plugin default."
    elif [[ "$sel" == "Use plugin default (no --audio-device flag)" ]]; then
      AUDIO_DEVICE=""
    elif (( REPLY == 1 )); then
      AUDIO_DEVICE="default"
    else
      AUDIO_DEVICE="${DEV_NAMES[$((REPLY-2))]}"
    fi
    ok "Audio device: ${AUDIO_DEVICE:-<plugin default>}"
    return 0
  done
  AUDIO_DEVICE=""   # EOF safety: plugin default
  ok "Audio device: <plugin default> (EOF)"
}

choose_device_pulse() {
  msg "Scanning PulseAudio/PipeWire sinks (pactl list sinks)..."
  scan_pulse_sinks
  local labels=() i
  for ((i=0; i<${#PULSE_NAMES[@]}; i++)); do labels+=("${PULSE_LABELS[$i]}"); done
  PS3=$'\nSelect audio output device: '
  select sel in "default" \
                "${labels[@]}" \
                "Enter a custom sink name" \
                "Use plugin default (no --audio-device flag)"; do
    [[ -z "$sel" ]] && { warn "Invalid choice."; continue; }
    if [[ "$sel" == "Enter a custom sink name" ]]; then
      read -rp "PulseAudio sink name: " AUDIO_DEVICE || AUDIO_DEVICE=""
      [[ -n "$AUDIO_DEVICE" ]] || warn "Empty name - using plugin default."
    elif [[ "$sel" == "Use plugin default (no --audio-device flag)" ]]; then
      AUDIO_DEVICE=""
    elif (( REPLY == 1 )); then
      AUDIO_DEVICE="default"
    else
      AUDIO_DEVICE="${PULSE_NAMES[$((REPLY-2))]}"
    fi
    ok "Audio device: ${AUDIO_DEVICE:-<plugin default>}"
    return 0
  done
  AUDIO_DEVICE=""   # EOF safety: plugin default
  ok "Audio device: <plugin default> (EOF)"
}

choose_device() {
  AUDIO_DEVICE=""
  case "${AUDIO_DRIVER:-alsa}" in
    pipewire)
      warn "pipewire driver: FluidSynth's native pipewire driver has NO device"
      warn "  setting upstream (and it fails on this machine) - skipping the"
      warn "  device menu, the driver default device will be used."
      return 0
      ;;
    pulseaudio) choose_device_pulse ;;
    *)          choose_device_alsa ;;
  esac
}

# ---------------------------------------------------------------- main
main() {
  banner
  check_lfs_files
  build_step
  choose_midi
  choose_plugin
  choose_driver
  choose_device
  [[ -n "${PLUGIN_PATH:-}" ]] || die "No plugin selected."
  AUDIO_DRIVER="${AUDIO_DRIVER:-alsa}"
  AUDIO_DEVICE="${AUDIO_DEVICE:-}"
  if [[ "$PLUGIN_PATH" == *harmonium* && ! -f "$DEFAULT_SOUNDFONT" ]]; then
    warn "SoundFont not found: $DEFAULT_SOUNDFONT"
    warn "The harmonium plugin may fail to start (its build was configured"
    warn "with a font other than the committed default). Override at configure"
    warn "time or point the build back at the in-repo default:"
    warn "  cmake -B build -DHARMONIUM_SOUNDFONT_PATH=$ROOT/plugins/harmonium/soundfonts/harmonium_v3.sf2"
  fi
  echo
  msg "Launching naadcore-cli (press Ctrl+C to quit)..."
  local args=(--plugin "$PLUGIN_PATH" --audio-driver "$AUDIO_DRIVER")
  [[ -n "${MIDI_ADDR:-}" ]] && args+=(--midi "$MIDI_ADDR")
  # Omit the flag entirely for "default"/unset: the plugin's own default device
  # is already "default" for these drivers (per-driver mapping in the plugin).
  if [[ -n "$AUDIO_DEVICE" && "$AUDIO_DEVICE" != "default" ]]; then
    args+=(--audio-device "$AUDIO_DEVICE")
  fi
  ok "Audio output: driver=$AUDIO_DRIVER device=${AUDIO_DEVICE:-<plugin default>}"
  exec "$CLI" "${args[@]}"
}

main "$@"
