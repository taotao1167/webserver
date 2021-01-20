default_target: static_lib

TARGETNAME := static_lib

#source file
OBJS_C := sqlite3.o

OBJS_CPP :=

#compile and lib parameter
CC      := gcc
CXX     := g++
LIBS    :=
LDFLAGS :=
DEFINES :=
INCLUDE :=

CFLAGS  := -g -Wall -O3 $(DEFINES) $(INCLUDE)
CXXFLAGS := $(CFLAGS)

${OBJS_C}: %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

${OBJS_CPP}: %.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGETNAME): ${OBJS_C} ${OBJS_CPP}
	ar -r libsqlite3.a $(OBJS_C) ${OBJS_CPP}
	@echo "\nBuild success!"

clean:
	rm -f *.o
	rm -f libsqlite3.a

