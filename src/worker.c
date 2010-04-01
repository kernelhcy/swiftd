#include "worker.h"
#include "log.h"

void* worker_main(void *arg)
{
	if (NULL == arg)
	{

		return NULL;
	}
}


worker* worker_init()
{
	worker* wrk = NULL;
	wkr = (worker *)calloc(1, sizeof(*wkr));
	if(NULL == wkr)
	{
		fprintf(stderr, "(%s %d) Can not initial worker.\n", __FILE__, __LINE__);
	}

	return wrk;
}

void worker_free(worker *wkr)
{
	free(wkr);
}

