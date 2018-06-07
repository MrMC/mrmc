cmake_minimum_required(VERSION 3.4.1)

get_property(rtdir GLOBAL PROPERTY ROOT_DIR)
include ( ${rtdir}/includes.cmake )

set (my_SOURCES
  DDSImage.cpp
  DirectXGraphics.cpp
  DirtyRegionSolvers.cpp
  DirtyRegionTracker.cpp
  FocusabilityTracker.cpp
  FrameBufferObject.cpp
  GraphicContext.cpp
  GUIAction.cpp
  GUIAudioManager.cpp
  GUIBaseContainer.cpp
  GUIBorderedImage.cpp
  GUIButtonControl.cpp
  GUICheckMarkControl.cpp
  GUIColorManager.cpp
  GUIControl.cpp
  GUIControlFactory.cpp
  GUIControlGroup.cpp
  GUIControlGroupList.cpp
  GUIControlProfiler.cpp
  GUIDialog.cpp
  GUIEditControl.cpp
  GUIFadeLabelControl.cpp
  GUIFixedListContainer.cpp
  GUIFont.cpp
  GUIFontCache.cpp
  GUIFontManager.cpp
  GUIFontTTF.cpp
  GUIImage.cpp
  GUIIncludes.cpp
  GUIInfoTypes.cpp
  GUIKeyboardFactory.cpp
  GUILabel.cpp
  GUILabelControl.cpp
  GUIListContainer.cpp
  GUIListGroup.cpp
  GUIListItem.cpp
  GUIListItemLayout.cpp
  GUIListLabel.cpp
  GUIMessage.cpp
  GUIMoverControl.cpp
  GUIMultiImage.cpp
  GUIMultiSelectText.cpp
  GUIPanelContainer.cpp
  GUIProgressControl.cpp
  GUIRadioButtonControl.cpp
  GUIResizeControl.cpp
  GUIRenderingControl.cpp
  GUIRSSControl.cpp
  GUIScrollBarControl.cpp
  GUISelectButtonControl.cpp
  GUISettingsSliderControl.cpp
  GUISliderControl.cpp
  GUISpinControl.cpp
  GUISpinControlEx.cpp
  GUIStaticItem.cpp
  GUITextBox.cpp
  GUITextLayout.cpp
  GUITexture.cpp
  GUIToggleButtonControl.cpp
  GUIVideoControl.cpp
  GUIVisualisationControl.cpp
  GUIWindow.cpp
  GUIWindowManager.cpp
  GUIWrappingListContainer.cpp
  imagefactory.cpp
  IWindowManagerCallback.cpp
  JpegIO.cpp
  LocalizeStrings.cpp
  PngIO.cpp
  Shader.cpp
  StereoscopicsManager.cpp
  Texture.cpp
  TextureBundleXPR.cpp
  TextureBundleXBT.cpp
  TextureBundle.cpp
  TextureManager.cpp
  VisibleEffect.cpp
  XBTF.cpp
  XBTFReader.cpp
  GifIO.cpp

  TextureGL.cpp
  TexturePi.cpp
  GUIFontTTFGL.cpp
  GUITextureGLES.cpp
  MatrixGLES.cpp
  GUIShader.cpp

  )

file(GLOB my_HEADERS *.h)

add_library(
  guilib
  STATIC

  ${my_SOURCES}
  ${my_HEADERS}
  )
