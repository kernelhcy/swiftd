#include "threadpool.h"

#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

/**
 * 工作线程的主函数。
 * 等待条件变量，然后调用作业函数。
 */
static void * thread_main(void *arg)
{
	thread_info *info = (thread_info*)arg;
	if (NULL == info)
	{
		return NULL;
	}

	
	debug_info("Create a thread. %d", info -> id);
	//变为守护线程
	//pthread_detach(pthread_self());

	while(1)
	{
		if (info -> stop)
		{
			break;;
		}
		//debug_info("Thread %d is waitting for a job.", info -> id);

		pthread_mutex_lock(&info -> lock);
		while(!info -> is_busy && ! info -> stop)
		{
			pthread_cond_wait(&info -> cond, &info -> lock);
		}
		pthread_mutex_unlock(&info -> lock);

		if (info -> stop)
		{
			break;
		}

		//运行作业
		if (info -> job)
		{
			info -> job -> func(info -> job -> ctx);
		}
			
		//debug_info("Thread %d. Finish a job.", info -> id);

		pthread_mutex_lock(&info -> lock);
		info -> is_busy = 0;
		sem_post(&info -> tp -> idle_thread);
		pthread_mutex_unlock(&info -> lock);

		debug_info("Thread %d is idle.", info -> id);
	}

	return NULL;
}

/**
 * 管理进程。
 * 每搁一秒钟检查工作线程的情况。
 * 如果空闲线程较多，则销毁一部分线程节约资源。
 */
static void * manage_thread(void *arg)
{
	thread_pool *tp = (thread_pool *)arg;
	if (NULL == tp)
	{
		return NULL;
	}

	pthread_detach(pthread_self());

	int idle_cnt; 	//空闲线程的数量。
	int i;
	
	while(1)
	{
		idle_cnt = 0;
		//统计空闲线程的数量。
		for (i = 0; i < tp -> cur_num; ++i)
		{
			pthread_mutex_lock(&tp -> threads[i].lock);
			if (!tp -> threads[i].stop && !tp -> threads[i].is_busy)
			{
				++idle_cnt;
			}
			pthread_mutex_unlock(&tp -> threads[i].lock);
		}

		debug_info("Idle thread num : %d", idle_cnt);
		/*
		 * 如果空闲的线程占到所有线程的50%以上，那么将空闲
		 * 线程的数量控制在50%以内。
		 */
		if ((float)idle_cnt / (float)tp -> cur_num > 0.5)
		{
			int need_stop_cnt = idle_cnt * 2 - tp -> cur_num - 1;
			debug_info("Need stop %d threads", need_stop_cnt);
			int del;
			for(i = 0; i < tp -> cur_num && need_stop_cnt > 0; ++i)
			{
				del = 0;
				pthread_mutex_lock(&tp -> threads[i].lock);
				if (!tp -> threads[i].stop && !tp -> threads[i].is_busy)
				{
					debug_info("Stop thread id %d", i);
					tp -> threads[i].stop = 1;
					pthread_cond_signal(&tp -> threads[i].cond);
					--need_stop_cnt;
					del = 1;
				}
				pthread_mutex_unlock(&tp -> threads[i].lock);

				if(del)
				{
					pthread_mutex_lock(&tp -> lock);
					int_node *n = (int_node*)malloc(sizeof(*n));
					if(NULL != n)
					{
						n -> id = i;
						n -> next = tp -> unused;
						tp -> unused = n;
					}
					pthread_mutex_unlock(&tp -> lock);
				}
			}
			debug_info("Clear thread over.");
		}

		sleep(1);

	}

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

	tp -> max_num = maxnum;
	tp -> min_num = minnum;
	tp -> unused = NULL;
	
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
		//先初始化数据，再启动线程。
		pthread_mutex_init(&tp -> threads[i].lock, NULL);
		pthread_cond_init(&tp -> threads[i].cond, NULL);
		tp -> threads[i].is_busy = 0;
		tp -> threads[i].stop = 0;
		tp -> threads[i].tp = tp;

		if (0 != pthread_create(&tp -> threads[i].id, NULL, thread_main, &tp -> threads[i]))
		{
			fprintf(stderr, "(%s %d)Create thread error.\n", __FILE__, __LINE__);
			tp -> threads[i].id = 0;
			continue;
		}
	}

	tp -> cur_num = minnum;
	return tp;

}
void tp_free(thread_pool *tp)
{
	if (NULL == tp)
	{
		return;
	}
	
	pthread_cancel(tp -> manage_thread_id);

	int i;
	//标记所有的线程终止。
	for (i = 0; i < tp -> cur_num; ++i)
	{
		if (0 != tp -> threads[i].id)
		{
			pthread_mutex_lock(&tp -> threads[i].lock);
			tp -> threads[i].stop = 1;
			pthread_mutex_unlock(&tp -> threads[i].lock);
		}
	}
	//等待线程退出。
	for (i = 0; i < tp -> cur_num; ++i)
	{
		if (0 != tp -> threads[i].id)
		{
			pthread_mutex_lock(&tp -> threads[i].lock);
			pthread_cond_signal(&tp -> threads[i].cond);
			pthread_mutex_unlock(&tp -> threads[i].lock);
			//等待线程终止
			pthread_join(tp -> threads[i].id, NULL);
			//销毁线程的数据。
			pthread_mutex_destroy(&tp -> threads[i].lock);
			pthread_cond_destroy(&tp -> threads[i].cond);
		}
	}

	//debug_info("Free thread pool.");
	pthread_mutex_destroy(&tp -> lock);
	sem_destroy(&tp -> idle_thread);
	free(tp -> threads);

	int_node *n = tp -> unused, *tmp;
	while(NULL != n)
	{
		tmp = n;
		n = n -> next;
		free(tmp);
	}
	
	free(tp);
	return;
}

/*
 * 从线程池中分配一个线程处理作业。
 * 成功返回0,否则返回小于0.
 */
int tp_run_job(thread_pool *tp, thread_job *job)
{
	if(NULL == tp || NULL == job)
	{
		return;
	}
	
	//寻找一个空闲的线程。
	sem_wait(&tp -> idle_thread);

	int i;
	for (i = 0; i < tp -> cur_num; ++i)
	{
		pthread_mutex_lock(&tp -> threads[i].lock);
		if (! tp -> threads[i].stop && !tp -> threads[i].is_busy)
		{
			tp -> threads[i].is_busy = 1;
			tp -> threads[i].job = job;

			debug_info("Got a thread. index : %d id %d", i, tp -> threads[i].id);

			pthread_cond_signal(&tp -> threads[i].cond);

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

	//是否有空闲。
	int ndx;
	if (tp -> unused != NULL)
	{
		int_node *tmp;
		ndx = tp -> unused -> id;
		tmp = tp -> unused;
		tp -> unused = tp -> unused -> next;
		free(tmp);
	}
	else
	{
		ndx = tp -> cur_num;
		++tp -> cur_num;
	}
	//先要初始化线程的数据在启动线程。
	//否则出错。
	tp -> threads[ndx].is_busy = 1;
	tp -> threads[ndx].stop = 0;
	tp -> threads[ndx].tp = tp;
	tp -> threads[ndx].job = job;
	pthread_mutex_init(&tp -> threads[ndx].lock, NULL);
	pthread_cond_init(&tp -> threads[ndx].cond, NULL);

	if (0 != pthread_create(&tp -> threads[ndx].id, NULL,
				thread_main, &tp -> threads[ndx]))
	{
		--tp->cur_num;
		pthread_mutex_unlock(&tp -> lock);
		return -2;
	}

	pthread_mutex_unlock(&tp -> lock);

	return 0;
}


static pthread_mutex_t debug_lock = PTHREAD_MUTEX_INITIALIZER;
void debug_info(const char *fmt, ...)
{
	pthread_mutex_lock(&debug_lock);
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	pthread_mutex_unlock(&debug_lock);
	return;
}
