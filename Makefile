
## Created by Anjuta

CC = g++
CFLAGS = -O2 -Wall -std=c++11
OBJECTS = cextractcontexts.o
INCFLAGS =
LDFLAGS = -lboost_filesystem -lboost_system -lboost_program_options
LIBS = 

all: CExtractContexts

CExtractContexts: $(OBJECTS)
	$(CC) -o CExtractContexts $(OBJECTS) $(LDFLAGS) $(LIBS)

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
