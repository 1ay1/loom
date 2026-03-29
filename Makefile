CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -O2
INCLUDES = -Iinclude

# Auto-dependency generation: -MMD emits a .d file per .o listing
# every header that .cpp included. -MP adds phony targets for each
# header so deleting a header doesn't break the build.
DEPFLAGS = -MMD -MP

SRC = $(shell find src -name "*.cpp")
OBJ = $(SRC:src/%.cpp=build/%.o)
DEP = $(OBJ:.o=.d)

TARGET = loom

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $(TARGET) -lz -lpthread

build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEPFLAGS) -c $< -o $@

clean:
	rm -rf build $(TARGET)

# Pull in the generated dependency files (silently skip if missing,
# e.g. on first build before any .d files exist).
-include $(DEP)
