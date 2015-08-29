TARGET=Textures.xbt

IMAGES := $(shell find . -name "*jpg")
IMAGES += $(shell find . -name "*png")
IMAGES += $(shell find . -name "*gif")

all: $(TARGET)

.PHONY: force

$(TARGET): $(IMAGES)
ifeq (x86-osx,arm)
	/Users/Shared/mrmc-depends/buildtools-native/bin/TexturePacker -use_none -input . -output $(TARGET)
else
	/Users/Shared/mrmc-depends/buildtools-native/bin/TexturePacker -input . -output $(TARGET)

endif

clean:
	rm -f $(TARGET)
distclean: clean

