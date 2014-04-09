
## Created by Anjuta

CC = g++
CFLAGS = -O2 -Wall -std=c++11 `pkg-config --cflags libxml-2.0`
EOBJECTS = cextractcontexts.o
COBJECTS = cclustercontexts.o
INCFLAGS =
LDFLAGS = -lboost_filesystem -lboost_system -lboost_program_options -lboost_iostreams -lmlpack
LIBS = 

all: CExtractContexts CClusterContexts

CExtractContexts: $(EOBJECTS)
	$(CC) -o CExtractContexts $(EOBJECTS) $(LDFLAGS) $(LIBS)

CClusterContexts: $(COBJECTS)
	$(CC) -o CClusterContexts $(COBJECTS) $(LDFLAGS) $(LIBS)

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
