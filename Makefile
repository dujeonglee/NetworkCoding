.PHONY : all debug release
.SUFFIXES : .cpp .o

SOURCES := $(wildcard *.cpp)
INCLUDES :=
OBJECTS := $(SOURCES:.cpp=.o)
LIBRARY :=
CPP := g++

TARGET = libnc.a

all : debug

$(TARGET) : $(OBJECTS)
	$(AR) rs $@ $^

.cpp.o : $(SOURCES)
	$(CPP) $(CPPFLAGS) $(INCLUDES) $(SOURCES)

clean:
	rm -rf $(OBJECTS) $(TARGET) *~

debug : CPPFLAGS := -g -c -Wall -fPIC
debug : $(TARGET)

release : CPPFLAGS := -O0 -c -Wall -fPIC
release : $(TARGET)

