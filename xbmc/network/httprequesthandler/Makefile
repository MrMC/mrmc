SRCS  = HTTPFileHandler.cpp
SRCS += HTTPImageHandler.cpp
SRCS += HTTPImageTransformationHandler.cpp
SRCS += HTTPJsonRpcHandler.cpp
SRCS += HTTPVfsHandler.cpp
SRCS += HTTPWebinterfaceAddonsHandler.cpp
SRCS += HTTPWebinterfaceHandler.cpp
SRCS += IHTTPRequestHandler.cpp

LIB   = httprequesthandlers.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))
