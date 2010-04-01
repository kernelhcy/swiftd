#include <stdlib.h>
#include <stdio.h>

#include "threadpool.h"

void * func(void *arg)
{
	fprintf(stderr, "A job. id %d\n", arg);
	int i, b = 2;
	for (i = 0; i < 199999999; ++i)
	{
		b *= b;
	}
	fprintf(stderr, "A job. id %d Over \n", arg);

	return NULL;
}


int main(int argc , char *argv[])
{
	thread_pool *tp = tp_init(2, 10);
	
	thread_job job;
	job.func = func;
	
	int i = 0;
	for (;i < 20 ; ++i)
	{
		job.ctx = (void *)i;
		tp_run_job(tp, &job);
	}

	tp_free(tp);
	return 0;
}
