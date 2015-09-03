SRCS  = GUIDialogPictureInfo.cpp
SRCS += GUIViewStatePictures.cpp
SRCS += GUIWindowPictures.cpp
SRCS += GUIWindowSlideShow.cpp
SRCS += Picture.cpp
SRCS += PictureInfoLoader.cpp
SRCS += PictureInfoTag.cpp
SRCS += PictureScalingAlgorithm.cpp
SRCS += PictureThumbLoader.cpp
SRCS += SlideShowPicture.cpp
     
LIB   = pictures.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))
