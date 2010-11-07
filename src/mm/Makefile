test:mm.o testmm.o
	gcc mm.o testmm.o -o test -Wall
mm.o: mm.h mm.c
	gcc mm.c -c -o mm.o -Wall
testmm.h: mm.h testmm.c
	gcc testmm.c -c -o testmm.c -Wall
clean:
	rm *.o
