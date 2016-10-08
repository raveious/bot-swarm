CCFLAGS = -Wall
GCC = g++
TARGET = swarm
OBJECTS = main.o networking.o

$(TARGET): $(OBJECTS)
	$(GCC) $(CCFLAGS) -o $(TARGET) -g $(OBJECTS)

.cpp.o:
	$(GCC) $(CCFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJECTS)
