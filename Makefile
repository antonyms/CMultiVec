ENABLE_HALITE ?= 1
ifeq ($(ENABLE_HALITE), 1)
    CFLAGS = -DENABLE_HALITE -Ihalite/include `pkg-config --cflags opencv`
    LDFLAGS = halite/libhalite.a `pkg-config --libs opencv` -llmdb
endif


CC = g++
CFLAGS += -g -O3 -Wall -std=c++11 `pkg-config --cflags libxml-2.0`
IOBJECTS = cindexcorpus.o common.o
EOBJECTS = cextractcontexts.o common.o
COBJECTS += cclustercontexts.o common.o
VOBJECTS = cexpandvocab.o common.o
ROBJECTS = crelabelcorpus.o common.o
INCFLAGS =
LDFLAGS += -lboost_filesystem -lboost_system -lboost_program_options -lboost_iostreams -lmlpack -larmadillo -O3
LIBS = 

all: CIndexCorpus CExtractContexts CClusterContexts CExpandVocab CRelabelCorpus

CIndexCorpus: $(IOBJECTS)
	$(CC) -o CIndexCorpus $(IOBJECTS) $(LDFLAGS) $(LIBS)

CExtractContexts: $(EOBJECTS)
	$(CC) -o CExtractContexts $(EOBJECTS) $(LDFLAGS) $(LIBS)

CClusterContexts: $(COBJECTS)
	$(CC) -o CClusterContexts $(COBJECTS) $(LDFLAGS) $(LIBS)

CExpandVocab: $(VOBJECTS)
	$(CC) -o CExpandVocab $(VOBJECTS) $(LDFLAGS) $(LIBS)

CRelabelCorpus: $(ROBJECTS)
	$(CC) -o CRelabelCorpus $(ROBJECTS) $(LDFLAGS) $(LIBS)

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
