#ifndef __SWIFTD_PLUGIN_H
#define __SWIFTD_PLUGIN_H
#include "base.h"
#include <pthread.h>
/**
 * 定义插件系统的接口。
 */

/*
 * 每一个插件都要定义一个结构体plugin_data用来存储插件所使用的数据。
 * 这个结构体的名称必须是plugin_data，且在定义的开始部分一定是下面
 * 宏：PLUGIN_DATA。
 * 插件在初始化的时候，要将这个结构体实例化并将其指针赋给plugin结构体
 * 中的data成员。
 *
 * 服务器在调用插件的时候，会将这个指针作为最后一个参数传入到各个函数中。
 * 所有插件必须包含一个函数XXXXXX_init。这个函数接受一个plugin指针，
 * XXXXXX为插件的名称。这个函数初始化plugin结构体中的指针。
 * 在加载插件的时候，程序调用此函数以初始化plugin结构体。
 */

//这个宏定义了插件所使用的数据的公共部分。
#define PLUGIN_DATA  size_t index
struct plugin_data
{
	PLUGIN_DATA;
};

/*
 * 定义slot的类型。
 */
typedef enum
{
	//PLUGIN_SLOT_INIT = 0,
	PLUGIN_SLOT_SET_DEFAULT= 0,
	PLUGIN_SLOT_CLEANUP,
	PLUGIN_SLOT_TRIGGER,
	PLUGIN_SLOT_SIGHUP,
	PLUGIN_SLOT_URL_RAW,
	PLUGIN_SLOT_URL_CLEAN,
	PLUGIN_SLOT_DOCROOT,
	PLUGIN_SLOT_PHYSICAL,
	PLUGIN_SLOT_JOBLIST,
	PLUGIN_SLOT_CONNECTION_CLOSE,
	PLUGIN_SLOT_CONNECTION_RESET,
	PLUGIN_SLOT_SUBREQUEST_START,
	PLUGIN_SLOT_HANDLE_SUBREQUEST,
	PLUGIN_SLOT_SUBREQUEST_END,
	PLUGIN_SLOT_SIZE, 			//slot的数量。
}plugin_slot_t;

/*
 * 插件
 */
typedef struct 
{
	size_t version;
	int ndx;

	void *data;
	void *lib;
	
	/*
	 * 下面一些列的函数指针由插件在初始化的时候赋值。
	 * 如果插件没有实现对应的函数功能，那么，函数指针必须赋值为NULL。
	 */
	
	/*
	 * 这三个函数在插件的整个生命周期中只执行一次。
	 * 其中，前两个会在初始化插件的时候调用。cleanup会在销毁插件时调用。
	 */
	void* (*init)();
	handler_t (*set_default)(server* srv, void *p_d);
	handler_t (*cleanup)(server* srv, void *p_d);
	
	/*
	 * 后面的函数服务器会不断的进行调用。
	 */
	
	//这个还是每秒钟服务器调用一次。相当于一个定时器。
	handler_t (*handle_trigger)(server* srv, void *p_d);
	//在接受到挂断信号时调用这个函数。
	handler_t (*handle_sighup)(server* srv, void *p_d);

	/*
	 * 在解析出request line中的url地址后调用。
	 * 此时url还没有进行解码。
	 */
	handler_t (*handle_url_raw)(server *srv, connection *con, void *p_d);
	/*
	 * 在url地址被解码后调用。
	 */
	handler_t (*handle_url_clean)(server *srv, connection *con, void *p_d);
	/*
	 * 处理插件运行的根目录。
	 */
	handler_t (*handle_docroot)(server *srv, connection *con, void *p_d);
	/*
	 * 得到url对应的物理地址后调用。
	 */
	handler_t (*handle_physical)(server *srv, connection *con, void *p_d);
	/*
	 * 在连接关闭时调用。
	 */
	handler_t (*handle_connection_close)(server *srv, connection *con, void *p_d);
	/*
	 * 在reset connection结构体是调用。
	 */
	handler_t (*handle_connection_reset)(server *srv, connection *con, void *p_d);
	/*
	 * 在每次处理完IO事件后调用。
	 */
	handler_t (*handle_joblist)(server *srv, connection *con, void *p_d);
	
	//请求的主要处理工作通过子请求函数进行处理。
	//通常，下面三个函数处理请求的实质性工作。
	/*
	 * 处理子请求开始。
	 */
	handler_t (*handle_subrequest_start)(server *srv, connection *con, void *p_d);
	/*
	 * 处理子请求。
	 */
	handler_t (*handle_handle_subrequest)(server *srv, connection *con, void *p_d);
	/*
	 * 子请求处理结束。
	 */
	handler_t (*handle_subrequest_end)(server *srv, connection *con, void *p_d);
	
}plugin;

/*
 * 加载和销毁插件。
 */
int plugin_load(server *srv);
void plugin_free(server *srv);

/*
 * 下面的一些类函数有服务器调用。
 * 这些函数依次调用各个插件对应的函数进行处理。
 */
handler_t plugin_handle_url_raw(server *srv, connection *con, void *p_d);
handler_t plugin_handle_url_clean(server *srv, connection *con, void *p_d);
handler_t plugin_handle_docroot(server *srv, connection *con, void *p_d);
handler_t plugin_handle_physical(server *srv, connection *con, void *p_d);
handler_t plugin_handle_connection_close(server *srv, connection *con, void *p_d);
handler_t plugin_handle_joblist(server *srv, connection *con, void *p_d);
handler_t plugin_handle_subrequest_start(server *srv, connection *con, void *p_d);
handler_t plugin_handle_handle_subrequest(server *srv, connection *con, void *p_d);
handler_t plugin_handle_request_end(server *srv, connection *con, void *p_d);
handler_t plugin_handle_connection_reeset(server *srv, connection *con, void *p_d);

handler_t plugin_handle_trigger(server *srv, void *p_d);
handler_t plugin_handle_cleanup(server *srv, void *p_d);


#endif
