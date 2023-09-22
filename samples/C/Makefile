UNAME=$(shell uname)
OPUS_VERSION=1.3.1
OPUS_SRC=../../libopus-build-scripts/opus-$(OPUS_VERSION)
CFLAGS=-O -fPIC -Wall -std=c99 -I../../src -Wall -std=c99
LDFLAGS=-L$(OPUS_SRC)/.libs

OS := $(shell lsb_release -si 2>/dev/null || cat /etc/os-release 2>/dev/null | grep ^NAME | cut -d= -f2 | sed 's/\"//g')
ifeq ($(UNAME),Darwin)
	LDFLAGS+=-L../../dist/mac
	LDLIBS=-lopusvad -lopus
else
ifeq ($(OS),Ubuntu)
	LDFLAGS+=-L../../dist/ubuntu
else ifeq ($(OS),CentOS)
	LDFLAGS+=-L../../dist/centos
else
endif
	LDLIBS=../../obj/dequeue.o ../../obj/opusvad.o ../../obj/adpcm.o $(OPUS_SRC)/.libs/libopus.a -luuid -lm
endif

ifdef USEMUSL
  CC=musl-gcc
  CFLAGS+=-static
  LDLIBS=../../obj/opusvad.o $(OPUS_SRC)/.libs/libopus.a
endif

opusvadtool:

clean:
	rm -f opusvadtool opusvadtool.o

.PHONY: clean
