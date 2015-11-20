#!/bin/bash

# magic to make codesign work correctly
export CODESIGN_ALLOCATE=`xcodebuild -find codesign_allocate`

# Prefer the expanded name, if available.
CODE_SIGN_IDENTITY_FOR_ITEMS="${EXPANDED_CODE_SIGN_IDENTITY_NAME}"
if [ "${CODE_SIGN_IDENTITY_FOR_ITEMS}" = "" ] ; then
    # Fall back to old behavior.
    CODE_SIGN_IDENTITY_FOR_ITEMS="${CODE_SIGN_IDENTITY}"
fi

echo "${CODE_SIGN_IDENTITY_FOR_ITEMS}"
echo "${CODESIGNING_FOLDER_PATH}"

# pull the CFBundleIdentifier out of the built xxx.app
BUNDLEID=`mdls -name kMDItemCFBundleIdentifier ${CODESIGNING_FOLDER_PATH} -raw `
echo "CFBundleIdentifier is ${BUNDLEID}"

# codesign the internal items that will be missed when bundle is signed by xcode
LIST_BINARY_EXTENSIONS="dylib so"
for binext in $LIST_BINARY_EXTENSIONS
do
  codesign -fvvv -s "${CODE_SIGN_IDENTITY_FOR_ITEMS}" -i "${BUNDLEID}" `find ${CODESIGNING_FOLDER_PATH} -name "*.$binext" -type f` ${CODESIGNING_FOLDER_PATH}
done
