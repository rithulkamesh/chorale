#!/usr/bin/env bash
# Codesign, notarize, and staple the macOS plugin bundles.
#
#   ./scripts/notarize.sh <artefacts-dir> <apple-id> <team-id> <app-password>
#
# artefacts-dir is e.g. build/Chorale_artefacts/Release. Requires a
# "Developer ID Application" certificate in the keychain and an app-specific
# password from appleid.apple.com. Takes a few minutes, not days — most delay
# people hit is submitting without `--wait` and polling manually.
set -euo pipefail

DIR="$1"; APPLE_ID="$2"; TEAM_ID="$3"; APP_PW="$4"
IDENTITY=$(security find-identity -v -p codesigning | grep "Developer ID Application" | head -1 | awk '{print $2}')
[ -n "$IDENTITY" ] || { echo "no Developer ID Application identity in keychain"; exit 1; }

BUNDLES=("$DIR/AU/Chorale.component" "$DIR/VST3/Chorale.vst3" "$DIR/Standalone/Chorale.app")

for b in "${BUNDLES[@]}"; do
    echo "signing $b"
    codesign --force --deep --options runtime --timestamp -s "$IDENTITY" "$b"
done

ZIP=$(mktemp -d)/Chorale-notarize.zip
(cd "$DIR" && zip -ryq "$ZIP" AU/Chorale.component VST3/Chorale.vst3 Standalone/Chorale.app)

echo "submitting for notarization (waits for the result)..."
xcrun notarytool submit "$ZIP" --apple-id "$APPLE_ID" --team-id "$TEAM_ID" \
    --password "$APP_PW" --wait

for b in "${BUNDLES[@]}"; do
    xcrun stapler staple "$b"
done
echo "notarized and stapled."
