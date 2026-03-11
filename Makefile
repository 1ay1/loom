CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -O2
INCLUDES = -Iinclude

SRC = $(shell find src -name "*.cpp")
OBJ = $(SRC:src/%.cpp=build/%.o)

TARGET = loom

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $(TARGET)

build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf build $(TARGET)
