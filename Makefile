.PHONY : all debug release backup
.SUFFIXES : .cpp .o

SOURCES  := $(wildcard *.cpp)
INCLUDES := 
OBJECTS  := $(SOURCES:.cpp=.o)
LIBRARY := -lpthread
CPP := g++
#CPP := arm-linux-gnueabihf-g++
TARGET = test

all : debug

$(TARGET) : $(OBJECTS)
	$(CPP) -o $@  $^ $(LIBRARY)

.cpp.o : $(SOURCES)
	$(CPP) $(SOURCES) $(CPPFLAGS) $(INCLUDES) $(LIBRARY)

clean :
	rm -rf $(OBJECTS) $(TARGET) *~ gmon.out

debug : CPPFLAGS := -g -c -Wall -std=c++11
debug : $(TARGET)

release : CPPFLAGS := -O3 -c -Wall -std=c++11
release : $(TARGET)
#	./$(TARGET)
#	gprof ./$(TARGET) gmon.out
