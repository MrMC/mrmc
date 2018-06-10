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
while  read var value ; do
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

sed -i -e "s#${VARIABLES[APP_WEBSITE]}#@APP_WEBSITE@#g" -e "s#${VARIABLES[APP_PACKAGE]}#@APP_PACKAGE@#g" -e "s#${VARIABLES[APP_NAME_DISPLAY]}#@APP_NAME_DISPLAY@#g" -e "s#${VARIABLES[APP_NAME_LC]}#@APP_NAME_LC@#g" -e "s#${VARIABLES[APP_NAME_UC]}#@APP_NAME_UC@#g" -e "s#${VARIABLES[APP_VERSION_ANDROID_VERSION]}#@APP_VERSION_ANDROID_VERSION@#g" -e "s#${VARIABLES[APP_VERSION]}#@APP_VERSION@#g" $TMP_DIR/strings.xml.in
find $TMP_DIR -name "*.in" |xargs -i sed -i -e "s#${VARIABLES[APP_WEBSITE]}#@APP_WEBSITE@#g" -e "s#${VARIABLES[APP_PACKAGE]}#@APP_PACKAGE@#g" -e "s#${VARIABLES[APP_NAME]}#@APP_NAME@#g" -e "s#${VARIABLES[APP_NAME_LC]}#@APP_NAME_LC@#g" -e "s#${VARIABLES[APP_NAME_UC]}#@APP_NAME_UC@#g" -e "s#${VARIABLES[APP_VERSION_ANDROID_VERSION]}#@APP_VERSION_ANDROID_VERSION@#g" -e "s#${VARIABLES[APP_VERSION]}#@APP_VERSION@#g" {}
sed -i -e "s#appPackageName = \"@APP_PACKAGE@\";#appPackageName = \"tv.mrmc.mrmc\";#g" $TMP_JAVA_DIR/Main.java.in

rsync -a --delete $TMP_JAVA_DIR/ $TARGET_JAVA_DIR
cp -f $TMP_DIR/AndroidManifest.xml.in $TARGET_DIR/AndroidManifest.xml.in
cp -f $TMP_DIR/strings.xml.in $TARGET_DIR/strings.xml.in
cp -f $TMP_DIR/colors.xml.in $TARGET_DIR/colors.xml.in
cp -f $TMP_DIR/searchable.xml.in $TARGET_DIR/searchable.xml.in
cp -f $TMP_DIR/activity_main.xml.in $TARGET_DIR/activity_main.xml.in

rm -rf $TMP_DIR
