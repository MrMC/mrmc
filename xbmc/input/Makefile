SRCS  = ButtonTranslator.cpp
SRCS += InertialScrollingHandler.cpp
SRCS += InputCodingTableBaiduPY.cpp
SRCS += InputCodingTableBasePY.cpp
SRCS += InputCodingTableFactory.cpp
SRCS += InputCodingTableKorean.cpp
SRCS += KeyboardLayout.cpp
SRCS += KeyboardLayoutManager.cpp
SRCS += KeyboardStat.cpp
SRCS += MouseStat.cpp
SRCS += SDLJoystick.cpp
SRCS += XBMC_keytable.cpp
SRCS += InputManager.cpp
SRCS += Key.cpp

LIB   = input.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))
