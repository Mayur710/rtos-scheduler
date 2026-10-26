CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude

#directories 
SRC_DIR := src
BUILD_DIR := build

#source files
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRCS))
#output binary name
TARGET := rtos_scheduler
#default target
all : $(TARGET)

#linking all objects in final executable
$(TARGET) : $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

#compile source files to object files
$(BUILD_DIR)/%.o : $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

#remove build artifacts
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

#build and immediately run the program
run: all
	./$(TARGET)

#unit / regression tests (no external framework)
TEST_TARGET := $(BUILD_DIR)/test_scheduler
$(TEST_TARGET): tests/test_scheduler.cpp $(SRC_DIR)/Task.cpp $(SRC_DIR)/Scheduler.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

test: $(TEST_TARGET)
	./$(TEST_TARGET)

#starvation before/after comparison (aging on vs aging off)
COMPARE_TARGET := $(BUILD_DIR)/compare_aging
$(COMPARE_TARGET): tools/compare_aging.cpp $(SRC_DIR)/Task.cpp $(SRC_DIR)/Scheduler.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

compare: $(COMPARE_TARGET)
	./$(COMPARE_TARGET)

.PHONY: all clean run test compare