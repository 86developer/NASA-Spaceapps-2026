CXX ?= g++
CXXFLAGS ?= -std=c++17 -O3 -Wall -Wextra -Isrc

ifeq ($(OS),Windows_NT)
    LDFLAGS = -lws2_32
    TARGET = vitalgrid_daemon.exe
else
    LDFLAGS = -pthread
    TARGET = vitalgrid_daemon
endif

SRCS = src/main.cpp \
       src/engine/flight_surgeon.cpp \
       src/soma/soma_watchdog.cpp \
       src/network/websocket_server.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) $(LDFLAGS) -o $(TARGET)

clean:
	rm -f $(TARGET) *.o
