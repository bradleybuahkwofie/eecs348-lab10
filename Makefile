# String-Double Calculator (EECS 348)
CXX = g++
CXXFLAGS = -std=gnu++17 -O2 -Wall -Wextra -Wpedantic

TARGET = calc
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	$(RM) $(TARGET)
