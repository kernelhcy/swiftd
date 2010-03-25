#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined HAVE_STDINT_H
#include <stdint.h>
#elif defined HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#include "buffer.h"
#include "etag.h"
/**
 * 判断etag是否和matches相同。
 * 就是判断etag中存储的字符串是否和matches相同。
 */
int etag_is_equal(buffer * etag, const char *matches)
{
	if (etag && !buffer_is_empty(etag) && 0 == strcmp(etag->ptr, matches))
	{
		return 1;
	}
	return 0;
}
/**
 * 根据struct stat *st中的文件信息和flags，对etag进行内容填充初始化。
 */
int etag_create(buffer * etag, struct stat *st, etag_flags_t flags)
{
	if (0 == flags)
		return 0;

	buffer_reset(etag);

	if (flags & ETAG_USE_INODE) //i节点号(serial number)
	{
		buffer_append_off_t(etag, st->st_ino);
		buffer_append_string_len(etag, CONST_STR_LEN("-"));
	}

	if (flags & ETAG_USE_SIZE) //普通文件的byte数
	{
		buffer_append_off_t(etag, st->st_size);
		buffer_append_string_len(etag, CONST_STR_LEN("-"));
	}

	if (flags & ETAG_USE_MTIME) //文件最后一次修改的时间。
	{
		buffer_append_long(etag, st->st_mtime);
	}

	return 0;
}

/**
 * 计算etag的哈希值，并存放到mut中。
 * mut以"开始和结尾。
 */
int etag_mutate(buffer * mut, buffer * etag)
{
	size_t i;
	uint32_t h;

	//计算哈希值。
	for (h = 0, i = 0; i < etag->used; ++i)
	{
		h = (h << 5) ^ (h >> 27) ^ (etag->ptr[i]);
	}

	buffer_reset(mut);
	buffer_copy_string_len(mut, CONST_STR_LEN("\""));
	buffer_append_long(mut, h);
	buffer_append_string_len(mut, CONST_STR_LEN("\""));

	return 0;
}
