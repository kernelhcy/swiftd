#ifndef ETAG_H
#define ETAG_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "buffer.h"
/**
 * etag的全称是entity tag(标记实体值)，在RFC2616中关于etag的定义如下：
 * 
 * The ETag response-header field provides the current value of the entity tag for the requested variant. 
 * The headers used with entity tags are described in sections 14.24, 14.26 and 14.44. 
 * The entity tag MAY be used for comparison with other entities from the same resource(see section 13.3.3).
 *					
 *					ETag = "ETag" ":" entity-tag
 * Examples:
 *     ETag: "xyzzy"
 *     ETag: W/"xyzzy"
 *     ETag: ""
 * 把Last-Modified和ETags请求的http报头一起使用，这样可利用客户端（例如浏览器）的缓存。
 * 因为服务器首先产生Last-Modified/Etag标记，服务器可在稍后使用它来判断页面是否已经被修改。
 * 本质上，客户端通过将该记号传回服务器要求服务器验证其（客户端）缓存。
 * 
 * 过程如下:
 * 1.客户端请求一个页面（A）。
 * 2.服务器返回页面A，并在给A加上一个Last-Modified/ETag。
 * 3.客户端展现该页面，并将页面连同Last-Modified/ETag一起缓存。
 * 4.客户再次请求页面A，并将上次请求时服务器返回的Last-Modified/ETag一起传递给服务器。
 * 5.服务器检查该Last-Modified或ETag，并判断出该页面自上次客户端请求之后还未被修改，直接返回响应304和一个空的响应体。
 *
 * 工作原理:
 * Etag由服务器端生成，客户端通过If-Match或者说If-None-Match这个条件判断请求来验证资源是否修改。常见的是使用If-None-Match.
 * 请求一个文件的流程可能如下：
 * ====第一次请求===
 * 1.客户端发起 HTTP GET 请求一个文件；
 * 2.服务器处理请求，返回文件内容和一堆Header，当然包括Etag(例如"2e681a-6-5d044840")(假设服务器支持Etag生成和已经开启了Etag).状态码200
 * ====第二次请求===
 * 1.客户端发起 HTTP GET 请求一个文件，注意这个时候客户端同时发送一个If-None-Match头，这个头的内容就是第一次请求时服务器返回的
 * Etag：2e681a-6-5d044840
 * 2.服务器判断发送过来的Etag和计算出来的Etag匹配，因此If-None-Match为False，不返回200，返回304，客户端继续使用本地缓存；
 * 流程很简单，问题是，如果服务器又设置了Cache-Control:max-age和Expires呢，怎么办？
 * 答案是同时使用，也就是说在完全匹配If-Modified-Since和If-None-Match即检查完修改时间和Etag之后，服务器才能返回304.
 *
 * 参考：
 * 		http://www.hudong.com/wiki/Etag
 * 		http://www.rfc-editor.org/rfc/rfc2616.txt
 */

/*
 * 这个结构体定义etag中可以包含的内容。
 * 可以使用或来包含多个内容。如：ETAG_USE_INODE | ETAG_USE_SIZE
 * 各个值对应的包含值如下：
 */
typedef enum 
{ 
	ETAG_USE_INODE = 1,  	//包含文件的i节点号。
	ETAG_USE_MTIME = 2,  	//包含文件最后一次修改的时间。
	ETAG_USE_SIZE = 4    	//包含文件的byte数。
} etag_flags_t;

int etag_is_equal(buffer * etag, const char *matches);
int etag_create(buffer * etag, struct stat *st, etag_flags_t flags);
int etag_mutate(buffer * mut, buffer * etag);


#endif
