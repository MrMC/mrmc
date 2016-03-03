#!/bin/bash

#      Copyright (C) 2015 Team MrMC
#      https://github.com/MrMC
#
#  This Program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2, or (at your option)
#  any later version.
#
#  This Program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with MrMC; see the file COPYING.  If not, see
#  <http://www.gnu.org/licenses/>.

#set -x

# magic to make codesign work correctly
export CODESIGN_ALLOCATE=`xcodebuild -find codesign_allocate`
# Prefer the expanded name, if available.
CODE_SIGN_IDENTITY_FOR_ITEMS="${EXPANDED_CODE_SIGN_IDENTITY_NAME}"
if [ "${CODE_SIGN_IDENTITY_FOR_ITEMS}" == "" ]; then
    # Fall back to old behavior.
    CODE_SIGN_IDENTITY_FOR_ITEMS="${CODE_SIGN_IDENTITY}"
fi
echo "${CODE_SIGN_IDENTITY_FOR_ITEMS}"

TARGET_NAME="${PRODUCT_NAME}.${WRAPPER_EXTENSION}"
TARGET_CONTENTS="${TARGET_BUILD_DIR}/${TARGET_NAME}"
TARGET_FRAMEWORKS="${TARGET_CONTENTS}/Frameworks"

# use the same date/time stamp format for all CFBundleVersions
BUNDLE_REVISION=$(date -u +%y%m%d.%H%M)

# ios/tvos use different framework plists
if [ "$PLATFORM_NAME" == "iphoneos" ]; then
  SEEDFRAMEWORKPLIST="${PROJECT_DIR}/xbmc/platform/darwin/ios/FrameworkSeed_Info.plist"
elif [ "${PLATFORM_NAME}" == "appletvos" ]; then
  SEEDFRAMEWORKPLIST="${PROJECT_DIR}/xbmc/platform/darwin/tvos/FrameworkSeed_Info.plist"
fi

function convert2framework
{
  DYLIB="${1}"
  # typical darwin dylib name format is lib<name>.<version>.dylib
  DYLIB_BASENAME=$(basename "${DYLIB}")
  # strip .<version>.dylib
  DYLIB_LIBBASENAME="${DYLIB_BASENAME%%.[0-9]*}"
  # make sure .dylib is stripped
  DYLIB_LIBNAME="${DYLIB_LIBBASENAME%.dylib}"

  if [ "$PLATFORM_NAME" == "iphoneos" ] || [ "$PLATFORM_NAME" == "appletvos" ]; then
    BUNDLEID="tv.mrmc.framework.${DYLIB_LIBNAME}"
    echo "CFBundleIdentifier is ${BUNDLEID}"
    echo "convert ${DYLIB_BASENAME} to ${DYLIB_LIBNAME}.framework and codesign"

    DEST_FRAMEWORK="${TARGET_FRAMEWORKS}/${DYLIB_LIBNAME}.framework"
    mkdir -p "${DEST_FRAMEWORK}"
    mkdir -p "${DEST_FRAMEWORK}/Headers"
    mkdir -p "${DEST_FRAMEWORK}/Modules"

    # framework plists are binary
    plutil -convert binary1 "${SEEDFRAMEWORKPLIST}" -o "${DEST_FRAMEWORK}/Info.plist"
    # set real CFBundleName
    plutil -replace CFBundleName -string "${DYLIB_LIBNAME}" "${DEST_FRAMEWORK}/Info.plist"
    # set real CFBundleVersion
    plutil -replace CFBundleVersion -string "${BUNDLE_REVISION}" "${DEST_FRAMEWORK}/Info.plist"
    # set real CFBundleIdentifier
    plutil -replace CFBundleIdentifier -string "${BUNDLEID}" "${DEST_FRAMEWORK}/Info.plist"
    # set real CFBundleExecutable
    plutil -replace CFBundleExecutable -string "${DYLIB_LIBNAME}" "${DEST_FRAMEWORK}/Info.plist"
    # move it (not copy)
    mv -f "${DYLIB}" "${DEST_FRAMEWORK}/${DYLIB_LIBNAME}"

    # fixup loader id/paths
    LC_ID_DYLIB="@rpath/${DYLIB_LIBNAME}.framework/${DYLIB_LIBNAME}"
    LC_RPATH1="@executable_path/Frameworks"
    LC_RPATH2="@loader_path/Frameworks"
    install_name_tool -id "${LC_ID_DYLIB}" "${DEST_FRAMEWORK}/${DYLIB_LIBNAME}"
    install_name_tool -add_rpath "${LC_RPATH1}" "${DEST_FRAMEWORK}/${DYLIB_LIBNAME}"
    install_name_tool -add_rpath "${LC_RPATH2}" "${DEST_FRAMEWORK}/${DYLIB_LIBNAME}"

    if [ "$STRIP_INSTALLED_PRODUCT" == "YES" ]; then
      strip -x "${DEST_FRAMEWORK}/${DYLIB_LIBNAME}"
    fi
    # codesign the framework to match its CFBundleIdentifier
    codesign --deep -f -s "${CODE_SIGN_IDENTITY_FOR_ITEMS}" -i "${BUNDLEID}" "${DEST_FRAMEWORK}/${DYLIB_LIBNAME}"

    if [ "$ACTION" == install ]; then
      # extract the uuid and use it to find the matching bcsymbolmap (needed for crashlog symbolizing)
      UUID=$(otool -l "${DEST_FRAMEWORK}/${DYLIB_LIBNAME}" | grep uuid | awk '{ print $2}')
      echo "bcsymbolmap is ${UUID}"
      if [ -f "${XBMC_DEPENDS}/bcsymbolmaps/${UUID}.bcsymbolmap" ]; then
        echo "bcsymbolmap is ${UUID}.bcsymbolmap"
        cp -f "${XBMC_DEPENDS}/bcsymbolmaps/${UUID}.bcsymbolmap" "${BUILT_PRODUCTS_DIR}/"
      fi
    fi

  else
    BUNDLEID="tv.mrmc.dylib.${DYLIB_LIBNAME}"
    echo "CFBundleIdentifier is ${BUNDLEID}"
    codesign --deep -f -s "${CODE_SIGN_IDENTITY_FOR_ITEMS}" -i "${BUNDLEID}" "${DYLIB}"
  fi
}

# loop over all xxx.dylibs in xxx.app/Frameworks
for dylib in $(find "${TARGET_FRAMEWORKS}" -name "*.dylib" -type f); do
  convert2framework "${dylib}"
done
