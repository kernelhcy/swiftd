#include "base.h"
#include "log.h"

void *job_entry(void *ctx)
{
	job_ctx *jc = (job_ctx*)ctx;
	
	jc -> r_val = jc -> handler(jc -> srv, jc -> ctx, jc -> revents);

	switch(jc -> r_val)
	{
		case HANDLER_GO_ON:
			break;
		case HANDLER_FINISHED:
			break;
		case HANDLER_COMEBACK:
		case HANDLER_WAIT_FOR_EVENT:
		case HANDLER_ERROR:
		case HANDLER_WAIT_FOR_FD:
			break;
		default:
			log_error_write((server *)jc -> srv, __FILE__, __LINE__, "s", "Unknown handler_t.");
			break;
	}
	
	return ctx;
}

job_ctx* job_ctx_get_new(server *srv)
{
	job_ctx *jc = NULL;
	pthread_mutex_lock(&srv -> jc_lock);
	
	if (NULL == srv -> jc_nodes)
	{
		jc = (job_ctx *)malloc(sizeof(*jc));
	}
	else
	{
		jc = srv -> jc_nodes;
		srv -> jc_nodes = srv -> jc_nodes -> next;
	}
	
	pthread_mutex_unlock(&srv -> jc_lock);
	return jc;
}


void job_ctx_free(server *srv, job_ctx *jc)
{
	pthread_mutex_lock(&srv -> jc_lock);
	
	jc -> next = srv -> jc_nodes;
	srv -> jc_nodes = jc;
	
	pthread_mutex_unlock(&srv -> jc_lock);
}

