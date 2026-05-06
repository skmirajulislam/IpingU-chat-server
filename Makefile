CXX = clang++
CXXFLAGS = -std=c++17 -pthread -Wall -Os
INCLUDE_DIR = include
SRC_DIR = src
BUILD_DIR = build

SOURCES = $(SRC_DIR)/main.cpp $(SRC_DIR)/handlers.cpp $(SRC_DIR)/utils.cpp $(SRC_DIR)/models.cpp
OBJECTS = $(BUILD_DIR)/main.o $(BUILD_DIR)/handlers.o $(BUILD_DIR)/utils.o $(BUILD_DIR)/models.o
TARGET = server

.PHONY: all clean run rebuild

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@
	strip $@
	@echo "✓ Build successful: $(TARGET) ($$(wc -c < $(TARGET) | tr -d ' ') bytes)"

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
	@echo "✓ Cleaned build artifacts"

run: $(TARGET)
	./$(TARGET)

rebuild: clean all
