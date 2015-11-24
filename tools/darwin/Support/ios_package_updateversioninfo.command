#!/bin/bash

# Update the build version number of the bundle (CFBundleVersion)
# The string should only contain numeric (0-9) and period (.) characters
# and MUST be monotonically increasing.
# we set it to the UTC ymd.hm value of when the build is run.
BUNDLE_REVISION="Unknown"
BUNDLE_NAME="$APP_NAME"

BUNDLE_REVISION=$(date -u +%y%m%d.%H%M)
echo setting CFBundleVersion version to $BUNDLE_REVISION
plutil -replace CFBundleVersion -string "$BUNDLE_REVISION" "$TARGET_BUILD_DIR/$BUNDLE_NAME.app/Info.plist"
