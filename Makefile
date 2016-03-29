-include .config

.PHONY : all debug release clean


DBG_CPPFLAGS := -g2 -O0 -Wall -fPIC -D__DBG
REL_CPPFLAGS := -g0 -O2 -Wall -fPIC


INCLUDES := $(INC-y:%=-I%)
LIBRARY :=

MKDIR=mkdir
CXX := g++
PY=python
PIP=pip

CONFIG_ENTRY:=./config.json
CONFIG_TARGET:=./.config
CONFIG_AUTOGEN=./$(AUTOGEN_DIR)/autogen.h

DBG_OBJS=$(OBJ-y:%=$(DBG_OBJ_CACHE)/%.do)
REL_OBJS=$(OBJ-y:%=$(REL_OBJ_CACHE)/%.o)

AUTOGEN_DIR=autogen
DBG_OBJ_CACHE:=Debug
REL_OBJ_CACHE:=Release
TOOL_DIR:=tools

JCONFIGPY=$(TOOL_DIR)/jconfigpy/jconfigpy.py

DBG_TARGET:=libncd.a
REL_TARGET:=libnc.a

SILENT= $(DBG_TARGET) $(REL_TARGET) $(DBG_OBJ_CACHE) $(REL_OBJ_CACHE) $(DBG_OBJS) $(REL_OBJS) \
		debug release clean
.SILENT : $(SILENT)
VPATH:=$(SRC-y)

all : debug

config : $(AUTOGEN_DIR) $(JCONFIGPY)
	$(PY) $(JCONFIGPY) -c -i $(CONFIG_ENTRY) -o $(CONFIG_TARGET) -g $(CONFIG_AUTOGEN)
	
$(JCONFIGPY) : 
	$(PIP) install jconfigpy -t $(TOOL_DIR) 

debug : $(DBG_OBJ_CACHE) $(DBG_TARGET)

release : $(REL_OBJ_CACHE) $(REL_TARGET)

$(DBG_TARGET): $(DBG_OBJS)
	@echo 'archiving...$@'
	$(AR) rcs $@ $(DBG_OBJS)

$(REL_TARGET): $(REL_OBJS) 
	@echo 'archiving...$@'
	$(AR) rcs $@ $(REL_OBJS)

$(DBG_OBJ_CACHE)/%.do : %.cpp
	@echo 'compile...$@'
	$(CXX) -c -o $@ $(INCLUDES) $(LIBRARY) $(CPPFLAGS) $<
	
$(REL_OBJ_CACHE)/%.o : %.cpp
	@echo 'compile...$@'
	$(CXX) -c -o $@ $(INCLUDES) $(LIBRARY) $(CPPFLAGS) $<
	
$(DBG_OBJ_CACHE) $(REL_OBJ_CACHE) $(AUTOGEN_DIR) :
	$(MKDIR) $@

clean:
	@echo 'clean all'
	rm -rf $(REL_OBJ_CACHE) $(REL_TARGET) $(DBG_OBJ_CACHE) $(DBG_TARGET) \
			$(DBG_OBJS) $(REL_OBJS) 
			
config_clean :
	@echo 'clean configuration'
	rm -rf $(AUTOGEN_DIR) $(CONFIG_TARGET) $(CONFIG_AUTOGEN)

