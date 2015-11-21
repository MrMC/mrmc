#!/bin/bash

if ! [ -z "${CODE_SIGN_IDENTITY}" ] && [ "${CODE_SIGN_IDENTITY}" != "Don't Code Sign"  ]; then
  # magic to make codesign work correctly
  export CODESIGN_ALLOCATE=`xcodebuild -find codesign_allocate`

  # Prefer the expanded name, if available.
  CODE_SIGN_IDENTITY_FOR_ITEMS="${EXPANDED_CODE_SIGN_IDENTITY_NAME}"
  if [ "${CODE_SIGN_IDENTITY_FOR_ITEMS}" = "" ]; then
      # Fall back to old behavior.
      CODE_SIGN_IDENTITY_FOR_ITEMS="${CODE_SIGN_IDENTITY}"
  fi

  # todo: pull from xxx.app/Info.plist
  BUNDLEID="tv.mrmc.mrmc.osx"
  if [ "${PLATFORM_NAME}" == "iphoneos" ]; then
    BUNDLEID="tv.mrmc.mrmc.ios"
  elif [ "${PLATFORM_NAME}" == "appletvos" ]; then
    BUNDLEID="tv.mrmc.mrmc.tvos"
  fi
  echo "CFBundleIdentifier is ${BUNDLEID}"
  echo "${CODE_SIGN_IDENTITY_FOR_ITEMS}"
  echo "${CODESIGNING_FOLDER_PATH}"

  # codesign the items that will be missed when main bundle is signed by xcode
  #codesign --deep -fvvv -s "${CODE_SIGN_IDENTITY_FOR_ITEMS}" -i "${BUNDLEID}" `find ${CODESIGNING_FOLDER_PATH}/Frameworks -name "*.framework" -type d`

  #codesign -fvvv -s "${CODE_SIGN_IDENTITY_FOR_ITEMS}" -i "${BUNDLEID}" `find ${CODESIGNING_FOLDER_PATH} -name "*.dylib" -type f` "${CODESIGNING_FOLDER_PATH}"
fi
