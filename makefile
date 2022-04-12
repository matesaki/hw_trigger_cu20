CXX = g++
CFLAGS = -g -Wall
LIBS = -ludev

SOURCES = main.cpp
HEADS =
#OBJECTS=$(SOURCES:.cpp=.o)

TARGET = hwTrigger

ALL: clean $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(CFLAGS) $(SOURCES) $(LIBS) -o $(TARGET)
	@ # gcc Trigger_Mode.c -ludev -o Trigger_Mode
	@ echo "\nDONE, run with:   sudo ./$(TARGET) on"

.c.o:
	$(CXX) $(CFLAGS) $(DEFINES) $(LIBS) $(HEADS) -c $< -o $@

c: clean
clean:
	$(RM) $(TARGET) *.o *~

install:
	sudo apt install -y libudev-dev

# Number of available cameras
number:
	lsusb | grep "2560:c120" | wc
