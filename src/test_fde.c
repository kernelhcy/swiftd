#include <stdlib.h>
#include <stdio.h>

#include "fdevent.h"

int main(int argc, char *argv[])
{
	fdevent *ev = fdevent_init(10, FDEVENT_HANDLER_EPOLL);
	return 0;
}
