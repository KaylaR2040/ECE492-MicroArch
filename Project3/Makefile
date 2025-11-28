CXX := g++
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra -pedantic
LDFLAGS :=
TARGET := sim
SRC := sim.cpp

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)
