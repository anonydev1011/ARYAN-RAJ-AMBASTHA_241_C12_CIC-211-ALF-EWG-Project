CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -Iinclude
SOURCES := main.cpp src/Sensors.cpp src/Alerts.cpp
TARGET := alf_ewg

.PHONY: all clean run
all: $(TARGET)

$(TARGET): $(SOURCES) include/DisasterCore.h
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	$(RM) $(TARGET) alf_ewg_incidents.bin
