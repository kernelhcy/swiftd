#include <stdlib.h>
#include <stdio.h>

#include "threadpool.h"

void * func(void *arg)
{
	debug_info("I am a job......... %d", (int)arg);
	int i , b = 2;
	for (i = 0; i < 99999999; ++i)
	{
		b*= b;
	}
	debug_info("Job Done......... %d", (int)arg);
	return NULL;
}


int main(int argc , char *argv[])
{
	thread_pool *tp = tp_init(2, 10);
	
//	sleep(1);
	int i = 1;
	for ( i = 1;i <= 10 ; ++i)
	{
		debug_info("try to run a job. %d", i);
		tp_run_job(tp, func, (void*)i);
	}
	sleep(2);
	debug_info("Start second.");
	for (;i <= 20 ; ++i)
	{
		debug_info("try to run a job. %d", i);
		tp_run_job(tp, func, (void*)i);
	}

	sleep(3);
	for (;i <= 40 ; ++i)
	{
		debug_info("try to run a job. %d", i);
		tp_run_job(tp, func, (void*)i);
	}
	debug_info("所有任务完成。");
	tp_free(tp);

	return 0;
}
