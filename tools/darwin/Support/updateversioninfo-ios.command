#!/bin/bash

# Update version in Info.plist with Git revision
BUNDLE_REVISION="Unknown"
BUNDLE_NAME="$APP_NAME"

BUNDLE_REVISION=$(date -u +%y%m%d%H%M)
echo setting CFBundleVersion version to $BUNDLE_REVISION
plutil -replace CFBundleVersion -string "$BUNDLE_REVISION" "$TARGET_BUILD_DIR/$BUNDLE_NAME.app/Info.plist"

