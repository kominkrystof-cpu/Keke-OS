CXX = g++
CXXFLAGS = -static -std=c++17

CC = gcc
CFLAGS = -static -O2 -Wall

TARGET = ../build/init
C_TARGETS = ../build/keke_info ../build/keketool

.PHONY: all clean run

all: $(TARGET) $(C_TARGETS)

$(TARGET): main.cpp kekeos.h keke.h
	@mkdir -p ../build
	$(CXX) $(CXXFLAGS) main.cpp -o $(TARGET)
	@echo "C++ init built: $(TARGET)"

../build/keke_info: keke_info.c kekeos.h keke.h
	$(CC) $(CFLAGS) keke_info.c -o ../build/keke_info
	@echo "C program built: ../build/keke_info"

../build/keketool: keketool.c kekeos.h keke.h
	$(CC) $(CFLAGS) keketool.c -o ../build/keketool
	@echo "C program built: ../build/keketool"

clean:
	rm -f $(TARGET) $(C_TARGETS)
	@echo "Cleaned build artifacts"

run: $(TARGET)
	$(TARGET)
