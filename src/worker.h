#ifndef __WORK_H
#define __WORK_H

/**
 * worker thread
 */

#include "base.h"

/*
 * the parameters of the worker thread
 */
typedef struct 
{
	int id; 		//ids
	int ndx; 		//index
	pthread_t tid; 	//

}worker_args;


//The entrance of the worker thread
void* worker_main(void * arg);


worker* worker_init();
void worker_free(work *wkr);


#endif
