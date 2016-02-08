#!/bin/bash

echo "copy root files"

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
  SYNC="rsync -aq ${PLATFORM} ${BUILDSYS} ${BUILDSYS}"

  if [ "$PLATFORM_NAME" == "appletvos" ] ; then
    # clean keymaps folder, not even sure this is needed
    SYNC="${SYNC} --include *keymaps/keyboard.xml --include *keymaps/joystick.AppleRemote.xml --exclude *keymaps/* --exclude *nyxboard*"
    # clean settings folder, only use stuff for tvos
    SYNC="${SYNC} --include *settings/settings.xml --include *settings/darwin_tvos.xml --include *settings/darwin.xml --exclude *settings/*"
  fi

  # rsync command for language pacs
  LANGSYNC="rsync -aq ${PLATFORM} ${BUILDSRC} ${BUILDSYS} --exclude resource.uisounds*"

  # rsync command for including everything but the skins
  DEFAULTSKIN_EXCLUDES="--exclude addons/skin.mrmc --exclude addons/skin.mrmc-touch --exclude addons/skin.amber --exclude addons/skin.pm3.hd"
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

  # sync skin.mrmc
  SYNCSKIN_A=${SYNC}
  if [ -f "$SRCROOT/addons/skin.mrmc/media/Textures.xbt" ]; then
    SYNCSKIN_A="${SYNC} --exclude *.png --exclude *.jpg --exclude media/Makefile* --exclude media/Subtitles --exclude media/LeftRating --exclude media/flagging --exclude media/epg-genres --exclude media/CenterRating"
  fi
  ${SYNCSKIN_A} "$SRCROOT/addons/skin.mrmc"         "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome/addons"
  ${SYNC} "$SRCROOT/addons/skin.mrmc/backgrounds"   "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome/addons/skin.mrmc"
  ${SYNC} "$SRCROOT/addons/skin.mrmc/icon.png"      "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome/addons/skin.mrmc"

  # sync touch skin if it exists
  if [ -f "$SRCROOT/addons/skin.mrmc-touch/addon.xml" ] && [ "$PLATFORM_NAME" == "iphoneos" ]; then
    SYNCSKIN_B=${SYNC}
    if [ -f "$SRCROOT/addons/skin.mrmc-touch/media/Textures.xbt" ]; then
      SYNCSKIN_B="${SYNC} --exclude *.png --exclude *.jpg --exclude media/Makefile*"
    fi
    ${SYNCSKIN_B} "$SRCROOT/addons/skin.mrmc-touch"    "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome/addons"
    ${SYNC} "$SRCROOT/addons/skin.mrmc-touch/background" "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome/addons/skin.mrmc-touch"
    ${SYNC} "$SRCROOT/addons/skin.mrmc-touch/icon.png" "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome/addons/skin.mrmc-touch"
  fi

  # sync amber skin on tvos
  if [ -f "$SRCROOT/addons/skin.amber/addon.xml" ] && [ "$PLATFORM_NAME" == "appletvos" ]; then
    SYNCSKIN_C=${SYNC}
    if [ -f "$SRCROOT/addons/skin.amber/media/Textures.xbt" ]; then
      SYNCSKIN_C="${SYNC} --exclude *.png --exclude *.jpg --exclude media/Makefile*"
    fi
    ${SYNCSKIN_C} "$SRCROOT/addons/skin.amber"    "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome/addons"
    ${SYNC} "$SRCROOT/addons/skin.amber/backgrounds" "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome/addons/skin.amber"
    ${SYNC} "$SRCROOT/addons/skin.amber/icon.png" "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome/addons/skin.amber"
  fi

    # sync pm3.hd skin on tvos
  if [ -f "$SRCROOT/addons/skin.pm3.hd/addon.xml" ] && [ "$PLATFORM_NAME" == "appletvos" ]; then
    SYNCSKIN_D=${SYNC}
    if [ -f "$SRCROOT/addons/skin.pm3.hd/media/Textures.xbt" ]; then
      SYNCSKIN_D="${SYNC} --exclude *.png --exclude *.jpg --exclude *.gif --exclude media/Makefile*"
    fi
    ${SYNCSKIN_D} "$SRCROOT/addons/skin.pm3.hd"    "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome/addons"
    ${SYNC} "$SRCROOT/addons/skin.pm3.hd/backgrounds" "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome/addons/skin.pm3.hd"
    ${SYNC} "$SRCROOT/addons/skin.pm3.hd/icon.png" "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome/addons/skin.pm3.hd"
  fi

  # fixups, addons might have silly symlinks because cmake is stupid, remove them
  # rsync can be strange so we manually do these fixes.
  find "$TARGET_BUILD_DIR/$TARGET_NAME/AppData/AppHome" -type l -delete
  find "$TARGET_BUILD_DIR/$TARGET_NAME/" -name ".git*" -delete
fi
