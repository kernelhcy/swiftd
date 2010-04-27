#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

typedef struct 
{
	int test;
}s;

int main( void )
{
#ifndef __ASSEMBLER__
        printf( "Undefine __ASSEMBLER__\n" );
#else
        printf( "define __ASSEMBLER__\n" );
#endif

#ifndef __LIBC
        printf( "Undefine __LIBC\n" );
#else
        printf( "define __LIBC\n" );
#endif

#ifndef _LIBC_REENTRANT
        printf( "Undefine _LIBC_REENTRANT\n" );
#else
        printf( "define _LIBC_REENTRANT\n" );
#endif
		printf("errno:%d", errno);
		s* p = malloc(sizeof(*p));
		free(p);
		free(p);
		free(p);
        return 0;
}
