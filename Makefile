CXX := g++
CXXFLAGS := -std=c++14 -O3 -Wall -Wextra -Wpedantic

SRC := src/main.cc src/cache.cc src/utils.cc
OBJ := $(SRC:.cc=.o)

all: sim

sim: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

src/%.o: src/%.cc src/%.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

src/main.o: src/main.cc src/sim.h src/cache.h src/utils.h
src/cache.o: src/cache.cc src/cache.h src/sim.h
src/utils.o: src/utils.cc src/utils.h src/sim.h src/cache.h

clean:
	rm -f src/*.o sim
