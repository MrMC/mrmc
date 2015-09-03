SRCS  = WebSocket.cpp
SRCS += WebSocketManager.cpp
SRCS += WebSocketV8.cpp
SRCS += WebSocketV13.cpp

LIB   = websocket.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))
