#!/bin/bash

set -x

#this is the list of binaries we have to sign for being able to run un-jailbroken
LIST_BINARY_EXTENSIONS="dylib so 0 vis pvr"

export CODESIGN_ALLOCATE=`xcodebuild -find codesign_allocate`

GEN_ENTITLEMENTS="$XBMC_DEPENDS_ROOT/buildtools-native/bin/gen_entitlements.py"

if [ ! -f ${GEN_ENTITLEMENTS} ]; then
  echo "error: $GEN_ENTITLEMENTS not found. Codesign won't work."
  exit -1
fi

BUNDLEID="org.mrmc.osx.mrmc"
if [ "${PLATFORM_NAME}" == "iphoneos" ]; then
  BUNDLEID="org.mrmc.ios.mrmc"
elif [ "${PLATFORM_NAME}" == "appletvos" ]; then
  BUNDLEID="org.mrmc.tvos.mrmc"
fi
echo "The BUNDLEID is ${BUNDLEID}"
echo "${CODE_SIGN_IDENTITY}"
echo "${BUILT_PRODUCTS_DIR}/${WRAPPER_NAME}/${PROJECT_NAME}.xcent"
echo "${CODESIGNING_FOLDER_PATH}"

${GEN_ENTITLEMENTS} "${BUNDLEID}" "${BUILT_PRODUCTS_DIR}/${WRAPPER_NAME}/${PROJECT_NAME}.xcent";
codesign -v -f -s "${CODE_SIGN_IDENTITY}" --entitlements "${BUILT_PRODUCTS_DIR}/${WRAPPER_NAME}/${PROJECT_NAME}.xcent" "${BUILT_PRODUCTS_DIR}/${WRAPPER_NAME}/"

echo Doing a full bundle sign using genuine identity "${CODE_SIGN_IDENTITY}"
for binext in $LIST_BINARY_EXTENSIONS
do
  codesign -fvvv -s "${CODE_SIGN_IDENTITY}" -i "${BUNDLEID}" `find ${CODESIGNING_FOLDER_PATH} -name "*.$binext" -type f` ${CODESIGNING_FOLDER_PATH}
done

echo "In case your app crashes with SIG_SIGN check the variable LIST_BINARY_EXTENSIONS in tools/darwin/Support/Codesign.command"
