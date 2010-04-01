#include "threadpool.h"

#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>


static void * thread_main(void *arg)
{
	thread_info *info = (thread_info*)arg;
	if (NULL == info)
	{
		return NULL;
	}
	
	debug_info("Create a thread. %d", info -> id);
	//变为守护线程
	pthread_detach(pthread_self());

	while(1)
	{
		pthread_mutex_lock(&info -> lock);
		debug_info("Thread %d is waitting for a job.", info -> id);
		pthread_cond_wait(&info -> cond, &info -> lock);
		debug_info("Thread %d get a job.", info -> id);
		pthread_mutex_unlock(&info -> lock);

		debug_info("Thread %d. Run a job.", info -> id);
		//运行作业
		info -> job -> func(info -> job -> ctx);
		debug_info("Thread %d. Finish a job.", info -> id);

		pthread_mutex_lock(&info -> lock);
		info -> is_busy = 0;
		pthread_mutex_unlock(&info -> lock);

		sem_post(&info -> tp -> idle_thread);
		debug_info("Thread %d is idle.", info -> id);
	}

	return NULL;
}

static void * manage_thread(void *arg)
{
	return NULL;
}

/**
 * 创建和释放线程池
 */
thread_pool* tp_init(int minnum, int maxnum)
{
	thread_pool *tp = (thread_pool *)malloc(sizeof(*tp));
	if (NULL == tp)
	{
		return NULL;
	}
	debug_info("Initial the thread pool.");

	tp -> max_num = maxnum;
	tp -> min_num = minnum;
	
	pthread_mutex_init(&tp -> lock, NULL);

	sem_init(&tp -> idle_thread, 0, maxnum); //信号量不跨进程。

	//创建管理线程。
	if (0 != pthread_create(&tp -> manage_thread_id , NULL, manage_thread, tp))
	{
		return NULL;
	}

	tp -> threads = (thread_info *)calloc(maxnum, sizeof(thread_info));
	if (NULL == tp -> threads)
	{
		return NULL;
	}

	//初始线程。
	int i = 0; 
	for (; i < minnum; ++i)
	{
		if (0 != pthread_create(&tp -> threads[i].id, NULL, thread_main, &tp -> threads[i]))
		{
			fprintf(stderr, "(%s %d)Create thread error.\n", __FILE__, __LINE__);
			tp -> threads[i].id = 0;
			continue;
		}
		pthread_mutex_init(&tp -> threads[i].lock, NULL);
		pthread_cond_init(&tp -> threads[i].cond, NULL);
		tp -> threads[i].is_busy = 0;
		tp -> threads[i].tp = tp;
	}

	tp -> cur_num = minnum;
	debug_info("Initail thread pool over. Max %d, Min %d", maxnum, minnum);
	return tp;

}
void tp_free(thread_pool *tp)
{
	if (NULL == tp)
	{
		return;
	}
	
	debug_info("Kill all thread.");
	pthread_kill(tp -> manage_thread_id, SIGKILL);
	int i;
	for (i = 0; i < tp -> cur_num; ++i)
	{
		if (0 != tp -> threads[i].id)
		{
			//pthread_kill(tp -> threads[i].id, SIGKILL);
			pthread_mutex_destroy(&tp -> threads[i].lock);
			pthread_cond_destroy(&tp -> threads[i].cond);
		}
	}

	debug_info("Free thread pool.");
	pthread_mutex_destroy(&tp -> lock);
	sem_destroy(&tp -> idle_thread);
	free(tp -> threads);
	free(tp);
	return;
}

/*
 * 从线程池中获得一个线程，如果没有空闲的线程，则创建一个。
 * 成功，则返回线程在tp->threads中的下标
 * 否则返回-1：线程数达到上限。-2创建线程出错。
 */
static int tp_get_thread(thread_pool *tp)
{
	if (NULL == tp)
	{
		return -1;
	}
	debug_info("Try to get an idle thread.");
	
	sem_wait(&tp -> idle_thread);

	int i;
	for (i = 0; i < tp -> cur_num; ++i)
	{
		pthread_mutex_lock(&tp -> threads[i].lock);
		if (!tp -> threads[i].is_busy)
		{
			tp -> threads[i].is_busy = 1;
			debug_info("Got a thread. index : %d id %d", i, tp -> threads[i].id);
			pthread_mutex_unlock(&tp -> threads[i].lock);
			return i;
		}
		pthread_mutex_unlock(&tp -> threads[i].lock);
	}

	/*
	 * 没有空闲的线程，创建一个。
	 */

	pthread_mutex_lock(&tp -> lock);
	//达到最大线程数。
	if (tp -> cur_num >= tp -> max_num)
	{
		debug_info("Maxnum attached.");
		pthread_mutex_unlock(&tp -> lock);
		return -1;
	}

	debug_info("All thread is busy. ok , create a new one.");
	if (0 != pthread_create(&tp -> threads[tp -> cur_num].id, NULL,
				thread_main, &tp -> threads[tp -> cur_num]))
	{
		pthread_mutex_unlock(&tp -> lock);
		return -2;
	}

	pthread_mutex_init(&tp -> threads[tp -> cur_num].lock, NULL);
	pthread_cond_init(&tp -> threads[tp -> cur_num].cond, NULL);
	tp -> threads[tp -> cur_num].is_busy = 0;

	int id = tp -> cur_num;
	tp -> cur_num ++;
	tp -> threads[id].is_busy = 1;
	pthread_mutex_unlock(&tp -> lock);

	return id;
}

/*
 * 从线程池中分配一个线程处理作业。
 */
void tp_run_job(thread_pool *tp, thread_job *job)
{
	if(NULL == tp || NULL == job)
	{
		return;
	}

	int id;
	while( (id = tp_get_thread(tp)) < 0);
	tp -> threads[id].job = job;
	debug_info("Tell thread %d to work. index %d", tp -> threads[id].id, id);
	pthread_cond_signal(&tp -> threads[id].cond);
	debug_info("Ok. Tell the thread done.");
	
	return;
}


//static pthread_mutex_t debug_lock = PTHREAD_MUTEX_INITIALIZER;
void debug_info(const char *fmt, ...)
{
	//pthread_mutex_lock(&debug_lock);
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	//pthread_mutex_unlock(&debug_lock);
	return;
}
