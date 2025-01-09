CC=gcc
#ARGS = -c -g -pg -p -Wall -D_XOPEN_SOURCE=600 -D_LARGEFILE64_SOURCE=1 -pthread -fpic
ARGS := -c -g -D_XOPEN_SOURCE=600 -D_LARGEFILE64_SOURCE=1 -pthread -I$(PWD)
#ARGS+=-I/home/kmehta/plfs-trunk/installation/include \
			-L/home/kmehta/plfs-trunk/installation/lib -lplfs

OBJS := $(patsubst %.c,%.o,$(wildcard *.c))

all	: static

static: $(OBJS)
	ar rcs libpcollio.a $(OBJS)
	mv libpcollio.a ../lib/.
	#mv $(OBJS) objs/.
	cp defs.h debug_info.h ompc_barrier.h omp_io_interfaces.h fs_table.h pthread_coll_io_base.h ../include/.

%.o : %.c
	$(CC) $(ARGS) -o $@ $^

shared : $(OBJS)
	$(CC) -shared -W1,-soname,libpcollio.so.1 -o libpcollio.so.1.0.1 $(OBJS)
	mv $(OBJS) objs/.
	mv libpcollio.so.1.0.1 ../lib/.

clean :
	rm -f libpcollio.a *.o 

cleanall: clean
	rm -f ../include/*h ../lib/lib*
