CC=gcc

test : threadpool.o test_tp.o
	${CC} threadpool.o test_tp.o -o test -lpthread

test_tp.o : test_tp.c
	${CC} -c  test_tp.c -o test_tp.o

threadpool.o : threadpool.c
	${CC} -c threadpool.c -o threadpool.o -lpthread

clean:
	rm *.o
