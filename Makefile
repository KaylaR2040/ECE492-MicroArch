# build: creates 'sim' at repo root
CXX := g++
CXXFLAGS := -std=c++14 -O3 -Wall -Wextra -Wpedantic

SRC := src/main.cc src/cache.cc src/utils.cc
OBJ := $(SRC:.cc=.o)

all: sim

sim: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

src/%.o: src/%.cc src/%.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# headers without .cc partners
src/main.o: src/sim.h
src/cache.o: src/sim.h
src/utils.o: src/sim.h src/cache.h

clean:
	rm -f src/*.o sim
.PHONY: all clean
