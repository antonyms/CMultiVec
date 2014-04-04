
## Created by Anjuta

CC = g++
CFLAGS = -O2 -Wall -std=c++11 -g
OBJECTS = cmultivec.o
INCFLAGS =
LDFLAGS = -lboost_filesystem -lboost_system -lboost_program_options -g
LIBS = 

all: CMultiVec

CMultiVec: $(OBJECTS)
	$(CC) -o CMultiVec $(OBJECTS) $(LDFLAGS) $(LIBS)

.SUFFIXES:
.SUFFIXES:	.c .cc .C .cpp .cxx .o

.cxx.o :
	$(CC) -o $@ -c $(CFLAGS) $< $(INCFLAGS)

count:
	wc *.c *.cc *.C *.cpp *.h *.hpp

clean:
	rm -f *.o

.PHONY: all
.PHONY: count
.PHONY: clean
