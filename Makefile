CFLAGS = -Wall -Wextra -O1 -g
CXXFLAGS = $(CFLAGS)
CC = gcc
CXX = g++
##LIBS_PAPI = -lpapi
LDFLAGS = -Wl,-z,now

BINARY_TARGETS = rapl_baseline rapl_measure

all: $(BINARY_TARGETS)

clean:
	rm -f $(BINARY_TARGETS)

.PHONY: all clean

rapl_baseline: rapl_threads.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ -lrt -l

rapl_measure: rapl_test.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ -lrt -lm


msr-poll-gaps-nsec-and-power: msr-poll-gaps-nsec-and-power.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ -lrt

#https://devhints.io/makefile
#  $@   # "out.o" (target)
#  $<   # "src.c" (first prerequisite)
#  $^   # "src.c src.h" (all prerequisites)
#%.o: %.c
#  $*   # the 'stem' with which an implicit rule matches ("foo" in "foo.c")
# $(@D) # target directory
