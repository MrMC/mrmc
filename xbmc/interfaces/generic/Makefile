SRCS  = ILanguageInvoker.cpp
SRCS += LanguageInvokerThread.cpp
SRCS += ScriptInvocationManager.cpp

LIB   = interfaces-generic.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))
