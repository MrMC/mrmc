#!/bin/bash

# magic to make codesign work correctly
export CODESIGN_ALLOCATE=`xcodebuild -find codesign_allocate`

echo "${CODE_SIGN_IDENTITY}"
echo "${CODESIGNING_FOLDER_PATH}"

# pull the CFBundleIdentifier out of the built xxx.app
BUNDLEID=`mdls -name kMDItemCFBundleIdentifier ${CODESIGNING_FOLDER_PATH} -raw `
echo "CFBundleIdentifier is ${BUNDLEID}"

# codesign the internal items that will be missed when bundle is signed by xcode
LIST_BINARY_EXTENSIONS="dylib so"
for binext in $LIST_BINARY_EXTENSIONS
do
  codesign -fvvv -s "${CODE_SIGN_IDENTITY}" -i "${BUNDLEID}" `find ${CODESIGNING_FOLDER_PATH} -name "*.$binext" -type f` ${CODESIGNING_FOLDER_PATH}
done
