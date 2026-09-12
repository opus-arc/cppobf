#
# COBF
# Makefile for gcc 3.4.4
#
#
CFLAGS = -Wall -O6 -fomit-frame-pointer -Dunix -I ../include
CCFLAGS = -Wall -O6 -fomit-frame-pointer -Dunix -I ../include
LDFLAGS = -lstdc++

AS	=as
LD	=gcc $(LDFLAGS)
CC	=gcc $(CFLAGS)
CCC	=g++ $(CCFLAGS)


all: cobf

lex_yy.cpp: cobf.l
	flex -t cobf.l >lex_yy.cpp

lex_yy.o: scanproc.h

BLIB_SRC     = b_alloc.cpp b_array.cpp b_cont.cpp b_str.cpp b_obj.cpp b_set.cpp
BLIB_OBJECTS = b_alloc.o  b_array.o  b_cont.o  b_str.o  b_obj.o  b_set.o

COBF_SRC     = main.cpp cobf_lex.cpp lib.cpp $(BLIB_SRC)
COBF_OBJECTS = main.o  cobf_lex.o  lib.o  $(BLIB_OBJECTS)

.c.o:
	$(CC) -c -o $*.o $<

.cc.o:
	$(CCC) -c -o $*.o $<

.cpp.o:
	$(CCC) -c -o $*.o $<

cobf:	$(COBF_OBJECTS)
	$(LD) -o $@ $(COBF_OBJECTS) $(LDFLAGS)

clean:
	-rm *.o
	-rm cobf
	-rm cobf.exe

cobfx:
	cat $(COBF_SRC) >cobf.cc
	$(CCC) -c -o cobf.o cobf.cc
	$(LD) -o cobfx cobf.o -s $(LDFLAGS)
#	strip cobfx
#	mv cobfx ../bin/linux/cobf
