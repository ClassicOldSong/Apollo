#!/bin/zsh
# Builds ApolloTouch.app (apollo-touchd) into $OUT (default build/ApolloTouch.app).
#
#   tools/macos/touchd/build.sh                       unsigned build (the touchscreen cannot be made)
#   tools/macos/touchd/build.sh --sign "<identity>" --profile <file.provisionprofile>
#
# KEYCHAIN=<file> picks the keychain that holds the identity. The bundle id comes from the profile's App ID
# (BUNDLE_ID overrides it).
#
# The App ID's HID Virtual Device capability is development only, so the real build is signed with an
# Apple Development identity and a macOS App Development profile that lists the Mac it runs on.
set -e
cd "$(dirname "$0")/../../.."
OUT=${OUT:-build/ApolloTouch.app}
SIGN=""; PROFILE=""
while [ $# -gt 0 ]; do
  case "$1" in
    --sign) SIGN="$2"; shift 2;;
    --profile) PROFILE="$2"; shift 2;;
    *) echo "unknown argument $1"; exit 2;;
  esac
done

TMP=$(mktemp -d)
if [ -n "$SIGN" ]; then
  [ -f "$PROFILE" ] || { echo "a --profile is needed with --sign"; exit 2; }
  security cms -D -i "$PROFILE" > "$TMP/profile.plist"
  TEAM=$(/usr/libexec/PlistBuddy -c "Print :TeamIdentifier:0" "$TMP/profile.plist")
  APPID=$(/usr/libexec/PlistBuddy -c "Print :Entitlements:com.apple.application-identifier" "$TMP/profile.plist")
  HID=$(/usr/libexec/PlistBuddy -c "Print :Entitlements:com.apple.developer.hid.virtual.device" "$TMP/profile.plist" 2>/dev/null || true)
  BUNDLE_ID=${BUNDLE_ID:-${APPID#$TEAM.}}
  [ "$APPID" = "$TEAM.$BUNDLE_ID" ] || { echo "profile is for $APPID, bundle is $TEAM.$BUNDLE_ID"; exit 2; }
  [ "$HID" = "true" ] || { echo "profile does not grant com.apple.developer.hid.virtual.device"; exit 2; }
fi
BUNDLE_ID=${BUNDLE_ID:-apollo.touch}

rm -rf "$OUT"
mkdir -p "$OUT/Contents/MacOS"
clang++ -std=c++20 -O2 -fobjc-arc -I. -mmacosx-version-min=14.0 -arch arm64 \
  tools/macos/touchd/touchd.mm -framework Foundation -framework IOKit \
  -o "$OUT/Contents/MacOS/apollo-touchd"
cat > "$OUT/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleIdentifier</key><string>$BUNDLE_ID</string>
  <key>CFBundleName</key><string>Apollo Touch</string>
  <key>CFBundleExecutable</key><string>apollo-touchd</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>1.0</string>
  <key>CFBundleVersion</key><string>1</string>
  <key>LSMinimumSystemVersion</key><string>14.0</string>
  <key>LSUIElement</key><true/>
</dict></plist>
PLIST

if [ -z "$SIGN" ]; then
  codesign -f -s - "$OUT"
  rm -rf "$TMP"
  echo "built $OUT (ad-hoc, no HID entitlement)"
  exit 0
fi

cp "$PROFILE" "$OUT/Contents/embedded.provisionprofile"
cat > "$TMP/ent.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>com.apple.application-identifier</key><string>$APPID</string>
  <key>com.apple.developer.team-identifier</key><string>$TEAM</string>
  <key>com.apple.developer.hid.virtual.device</key><true/>
</dict></plist>
PLIST
KC_ARGS=(); [ -n "$KEYCHAIN" ] && KC_ARGS=(--keychain "$KEYCHAIN")
codesign -f --timestamp --options runtime "${KC_ARGS[@]}" --entitlements "$TMP/ent.plist" -s "$SIGN" "$OUT"
codesign -dv --entitlements - "$OUT" 2>&1 | grep -E "Identifier|TeamIdentifier|hid"
rm -rf "$TMP"
echo "built and signed $OUT"
