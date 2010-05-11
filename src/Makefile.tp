CC=gcc
OBJPATH=../objects

test : ${OBJPATH}/threadpool.o ${OBJPATH}/test_tp.o
	${CC} -ggdb  ${OBJPATH}/threadpool.o ${OBJPATH}/test_tp.o -o test -lpthread -lrt
##使用clock_gettime必须加上库-lrt

${OBJPATH}/test_tp.o : test_tp.c
	${CC} -ggdb  -c  test_tp.c -o ${OBJPATH}/test_tp.o

${OBJPATH}/threadpool.o : threadpool.c
	${CC} -ggdb  -c threadpool.c -o ${OBJPATH}/threadpool.o -lpthread

clean:
	rm ${OBJPATH}/*.o test
