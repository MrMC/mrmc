#!/bin/bash

# Update version in Info.plist with Git revision
GIT_REVISION="Unknown"
BUNDLE_NAME="$APP_NAME"

GIT_REVISION=$(cat git_revision.h | sed -n 's/\(.*\)\"\(.*\)\"\(.*\)/\2/p' | sed -n 's|-|.|p')
plutil -replace CFBundleVersion -string "$GIT_REVISION" "$TARGET_BUILD_DIR/$BUNDLE_NAME.app/Info.plist"

