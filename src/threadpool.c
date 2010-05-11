#include "threadpool.h"

#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include "base.h"
#include "memoryleak.h"

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

	//debug_info("Create a thread. %d", info -> id);
	int stop = 0;
	int_node *n;
	while(1)
	{
		if (info -> stop)
		{
			break;
		}
		//debug_info("Thread %d is waitting for a job.", info -> ndx);
		pthread_mutex_lock(&info -> lock);
		while(!info -> is_busy && !info -> stop)
		{
			pthread_cond_wait(&info -> cond, &info -> lock);
			//debug_info("Thread %d got a signal. ", info -> ndx);
			if (info -> stop && !info -> job)
			{
				stop = 1;
				break;
			}
			if (info -> job == NULL)
			{
				continue;
			}
		}
		pthread_mutex_unlock(&info -> lock);

		//debug_info("Thread %d get a job.", info -> ndx);

		//运行作业
		if (info -> job)
		{
			info -> job(info -> ctx);
		}
			
		//debug_info("Thread %d. Finish a job.", info -> ndx);
		pthread_mutex_lock(&info -> lock);
		info -> is_busy = 0;
		info -> job = NULL;

		pthread_mutex_lock(&info -> tp -> lock);
		sem_post(&info -> tp -> thread_cnt_sem);
		n = info -> tp -> unused;

		if (NULL == n)
		{
			n = (int_node*)my_malloc(sizeof(*n));
		}
		else
		{
			info -> tp -> unused = info -> tp -> unused -> next;
		}
		
		if (NULL == n)
		{
			info -> stop = 1;
		}

		n -> id = info -> ndx;
		n -> next = info -> tp -> idle_threads;
		info -> tp -> idle_threads = n;
		n = NULL;
		pthread_mutex_unlock(&info -> tp -> lock);
		
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
	int_node *n;

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

		debug_info("Idle thread num : %d cur num %d", idle_cnt, tp -> cur_num);
		/*
		 * 如果空闲的线程占到最大线程数量的一半以上，那么将空闲
		 * 线程的数量控制在50%以内。
		 */
		if (idle_cnt > tp -> max_num /2)
		{
			int need_stop_cnt = idle_cnt * 2 - tp -> max_num - 1;

			//debug_info("Need stop %d threads", need_stop_cnt);
			int del;
			for(i = 0; i < tp -> cur_num && need_stop_cnt > 0; ++i)
			{
				del = 0;
				pthread_mutex_lock(&tp -> threads[i].lock);
				if (!tp -> threads[i].stop && !tp -> threads[i].is_busy)
				{
				//	debug_info("Stop thread id %d", i);
					tp -> threads[i].stop = 1;
					pthread_cond_signal(&tp -> threads[i].cond);
					--need_stop_cnt;
					del = 1;
				}
				pthread_mutex_unlock(&tp -> threads[i].lock);

				if(del)
				{
					pthread_mutex_lock(&tp -> lock);
					n = tp -> unused;
					if (NULL == n)
					{
						n = (int_node*)my_malloc(sizeof(*n));
					}
					else
					{
						tp -> unused = tp -> unused -> next;
					}

					if(NULL != n)
					{
						n -> id = i;
						n -> next = tp -> unused_ndx;
						tp -> unused_ndx = n;
					}
					--tp -> cur_num;
					pthread_mutex_unlock(&tp -> lock);
				}
			}
			//debug_info("Clear thread over.");
		}

		sleep(10);

	}

	return NULL;
}

/**
 * 创建和释放线程池
 */
thread_pool* tp_init(int minnum, int maxnum)
{
	thread_pool *tp = (thread_pool *)my_malloc(sizeof(*tp));
	if (NULL == tp)
	{
		return NULL;
	}

	tp -> max_num = maxnum;
	tp -> min_num = minnum;
	tp -> cur_num = 0;
	tp -> unused = NULL;
	tp -> idle_threads = NULL;
	tp -> unused_ndx = NULL;
	
	pthread_mutex_init(&tp -> lock, NULL);

	sem_init(&tp -> thread_cnt_sem, 0, maxnum); //信号量不跨进程。

	tp -> threads = (thread_info *)my_calloc(maxnum, sizeof(thread_info));
	if (NULL == tp -> threads)
	{
		return NULL;
	}

	//初始线程。
	int i = 0; 
	int_node *tmp = NULL;
	for (; i < minnum; ++i)
	{
		//先初始化数据，再启动线程。
		pthread_mutex_init(&tp -> threads[i].lock, NULL);
		pthread_cond_init(&tp -> threads[i].cond, NULL);
		tp -> threads[i].is_busy = 0;
		tp -> threads[i].stop = 0;
		tp -> threads[i].tp = tp;
		tp -> threads[i].ndx = i;
		tp -> threads[i].job = NULL;

		if (0 != pthread_create(&tp -> threads[i].id, NULL, thread_main, &tp -> threads[i]))
		{
			fprintf(stderr, "(%s %d)Create thread error.\n", __FILE__, __LINE__);
			tp -> threads[i].id = 0;
			continue;
		}

		//在空闲链表中记录线程。
		tmp = (int_node*)my_malloc(sizeof(*tmp));
		if (NULL == tmp)
		{
			return NULL;
		}

		tmp -> id = i;
		tmp -> next = tp -> idle_threads;
		tp -> idle_threads = tmp;
		tmp = NULL;
	}

	tp -> cur_num = minnum;
	
	
	//创建管理线程。
	if (0 != pthread_create(&tp -> manage_thread_id , NULL, manage_thread, tp))
	{
		return NULL;
	}
	
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
	debug_info("Tell all threads to stop.");
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
	debug_info("Wait all threads to stop .........");
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

	debug_info("Free thread pool.");
	pthread_mutex_destroy(&tp -> lock);
	sem_destroy(&tp -> thread_cnt_sem);
	my_free(tp -> threads);

	int_node *n = tp -> unused, *tmp;
	while(NULL != n)
	{
		tmp = n;
		n = n -> next;
		my_free(tmp);
	}
	
	n = tp -> idle_threads;
	while(NULL != n)
	{
		tmp = n;
		n = n -> next;
		my_free(tmp);
	}

	n = tp -> unused_ndx;
	while(NULL != n)
	{
		tmp = n;
		n = n -> next;
		my_free(tmp);
	}

	my_free(tp);
	return;
}

/*
 * 从线程池中分配一个线程处理作业。
 * 成功返回0,否则返回小于0.
 */
int tp_run_job(thread_pool *tp, job_func job, void *ctx)
{
	if(NULL == tp || NULL == job)
	{
		return -1;
	}
	
	/*
	 * 寻找一个空闲的线程。
	 * 如果线程池达到最大容量且没有线程空闲，
	 * 那么程序会阻塞在此直到有线程空闲或接收到信号等。
	 */
	sem_wait(&tp -> thread_cnt_sem);

	int id = -1;
	pthread_mutex_lock(&tp -> lock);

	//空闲链表中的线程可能已经停止，
	//删除已经停止的线程对应的节点，寻找空闲未停止的线程。
	int_node *tmp;
	tmp = tp -> idle_threads;
	while(NULL != tmp && tp -> threads[tmp -> id].stop)
	{
		tp -> idle_threads = tp -> idle_threads -> next;

		tmp -> next = tp -> unused;
		tp -> unused = tmp;
		
		tmp = tp -> idle_threads;
	}

	if(tp -> idle_threads != NULL)
	{
		//有空闲的线程。
		id = tp -> idle_threads -> id;
		int_node *tmp = tp -> idle_threads;
		tp -> idle_threads = tp -> idle_threads -> next;
		
		tmp -> next = tp -> unused;
		tp -> unused = tmp;
		
		debug_info("有空闲:%d", id);
		tp -> threads[id].is_busy = 1;
		
	}
	pthread_mutex_unlock(&tp -> lock);

	if (id != -1)
	{
		//设置任务并通知线程。
		pthread_mutex_lock(&tp -> threads[id].lock);
		tp -> threads[id].job = job;
		tp -> threads[id].ctx = ctx;
		tp -> threads[id].tp = tp;
		pthread_cond_signal(&tp -> threads[id].cond);
		pthread_mutex_unlock(&tp -> threads[id].lock);
		return 0;
	}

	/*
	 * 没有空闲的线程，创建一个。
	 */

	pthread_mutex_lock(&tp -> lock);
	//达到最大线程数。
	if (tp -> cur_num >= tp -> max_num)
	{
		debug_info("Maxnum attached. cur_num %d, max_num %d", tp -> cur_num, tp -> max_num);
		pthread_mutex_unlock(&tp -> lock);
		return -1;
	}

	//查找未使用的节点。
	int ndx;
	if (tp -> unused_ndx != NULL)
	{
		int_node *tmp;
		ndx = tp -> unused_ndx -> id;
		tmp = tp -> unused_ndx;
		tp -> unused_ndx = tp -> unused_ndx -> next;

		tmp -> next = tp -> unused;
		tp -> unused = tmp;
		tmp = NULL;
	}
	else
	{
		ndx = tp -> cur_num;
		++tp -> cur_num;
	}
	//先要初始化线程的数据在启动线程。
	//否则出错。
	debug_info("创建一个新线程：%d", ndx);
	tp -> threads[ndx].is_busy = 1;
	tp -> threads[ndx].stop = 0;
	tp -> threads[ndx].tp = tp;
	tp -> threads[ndx].job = job;
	tp -> threads[ndx].ctx = ctx;
	tp -> threads[ndx].ndx = ndx;
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
	/*
	pthread_mutex_lock(&debug_lock);
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	pthread_mutex_unlock(&debug_lock);
	return;
	*/
}
