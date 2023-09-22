UNAME=$(shell uname)
OPUS_VERSION=1.3.1
OPUSSRC=./libopus-build-scripts/opus-$(OPUS_VERSION)
CFLAGS=-O -fPIC -Wall -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux -I$(OPUSSRC)/include -I$(OPUSSRC)/src -I$(OPUSSRC)/celt -I$(OPUSSRC)/silk -I$(OPUSSRC)/silk/float -std=c99
LDLIBS=-lopus
LDFLAGS=-L$(OPUSSRC)/.libs
ifeq ($(UNAME),Darwin)
  LDFLAGS+=-shared
  LIBEXT=dylib
  LDLIBS+=-lc
  LD=gcc
  JAVAHOSTOS=darwin
else
  LDFLAGS+=-shared
  LIBEXT=so
  JAVAHOSTOS=linux
endif

CFLAGS+=-I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/$(JAVAHOSTOS) -I ./src

SRC_DIR=src
OBJ_DIR=obj
SRC_FILES=$(wildcard $(SRC_DIR)/*.c)
OBJ_FILES=$(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES))

 # Make sure the object file directory exists
$(shell mkdir -p $(OBJ_DIR))

 
.DEFAULT: libopusvad.$(LIBEXT)

libopusvad.$(LIBEXT): $(OBJ_FILES)
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)


$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

opusvadjava.o: CFLAGS+=-I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/$(JAVAHOSTOS)
 
libopusvadjava.$(LIBEXT): $(OBJ_FILES) ./obj/opusvadjava.o
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

debug: CFLAGS += -DDEBUG -g
debug: libopusvad.$(LIBEXT)

clean:
	rm -f *.$(LIBEXT) $(OBJ_DIR)/*.o
	rm -rf doc

doc:
	doxygen

.PHONY: clean doc
