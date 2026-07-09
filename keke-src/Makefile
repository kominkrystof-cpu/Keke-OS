CXX = g++
CXXFLAGS = -static -std=c++17
TARGET = ../build/init
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	@mkdir -p ../build
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)
	@echo "Build complete! Run with: ../build/init"

clean:
	rm -f $(TARGET)
	@echo "Cleaned build artifacts"

run: $(TARGET)
	$(TARGET)
