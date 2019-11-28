#!/bin/bash

if [ "$#" -ne 1 ]; then
  echo "Source dir to sync required"
  exit 1
fi

SRC_DIR=$1
ROOT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/../../../" && pwd )"
TMP_DIR="${ROOT_DIR}/tools/android/packaging/tmp"
TMP_JAVA_DIR="${TMP_DIR}/java"
TARGET_DIR="${ROOT_DIR}/tools/android/packaging/xbmc"
TARGET_JAVA_DIR="${TARGET_DIR}/src"

declare -A VARIABLES
while  IFS="= " read -r var value ; do
  if [[ $var =~ [a-zA-Z] ]]; then
    VARIABLES[$var]=$value
  fi
done < <(cat ${ROOT_DIR}/tools/android/packaging/variables.txt)

APP_PACKAGE_SLASH=${VARIABLES[APP_PACKAGE]//./\/}

rm -rf $TMP_DIR
mkdir -p $TMP_JAVA_DIR

function cp_java {
  #echo $SRC_DIR/java/$APP_PACKAGE_SLASH/$1 $TMP_DIR/$1.in
  mkdir -p $(dirname "$TMP_JAVA_DIR/$1")
  cp $SRC_DIR/java/$APP_PACKAGE_SLASH/$1 $TMP_JAVA_DIR/$1.in
}

find $1/java -type f -name "*.java" | sed "s|$1/java/$APP_PACKAGE_SLASH||" | while read file; do
  cp_java $file
done

cp $SRC_DIR/AndroidManifest.xml $TMP_DIR/AndroidManifest.xml.in
cp $SRC_DIR/res/values/strings.xml $TMP_DIR/strings.xml.in
cp $SRC_DIR/res/values/colors.xml $TMP_DIR/colors.xml.in
cp $SRC_DIR/res/xml/searchable.xml $TMP_DIR/searchable.xml.in
cp $SRC_DIR/res/layout/activity_main.xml $TMP_DIR/activity_main.xml.in

cp $SRC_DIR/CMakeLists.txt $TMP_DIR/CMakeLists.txt.in
cp $SRC_DIR/sources.cmake $TMP_DIR/sources.cmake
cp $SRC_DIR/libs.cmake $TMP_DIR/libs.cmake
#cp $SRC_DIR/includes.cmake $TMP_DIR/includes.cmake
cp $SRC_DIR/build.gradle $TMP_DIR/build.gradle.in

sed -i -e "s#${VARIABLES[APP_WEBSITE]}#@APP_WEBSITE@#g" -e "s#${VARIABLES[APP_PACKAGE]}#@APP_PACKAGE@#g" -e "s#${VARIABLES[APP_NAME_DISPLAY]}#@APP_NAME_DISPLAY@#g" -e "s#${VARIABLES[APP_NAME_LC]}#@APP_NAME_LC@#g" -e "s#${VARIABLES[APP_NAME_UC]}#@APP_NAME_UC@#g" -e "s#${VARIABLES[APP_VERSION_ANDROID_VERSION]}#@APP_VERSION_ANDROID_VERSION@#g" -e "s#${VARIABLES[APP_VERSION]}#@APP_VERSION@#g" -e "s#${VARIABLES[APP_NAME_INSTALL_LC]}#@APP_NAME_INSTALL_LC@#g" $TMP_DIR/strings.xml.in
find $TMP_DIR -name "*.in" |xargs -i sed -i -e "s#${VARIABLES[APP_WEBSITE]}#@APP_WEBSITE@#g" -e "s#${VARIABLES[APP_PACKAGE]}#@APP_PACKAGE@#g" -e "s#${VARIABLES[APP_NAME]}#@APP_NAME@#g" -e "s#${VARIABLES[APP_NAME_LC]}#@APP_NAME_LC@#g" -e "s#${VARIABLES[APP_NAME_UC]}#@APP_NAME_UC@#g" -e "s#${VARIABLES[APP_VERSION_ANDROID_VERSION]}#@APP_VERSION_ANDROID_VERSION@#g" -e "s#${VARIABLES[APP_VERSION]}#@APP_VERSION@#g" -e "s#${VARIABLES[APP_NAME_INSTALL_LC]}#@APP_NAME_INSTALL_LC@#g" {}
find $TMP_DIR -name "Main.java.in" |xargs -i sed -i -e "s#appPackageName = \"@APP_PACKAGE@\";#appPackageName = \"tv.mrmc.mrmc\";#g" {}

rsync -a --delete $TMP_JAVA_DIR/ $TARGET_JAVA_DIR
cp -f $TMP_DIR/AndroidManifest.xml.in $TARGET_DIR/AndroidManifest.xml.in
cp -f $TMP_DIR/strings.xml.in $TARGET_DIR/strings.xml.in
cp -f $TMP_DIR/colors.xml.in $TARGET_DIR/colors.xml.in
cp -f $TMP_DIR/searchable.xml.in $TARGET_DIR/searchable.xml.in
cp -f $TMP_DIR/activity_main.xml.in $TARGET_DIR/activity_main.xml.in

cp -f $TMP_DIR/CMakeLists.txt.in $TARGET_DIR/CMakeLists.txt.in
cp -f $TMP_DIR/sources.cmake $TARGET_DIR/sources.cmake
cp -f $TMP_DIR/libs.cmake $TARGET_DIR/libs.cmake
#cp -f $TMP_DIR/includes.cmake $TARGET_DIR/includes.cmake
cp -f $TMP_DIR/build.gradle.in $TARGET_DIR/build.gradle.in

rm -rf $TMP_DIR
