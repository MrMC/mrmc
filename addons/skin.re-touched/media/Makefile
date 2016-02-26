TARGET=Textures.xbt

IMAGES := $(shell find . -name "*jpg")
IMAGES += $(shell find . -name "*png")
IMAGES += $(shell find . -name "*gif")

all: $(TARGET)

.PHONY: force

$(TARGET): $(IMAGES)
ifeq (arm-osx,arm)
	/Users/Shared/mrmc2-depends/buildtools-native/bin/TexturePacker -dupecheck -use_none -input . -output $(TARGET)
else
	/Users/Shared/mrmc2-depends/buildtools-native/bin/TexturePacker -dupecheck -input . -output $(TARGET)

endif

clean:
	rm -f $(TARGET)
distclean: clean

