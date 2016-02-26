@echo off
ECHO ----------------------------------------
echo Creating re-Touched Build Folder
IF Exist ..\..\project\Win32BuildSetup\BUILD_WIN32\addons\skin.re-touched rmdir ..\..\project\Win32BuildSetup\BUILD_WIN32\addons\skin.re-touched /S /Q
md ..\..\project\Win32BuildSetup\BUILD_WIN32\addons\skin.re-touched\media\

Echo build.bat>>exclude.txt
Echo .git>>exclude.txt
Echo \skin.re-touched\media\>>exclude.txt
Echo exclude.txt>>exclude.txt

ECHO ----------------------------------------
ECHO Creating XBT File...
START /B /WAIT ..\..\Tools\TexturePacker\TexturePacker -dupecheck -input media -output ..\..\project\Win32BuildSetup\BUILD_WIN32\addons\skin.re-touched\media\Textures.xbt

ECHO ----------------------------------------
ECHO XBT Texture Files Created...
ECHO Building Skin Directory...
xcopy "..\skin.re-touched" "..\..\project\Win32BuildSetup\BUILD_WIN32\addons\skin.re-touched" /E /Q /I /Y /EXCLUDE:exclude.txt

del exclude.txt
