CXX := g++
CXXFLAGS := -std=c++14 -O3 -Wall -Wextra -Wpedantic

SRC_DIR := src
OBJ := $(SRC_DIR)/main.o $(SRC_DIR)/cache.o $(SRC_DIR)/utils.o
DEPS := $(SRC_DIR)/sim.h $(SRC_DIR)/cache.h $(SRC_DIR)/utils.h

.PHONY: all clean
all: sim

sim: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cc $(DEPS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(SRC_DIR)/*.o sim
