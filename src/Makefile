OBJS=buffer.o log.o  chunk.o connection.o array.o  configure.o fdevent.o threadpool.o\
	 fde_epoll.o joblist.o main.o network.o
FLAGS=-g
swiftd: ${OBJS}
	gcc ${OBJS} -o swiftd -lpthread

buffer.o: buffer.c
	gcc -c ${FLAGS} buffer.c -o buffer.o
chunk.o: chunk.c
	gcc -c ${FLAGS} chunk.c -o  chunk.o
connection.o:connection.c
	gcc -c ${FLAGS} connection.c  -o  connection.o
array.o: array.c
	gcc -c ${FLAGS} array.c  -o  array.o
log.o:log.c
	gcc -c ${FLAGS} log.c  -o  log.o
configure.o:configure.c
	gcc -c ${FLAGS} configure.c  -o configure.o
fdevent.o:fdevent.c
	gcc -c ${FLAGS} fdevent.c  -o fdevent.o
threadpool.o:threadpool.c
	gcc -c ${FLAGS} threadpool.c -o threadpool.o -lphtread 
fde_epoll.o:fde_epoll.c
	gcc -c ${FLAGS} fde_epoll.c -o  fde_epoll.o
joblist.o:joblist.c
	gcc -c ${FLAGS} joblist.c -o joblist.o
main.o:main.c
	gcc -c ${FLAGS} main.c -o main.o
network.o:network.c
	gcc -c ${FLAGS} network.c -o  network.o

clean:
	rm *.o
#	gcc -c ${FLAGS}   -o 
