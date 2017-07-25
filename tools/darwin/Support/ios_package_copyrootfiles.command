#!/bin/bash

#set -x

function package_skin
{
  SYNC_CMD="${1}"
  SKIN_PATH="${2}"
  SKIN_NAME=$(basename "${SKIN_PATH}")

  echo "Packaging ${SKIN_NAME}"

  if [ -f "$SKIN_PATH/addon.xml" ]; then
    SYNCSKIN_CMD=${SYNC_CMD}
    if [ -f "$SKIN_PATH/media/Textures.xbt" ]; then
      SYNCSKIN_CMD="${SYNC_CMD} --include *${SKIN_NAME}/media/Textures.xbt --exclude *${SKIN_NAME}/media/*"
    fi
    ${SYNCSKIN_CMD} "$SKIN_PATH" "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome/addons"
  fi
}

echo "packaging root files"

if [ "$ACTION" == build ] || [ "$ACTION" == install ]; then

  # for external testing
  TARGET_NAME=$PRODUCT_NAME.$WRAPPER_EXTENSION
  #SRCROOT=/Users/Shared/xbmc_svn/$APP_NAME
  #TARGET_BUILD_DIR=/Users/Shared/xbmc_svn/$APP_NAME/build/Debug

  PLATFORM="--exclude .DS_Store*"
  BUILDSYS="--exclude *.a --exclude *.dll --exclude *.DLL --exclude *.zlib --exclude *linux.* --exclude *x86-osx.*"
  BUILDSRC="--exclude CVS* --exclude .svn* --exclude .cvsignore* --exclude .cvspass* --exclude *.bat --exclude *README --exclude *README.txt"
  BUILDDBG="--exclude *.dSYM --exclude *.bcsymbolmap"
  # rsync command with exclusions for items we don't want in the app package
  SYNC="rsync -aq ${PLATFORM} ${BUILDSYS} ${BUILDDBG}"

  if [ "$PLATFORM_NAME" == "appletvos" ] ; then
    # clean keymaps folder, only include required items for tvos
    SYNC="${SYNC} --include *keymaps/keyboard.xml --include *keymaps/customcontroller.SiriRemote.xml --exclude *keymaps/* --exclude *nyxboard*"
    # clean settings folder, only include required items for tvos
    SYNC="${SYNC} --include *settings/settings.xml --include *settings/settings.lite.xml --include *settings/darwin_tvos.xml --include *settings/darwin.xml --exclude *settings/*"
  fi

  if [ "$PLATFORM_NAME" == "iphoneos" ] ; then
    # clean keymaps folder, only include required items for ios
    SYNC="${SYNC} --include *keymaps/keyboard.xml --include *keymaps/touchscreen.xml --exclude *keymaps/* --exclude *nyxboard*"
    # clean settings folder, only include required items for ios
    SYNC="${SYNC} --include *settings/settings.xml --include *settings/settings.lite.xml --include *settings/darwin_ios.xml --include *settings/darwin.xml --exclude *settings/*"
  fi  

  # rsync command for language pacs
  LANGSYNC="rsync -aq ${PLATFORM} ${BUILDSRC} ${BUILDSYS} --exclude resource.uisounds*"

  # rsync command for including everything but the skins
  DEFAULTSKIN_EXCLUDES="--exclude addons/skin.mrmc --exclude addons/skin.re-touched --exclude addons/skin.amber --exclude addons/skin.pm3.hd --exclude addons/skin.sio2 --exclude addons/skin.opacity"
  ADDONSYNC="rsync -aq ${PLATFORM} ${BUILDSRC} ${BUILDDBG} ${DEFAULTSKIN_EXCLUDES} --exclude addons/lib --exclude addons/share  --exclude *changelog.* --exclude *library.*/*.h --exclude *library.*/*.cpp --exclude *xml.in"

  # binary name is MrMC but we build MrMC so to get a clean binary each time
  mv $TARGET_BUILD_DIR/$TARGET_NAME/$APP_NAME $TARGET_BUILD_DIR/$TARGET_NAME/$APP_NAME

  mkdir -p "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome"
  mkdir -p "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome/addons"
  mkdir -p "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome/media"
  mkdir -p "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome/system"
  mkdir -p "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome/userdata"
  mkdir -p "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome/media"

  ${SYNC} "$SRCROOT/LICENSE.GPL"  "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/"
  ${SYNC} "$SRCROOT/xbmc/platform/darwin/Credits.html"  "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/"
  ${SYNC} "$SRCROOT/media"        "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome"
  ${SYNC} "$SRCROOT/system"       "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome"
  ${SYNC} "$SRCROOT/userdata"     "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome"
  ${ADDONSYNC} "$SRCROOT/addons"  "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome"

  # package items that are located in depends
  ${LANGSYNC} "$XBMC_DEPENDS/mrmc/repo-resources/" "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome/addons"
  ${ADDONSYNC} "$XBMC_DEPENDS/mrmc/addons/" "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome/addons"

  # always sync skin.mrmc
  package_skin "${SYNC}" "$SRCROOT/addons/skin.mrmc"
  package_skin "${SYNC}" "$SRCROOT/addons/skin.opacity"

  # sync touch skin if it exists
  if [ -f "$SRCROOT/addons/skin.re-touched/addon.xml" ] && [ "$PLATFORM_NAME" == "iphoneos" ]; then
    package_skin "${SYNC}" "$SRCROOT/addons/skin.re-touched"
  fi

  # sync amber skin if tvos
  if [ -f "$SRCROOT/addons/skin.amber/addon.xml" ] && [ "$PLATFORM_NAME" == "appletvos" ]; then
    package_skin "${SYNC}" "$SRCROOT/addons/skin.amber"
  fi

    # sync pm3.hd skin if tvos
  if [ -f "$SRCROOT/addons/skin.pm3.hd/addon.xml" ] && [ "$PLATFORM_NAME" == "appletvos" ]; then
    package_skin "${SYNC}" "$SRCROOT/addons/skin.pm3.hd"
  fi

  # sync sio2 skin if tvos
  if [ -f "$SRCROOT/addons/skin.sio2/addon.xml" ] && [ "$PLATFORM_NAME" == "appletvos" ]; then
    package_skin "${SYNC}" "$SRCROOT/addons/skin.sio2"
  fi

  # fixups, addons might have silly symlinks because cmake is stupid, remove them
  # rsync can be strange so we manually do these fixes.
  find "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome" -type l -delete
  find "$TARGET_BUILD_DIR/$TARGET_NAME/" -name ".git*" -delete
fi
