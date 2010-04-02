CC=gcc

test : threadpool.o test_tp.o
	${CC} -g  threadpool.o test_tp.o -o test -lpthread -lrt
##使用clock_gettime必须加上库-lrt

test_tp.o : test_tp.c
	${CC} -g  -c  test_tp.c -o test_tp.o

threadpool.o : threadpool.c
	${CC} -g  -c threadpool.c -o threadpool.o -lpthread

clean:
	rm *.o
