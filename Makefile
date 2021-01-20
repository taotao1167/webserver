ifeq ($(OS), Windows_NT)
	PLATFORM := "Windows"
else
	PLATFORM := "Linux"
endif

LIBNAME := lib/libhttpsvr.a

#source file
OBJS_C := tt_web.o tt_web_dispatch.o tt_file.o tt_buffer.o tt_malloc_debug.o \
	tt_sha1.o tt_base64.o tt_session.o tt_rbtree.o tt_handler.o tt_msgqueue.o

OBJS_CPP :=

#compile and lib parameter
CC      := gcc
CXX     := g++
LIBS    := -lpthread -levent -levent_openssl -lcrypto -lssl
DEFINES := -DWATCH_RAM -DWITH_IPV6 -DWITH_WEBSOCKET -DWITH_SSL
ifeq ($(PLATFORM), "Windows")
	LIBS    += -lws2_32
	LDFLAGS := -L/mingw64/lib
	INCLUDE := -I/mingw64/include
else
	LDFLAGS :=
	INCLUDE :=
endif

CFLAGS  := -g -Wall -pthread -O3 $(DEFINES) $(INCLUDE)
CXXFLAGS := $(CFLAGS)

app: ${LIBNAME} main.c
	$(CC) $(CFLAGS) main.c -Llib -lhttpsvr $(LIBS) -o $@

${LIBNAME}: ${OBJS_C} ${OBJS_CPP}
	ar -r $@ ${OBJS_C} ${OBJS_CPP}
	@cp *.h include/

${OBJS_C}: %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

${OBJS_CPP}: %.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@


clean:
	@rm -f app
	@rm -f ${LIBNAME}
	@rm -f *.o

