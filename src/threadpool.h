#ifndef __THREAD_POOL_H
#define __THREAD_POOL_H

/**
 * 一个简单的线程池
 */

#include <pthread.h>
#include <semaphore.h>

//定义job。
//工作被封装在函数中，函数的签名如下。
typedef void *(*process_func)(void *ctx); 	

/*
 * 封装job。
 * func是指向作业的函数指针。
 * ctx为运行作业时所需要的数据，通常作为参数传给func。
 */
typedef struct 
{
	process_func func;
	void *ctx;
	int is_done; //标记作业是否已经完成。
}thread_job;


typedef struct s_thread_pool thread_pool;
/*
 * 定义描述线程的数据结构。
 * 每个线程都有一个实例。
 */
typedef struct 
{
	pthread_t id; 					//线程id
	int ndx; 						//在数组threads中的位置。
	pthread_mutex_t lock; 			//用于锁住整个结构体。也用于配合条件变量使用。
	pthread_cond_t  cond; 			//条件变量。用于等待作业分配。

	thread_job *job; 				//线程要处理的作业。
	thread_pool *tp; 				//指向线程池。

	int stop; 						//标记线程结束。
	int is_busy;
}thread_info;

/*
 * 一个整数链表。
 */
typedef struct s_int_node
{
	int id;
	struct s_int_node *next;
}int_node;

/*
 * 线程池结构体。
 * 存储有关整个线程池的数据。
 */
struct s_thread_pool
{
	pthread_t manage_thread_id;		//管理线程的id
	pthread_mutex_t lock; 			//锁。

	int max_num; 					//最大线程数，允许的最大线程数。
	int min_num; 					//最小线程数，也就是在没有作业要处理时，池中的线程数。
	int cur_num; 					//池中当前线程数。

	thread_info *threads; 			//线程数组。
	sem_t thread_cnt_sem;			//用于达到线程最大值且还有作业时，等待线程空闲。

	int_node *unused; 				//threads中空闲的节点。用来回收。
	int_node *idle_threads;  		//记录空闲的线程的下标
};


/**
 * 创建和释放线程池
 */
thread_pool* tp_init(int minnum, int maxnum);
void tp_free(thread_pool *tp);

/*
 * 从线程池中分配一个线程处理作业。
 * 成功返回0,否则小于0
 */
int tp_run_job(thread_pool *tp, thread_job *job);

/*
 * 获得线程池当前的状态。
 * 
 */
int tp_get_status(thread_pool *tp);


void debug_info(const char *fmt, ...);
#endif
