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
	$(CPP) -o $@  $^ -pg $(LIBRARY)

.cpp.o : $(SOURCES)
	$(CPP) $(SOURCES) $(CPPFLAGS) $(INCLUDES) $(LIBRARY)

clean :
	rm -rf $(OBJECTS) $(TARGET) *~ ./client

debug : CPPFLAGS := -g -pg -c -Wall -std=c++11 -fopenmp -DRANDOM_PKT_LOSS
debug : $(TARGET)

release : CPPFLAGS := -O0 -c -Wall -std=c++11 -fopenmp
release : $(TARGET)

