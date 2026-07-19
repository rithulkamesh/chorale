#!/usr/bin/env bash
# Install Chorale on macOS from the latest GitHub release.
#
#   curl -fsSL https://raw.githubusercontent.com/rithulkamesh/chorale/master/scripts/install.sh | bash
#
# Or from a clone:
#   ./scripts/install.sh
#
# Downloads the universal macOS zip, clears Gatekeeper quarantine, ad-hoc
# codesigns (no Apple Developer account required), and installs AU / VST3 /
# Standalone into the usual user locations.
set -euo pipefail

REPO="rithulkamesh/chorale"
ASSET="Chorale-macOS.zip"
AU_DST="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DST="$HOME/Library/Audio/Plug-Ins/VST3"
APP_DST="$HOME/Applications"

# ── terminal ──────────────────────────────────────────────────────────────
if [[ -t 1 ]]; then
  BOLD=$'\033[1m'; DIM=$'\033[2m'; GREEN=$'\033[32m'
  CYAN=$'\033[36m'; RED=$'\033[31m'; RESET=$'\033[0m'
else
  BOLD=""; DIM=""; GREEN=""; CYAN=""; RED=""; RESET=""
fi

say()  { printf '%s\n' "$*"; }
step() { printf '\n  %s▸%s %s%s%s\n' "$CYAN" "$RESET" "$BOLD" "$*" "$RESET"; }
ok()   { printf '  %s✓%s %s\n' "$GREEN" "$RESET" "$*"; }
fail() { printf '  %s✗%s %s\n' "$RED" "$RESET" "$*" >&2; exit 1; }

# Spinner while a background pid runs. Args: pid label
spin() {
  local pid=$1 label=$2
  local frames='⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏' i=0
  if [[ ! -t 1 ]]; then
    wait "$pid"; return $?
  fi
  while kill -0 "$pid" 2>/dev/null; do
    printf '\r  %s%s%s  %s' "$CYAN" "${frames:i++%${#frames}:1}" "$RESET" "$label"
    sleep 0.08
  done
  printf '\r\033[K'
  wait "$pid"
}

# Download with a live percent bar. Args: url dest
download() {
  local url=$1 dest=$2
  if command -v curl >/dev/null 2>&1; then
    if [[ -t 1 ]]; then
      curl -fL --progress-bar -o "$dest" "$url"
    else
      curl -fsL -o "$dest" "$url"
    fi
  else
    fail "curl is required"
  fi
}

# ── preflight ─────────────────────────────────────────────────────────────
[[ "$(uname -s)" == "Darwin" ]] || fail "this installer is for macOS only"

say ""
say "  ${BOLD}Chorale${RESET}  ${DIM}macOS installer${RESET}"
say "  ${DIM}────────────────────────────────────${RESET}"

# ── resolve latest release ─────────────────────────────────────────────────
step "checking latest release"
TAG=""
API="https://api.github.com/repos/${REPO}/releases/latest"
if TAG=$(curl -fsSL "$API" 2>/dev/null | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p' | head -1) \
   && [[ -n "$TAG" ]]; then
  :
else
  fail "could not reach GitHub releases — check your network"
fi
URL="https://github.com/${REPO}/releases/download/${TAG}/${ASSET}"
ok "$TAG"

# ── download ──────────────────────────────────────────────────────────────
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
ZIP="$TMP/$ASSET"

step "downloading ${ASSET}"
download "$URL" "$ZIP"
SIZE=$(du -h "$ZIP" | awk '{print $1}')
ok "${SIZE} from GitHub"

# ── unpack ────────────────────────────────────────────────────────────────
step "unpacking"
( cd "$TMP" && unzip -q "$ZIP" ) &
spin $! "extracting…"
ok "AU · VST3 · Standalone"

COMPONENT="$TMP/AU/Chorale.component"
VST3="$TMP/VST3/Chorale.vst3"
APP="$TMP/Standalone/Chorale.app"

[[ -d "$COMPONENT" && -d "$VST3" && -d "$APP" ]] \
  || fail "zip layout unexpected — expected AU/, VST3/, Standalone/"

# ── clear quarantine + ad-hoc sign ─────────────────────────────────────────
# Unsigned downloads land with com.apple.quarantine, which makes Gatekeeper
# block them. Clearing the xattr and ad-hoc signing (-s -) is the standard
# local-trust path when no Developer ID certificate is available.
step "trusting locally (quarantine + ad-hoc sign)"
(
  xattr -cr "$COMPONENT" "$VST3" "$APP"
  codesign --force --deep -s - "$COMPONENT" >/dev/null
  codesign --force --deep -s - "$VST3"      >/dev/null
  codesign --force --deep -s - "$APP"       >/dev/null
) &
spin $! "signing…"
ok "ad-hoc signed · quarantine cleared"

# ── install ────────────────────────────────────────────────────────────────
step "installing"
mkdir -p "$AU_DST" "$VST3_DST" "$APP_DST"

rm -rf "$AU_DST/Chorale.component" "$VST3_DST/Chorale.vst3" "$APP_DST/Chorale.app"
cp -R "$COMPONENT" "$AU_DST/"
cp -R "$VST3"      "$VST3_DST/"
cp -R "$APP"       "$APP_DST/"

ok "AU    → ${AU_DST}/Chorale.component"
ok "VST3  → ${VST3_DST}/Chorale.vst3"
ok "App   → ${APP_DST}/Chorale.app"

# ── done ───────────────────────────────────────────────────────────────────
say ""
say "  ${GREEN}${BOLD}installed ${TAG}${RESET}"
say "  ${DIM}rescan plugins in your DAW, or open Chorale from ~/Applications${RESET}"
say ""
