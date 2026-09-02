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
.PHONY: all clean run