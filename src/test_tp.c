#include <stdlib.h>
#include <stdio.h>

#include "threadpool.h"

void * func(void *arg)
{
	int i = (int)arg;
	return NULL;
}


int main(int argc , char *argv[])
{
	thread_pool *tp = tp_init(2, 5);
	
	thread_job *job;
	
	int i = 1;
	for ( i = 1;i <= 10 ; ++i)
	{
		job = (thread_job*)malloc(sizeof(*job));
		job -> func = func;
		job -> ctx = (void *)i;
		debug_info("try to run a job. %d", i);
		tp_run_job(tp, job);
	}

	sleep(5);
	tp_free(tp);
	return 0;
}
