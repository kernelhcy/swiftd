/**
 * worker thread
 */

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

