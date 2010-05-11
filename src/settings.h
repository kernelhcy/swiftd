#ifndef _SWIFTD_SETTINGS_H_
#define _SWIFTD_SETTINGS_H_

#define BV(x) (1 << x)

#define INET_NTOP_CACHE_MAX 4
#define FILE_CACHE_MAX      16
#define SWIFTD_VERSION	  	1
/**
 * max size of a buffer which will just be reset
 * to ->used = 0 instead of really freeing the buffer
 *
 * 64kB (no real reason, just a guess)
 */
#define BUFFER_MAX_REUSE_SIZE  (4 * 1024)

/**
 * max size of the HTTP request header
 *
 * 32k should be enough for everything (just a guess)
 *
 */
#define MAX_HTTP_REQUEST_HEADER  (32 * 1024)

/**
 * 在server.c中的worker的主循环开始的时候，使用了这个枚举类型。用来标记SIGHUP信号的处理方式。
 */
typedef enum 
{ 
	HANDLER_UNSET,
	HANDLER_GO_ON,
	HANDLER_FINISHED,
	HANDLER_COMEBACK,
	HANDLER_WAIT_FOR_EVENT,
	HANDLER_ERROR,
	HANDLER_WAIT_FOR_FD,
} handler_t;


#endif
