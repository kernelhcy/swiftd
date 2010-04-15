#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include <assert.h>
#include <ctype.h>

#include "buffer.h"

//16进制
static const char hex_chars[] = "0123456789abcdef";


/**
 * init the buffer
 *
 */

buffer* buffer_init(void) 
{
	buffer *b;

	b = malloc(sizeof(*b));//b不指向任何数据，但可以确定其可以指向的数据的大小！
	assert(b);

	b->ptr = NULL;
	b->size = 0;
	b->used = 0;

	return b;
}
//用一个buffer src，初始化一个新的buffer
buffer *buffer_init_buffer(buffer *src) 
{
	buffer *b = buffer_init();
	buffer_copy_string_buffer(b, src);
	return b;
}

/**
 * free the buffer
 *
 */

void buffer_free(buffer *b) 
{
	if (!b) return;

	free(b->ptr);
	free(b);
}

void buffer_reset(buffer *b) 
{
	if (!b) return;

	/* 
	 * limit don't reuse buffer larger than ... bytes 
	 * 当buffer的大小超过BUFFER_MAX_REUSE_SIZE时，
	 * 直接释放buffer的空间，而不是重新使用。
	 */
	if (b->size > BUFFER_MAX_REUSE_SIZE) 
	{
		free(b->ptr);
		b->ptr = NULL;
		b->size = 0;
	} 
	else if (b->size) 
	{
		b->ptr[0] = '\0';
	}

	b->used = 0;
}


/**
 *
 * allocate (if neccessary) enough space for 'size' bytes and
 * set the 'used' counter to 0
 * 为复制分配足够的空间，并将used设置为0
 * size为复制所需要的空间
 */

#define BUFFER_PIECE_SIZE 64

int buffer_prepare_copy(buffer *b, size_t size) 
{
	if (!b) return -1;
	
	//当原有的空间为0或小于所需的size，则重新分配空间
	if ((0 == b -> size) || (size > b -> size)) 
	{
		if (b->size) 
			free(b->ptr);

		b->size = size;

		/* 
		 * always allocate a multiply of BUFFER_PIECE_SIZE 
		 * 总是分配BUFFER_PIECE_SIZE倍数的大小的内存空间
		 */
		b->size += BUFFER_PIECE_SIZE - (b->size % BUFFER_PIECE_SIZE);

		b->ptr = malloc(b->size);
		assert(b->ptr);
	}
	b->used = 0;
	return 0;
}

/**
 *
 * increase the internal buffer (if neccessary) to append another 'size' byte
 * ->used isn't changed
 * 为追加size大小的数据而准备空间，可能分配更多的空间
 *
 */

int buffer_prepare_append(buffer *b, size_t size) 
{
	if (!b) return -1;

	if (0 == b->size) {
		b->size = size;

		/* always allocate a multiply of BUFFER_PIECE_SIZE */
		b->size += BUFFER_PIECE_SIZE - (b->size % BUFFER_PIECE_SIZE);

		b->ptr = malloc(b->size);
		b->used = 0;
		assert(b->ptr);
	} else if (b->used + size > b->size) {
		b->size += size;

		/* always allocate a multiply of BUFFER_PIECE_SIZE */
		b->size += BUFFER_PIECE_SIZE - (b->size % BUFFER_PIECE_SIZE);

		b->ptr = realloc(b->ptr, b->size);
		assert(b->ptr);
	}
	return 0;
}
//将s复制到b中，覆盖原来的数据
int buffer_copy_string(buffer *b, const char *s) {
	size_t s_len;

	if (!s || !b) return -1;

	s_len = strlen(s) + 1;
	buffer_prepare_copy(b, s_len);

	memcpy(b->ptr, s, s_len);
	b->used = s_len;

	return 0;
}
/**
 * 将s复制到b的数据区中，s_len是s的长度
 */
int buffer_copy_string_len(buffer *b, const char *s, size_t s_len) 
{
	if (!s || !b) return -1;

	buffer_prepare_copy(b, s_len + 1);

	memcpy(b->ptr, s, s_len);
	b->ptr[s_len] = '\0';
	b->used = s_len + 1;

	return 0;
}
/**
 * 将src的内容复制给b
 */
int buffer_copy_string_buffer(buffer *b, const buffer *src) 
{
	if (!src) return -1;

	if (src->used == 0) 
	{
		b->used = 0;
		return 0;
	}
	return buffer_copy_string_len(b, src->ptr, src->used - 1);
}

/**
 * 将src中的长度为len的内容复制给b
 */
int buffer_copy_string_buffer_len(buffer *b, const buffer *src, size_t len) 
{
	if (!src) 
	{
		return -1;
	}
	
	if (src -> used == 0) 
	{
		b -> used = 0;
		return 0;
	}
	return buffer_copy_string_len(b, src -> ptr, len);
}


//将s追加到b中
int buffer_append_string(buffer *b, const char *s)
{
	size_t s_len;

	if (!s || !b) return -1;

	s_len = strlen(s);
	buffer_prepare_append(b, s_len + 1);

	/*
	 * 如果buffer中原来有数据（字符串），那么最后一个字符是NULL,
	 * 在复制的时候，要覆盖这个字符。
	 * 但当buffer为空时，就不需要覆盖NULL字符，因此，需要加一，
	 * 以便和有数据的情况下处理相同。
	 */
	if (b->used == 0)
		b->used++;
	
	//覆盖原来数据最后一个字符NULL，同时，也将s中的NULL复制到b中。
	memcpy(b->ptr + b->used - 1, s, s_len + 1);
	b->used += s_len;

	return 0;
}

/**
 * 将s追加到b中
 * 如果s的长度小于maxlen，则在b的数据中补上空格，使得
 * b追加的数据的长度等于maxlen。
 *
 * s的长度大于maxlen？？？
 *
 */
int buffer_append_string_rfill(buffer *b, const char *s, size_t maxlen) 
{
	size_t s_len;

	if (!s || !b) return -1;

	s_len = strlen(s);
	buffer_prepare_append(b, maxlen + 1);
	if (b->used == 0)
		b->used++;

	//如果s的长度大于maxlen，这就溢出了。。。
	memcpy(b->ptr + b->used - 1, s, s_len);
	
	//追加空格
	if (maxlen > s_len) 
	{
		memset(b->ptr + b->used - 1 + s_len, ' ', maxlen - s_len);
	}
	
	//设置
	b->used += maxlen;
	b->ptr[b->used - 1] = '\0';
	return 0;
}

/**
 * 追加s到b中。
 * s被看作是一个不以'\0'结尾的字符串，s_len是s的长度。
 * 最终b中的数据以'\0'结尾。
 * 也就是说，如果s的结尾是'\0'，那么，最终，b中的数据末尾有两个'\0'，而且
 * b中used表示的数据长度，包括其中一个'\0'！
 *
 * @param b a buffer
 * @param s the string
 * @param s_len size of the string (without the terminating \0)
 */

int buffer_append_string_len(buffer *b, const char *s, size_t s_len) 
{
	if (!s || !b) return -1;
	if (s_len == 0) return 0;

	buffer_prepare_append(b, s_len + 1);
	if (b->used == 0)
		b->used++;

	memcpy(b->ptr + b->used - 1, s, s_len);
	b->used += s_len;
	b->ptr[b->used - 1] = '\0';

	return 0;
}

//将src中的数据追加到b中
int buffer_append_string_buffer(buffer *b, const buffer *src) 
{
	if (!src) return -1;
	if (src->used == 0) return 0;

	return buffer_append_string_len(b, src->ptr, src->used - 1);
}

//将src中的数据追加到b中
int buffer_append_string_buffer_len(buffer *b, const buffer *src, size_t len) 
{
	if (!src) 
	{
		return -1;
	}
	if (src->used == 0) 
	{
		return 0;
	}
	
	return buffer_append_string_len(b, src -> ptr, len);
}

/**
 * 将s指向的内存区域中的数据追加到b中。
 * 由于追加的是数据而不是字符串，因此，在追加后不需要加上'\0'！
 * s_len为内存区大小
 */
int buffer_append_memory(buffer *b, const char *s, size_t s_len) 
{
	if (!s || !b) return -1;
	if (s_len == 0) return 0;

	buffer_prepare_append(b, s_len);
	memcpy(b->ptr + b->used, s, s_len);
	b->used += s_len;
	/* 不需要追加一个'\0' */
	return 0;
}

//复制s指向的内存区域中的数据到b中
int buffer_copy_memory(buffer *b, const char *s, size_t s_len) 
{
	if (!s || !b) return -1;
	
	//将b的数据长度设为0,调用buffer_append_memory覆盖原来的数据。
	b->used = 0;

	return buffer_append_memory(b, s, s_len);
}
/**
 * 将无符号长整型value以16进制的形式追加到b中。
 * 其中需要将value转化成对应的16进制字符串。
 */
int buffer_append_long_hex(buffer *b, unsigned long value) 
{
	char *buf;
	int shift = 0;
	unsigned long copy = value;
	
	//计算十六进制表示的value的长度
	while (copy) 
	{
		copy >>= 4;
		shift++;
	}

	if (shift == 0)
		shift++;
	/*
	 * 保证追加的字符串为偶数位。
	 * 如若不是偶数位，则在最前面加一个'0'.
	 */
	if (shift & 0x01)
		shift++;

	buffer_prepare_append(b, shift + 1);//最后一个'\0'
	if (b->used == 0)
		b->used++;
	//buf指向开始存放的位置
	buf = b->ptr + (b->used - 1);

	b->used += shift;

	/*
	 * 每四位一组，转化value为十六进制形式的字符串
	 */
	shift <<= 2;
	while (shift > 0) 
	{
		shift -= 4;
		*(buf++) = hex_chars[(value >> shift) & 0x0F];
	}
	*buf = '\0';

	return 0;
}
/*
 * 将长整型val转化成字符串，并存入buf中。
 */
int LI_ltostr(char *buf, long val) 
{
	char swap;
	char *end;
	int len = 1;

	//val为负数，加一个负号，然后转化成正数
	if (val < 0) 
	{
		len++;
		*(buf++) = '-';
		val = -val;
	}

	end = buf;
	/*
	 * 这里val必须设置为大于9，并在循环外在做一次转换(*(end) = '0' + val)！
	 * 因为如果val设置为大于0,当val为0时，将不进入循环，那么循环后面直接在buf中
	 * 追加'\0'。这样0就被转化成了空串！！
	 *
	 * 这里val转化后的字符串是逆序存放在buf中的，在后面要反转，
	 * 以得到正确的顺序。
	 *
	 */
	while (val > 9) 
	{
		*(end++) = '0' + (val % 10);
		val = val / 10;
	}
	*(end) = '0' + val;
	*(end + 1) = '\0';
	len += end - buf;

	//将字符串反转，
	while (buf < end) 
	{
		swap = *end;
		*end = *buf;
		*buf = swap;

		buf++;
		end--;
	}

	return len;
}

int buffer_append_long(buffer *b, long val) 
{
	if (!b) return -1;

	buffer_prepare_append(b, 32); 	//将b的数据空间扩展32个字节
	if (b->used == 0)
		b->used++;

	b->used += LI_ltostr(b->ptr + (b->used - 1), val);
	return 0;
}

int buffer_copy_long(buffer *b, long val) 
{
	if (!b) return -1;

	b->used = 0;
	return buffer_append_long(b, val);
}

//追加off_t类型的val到b中。
//如果off_t的长度和long相同，则不需要下面的函数定义。
int buffer_append_off_t(buffer *b, off_t val) 
{
	char swap;
	char *end;
	char *start;
	int len = 1;

	if (!b) return -1;

	buffer_prepare_append(b, 32);
	if (b->used == 0)
		b->used++;

	start = b->ptr + (b->used - 1);
	if (val < 0) {
		len++;
		*(start++) = '-';
		val = -val;
	}

	end = start;
	while (val > 9) {
		*(end++) = '0' + (val % 10);
		val = val / 10;
	}
	*(end) = '0' + val;
	*(end + 1) = '\0';
	len += end - start;

	while (start < end) {
		swap   = *end;
		*end   = *start;
		*start = swap;

		start++;
		end--;
	}

	b->used += len;
	return 0;
}

int buffer_copy_off_t(buffer *b, off_t val) {
	if (!b) return -1;

	b->used = 0;
	return buffer_append_off_t(b, val);
}

//将c转化成对应的16进制形式
char int2hex(char c) 
{
	return hex_chars[(c & 0x0F)];
}

/** 
 * converts hex char (0-9, A-Z, a-z) to decimal.
 * returns 0xFF on invalid input.
 * 将16进制的字符转化成对应的数字
 * 非法输入返回0xFF
 */
char hex2int(unsigned char hex) 
{
	hex = hex - '0';
	if (hex > 9) {
		hex = (hex + '0' - 1) | 0x20; //将大写字符转化成小写字母
		hex = hex - 'a' + 11;
	}
	if (hex > 15)
		hex = 0xFF;

	return hex;
}


/**
 * init the buffer array
 *
 */

buffer_array* buffer_array_init(void) 
{
	buffer_array *b;

	b = malloc(sizeof(*b));

	assert(b);
	b->ptr = NULL;
	b->size = 0;
	b->used = 0;

	return b;
}

void buffer_array_reset(buffer_array *b) 
{
	size_t i;

	if (!b) return;

	/* if they are too large, reduce them */
	for (i = 0; i < b->used; i++) 
	{
		buffer_reset(b->ptr[i]);
	}

	b->used = 0;
}


/**
 * free the buffer_array
 *
 */

void buffer_array_free(buffer_array *b) 
{
	size_t i;
	if (!b) return;

	for (i = 0; i < b->size; i++) 
	{
		if (b->ptr[i]) buffer_free(b->ptr[i]);
	}
	free(b->ptr);
	free(b);
}

/**
 * 返回buffer array中第一个未使用的buffer
 * 如果buffer array为空或以满，则分配新的空间。
 *
 */
buffer *buffer_array_append_get_buffer(buffer_array *b) 
{
	size_t i;

	if (b->size == 0) //分配空间
	{
		b->size = 16;
		b->ptr = malloc(sizeof(*b->ptr) * b->size);
		assert(b->ptr);
		for (i = 0; i < b->size; i++) 
		{
			b->ptr[i] = NULL;
		}
	} 
	else if (b->size == b->used) //满，重新分配空间
	{
		b->size += 16;
		b->ptr = realloc(b->ptr, sizeof(*b->ptr) * b->size);
		assert(b->ptr);
		for (i = b->used; i < b->size; i++) 
		{
			b->ptr[i] = NULL;
		}
	}

	if (b->ptr[b->used] == NULL) 
	{
		b->ptr[b->used] = buffer_init(); //初始化这个buffer
	}

	b->ptr[b->used]->used = 0;

	return b->ptr[b->used++]; 
}

/**
 * 判断b中是否含有字符串needle，needle的长度为len
 * 如果存在，则返回needle在b中的指针位置，否则返回NULL
 */
char * buffer_search_string_len(buffer *b, const char *needle, size_t len) 
{
	size_t i;
	if (len == 0) return NULL;
	if (needle == NULL) return NULL;

	if (b->used < len) return NULL;

	for(i = 0; i < b->used - len; i++) 
	{
		if (0 == memcmp(b->ptr + i, needle, len)) 
		{
			return b->ptr + i;
		}
	}

	return NULL;
}

//用字符串str初始化一个buffer
buffer *buffer_init_string(const char *str) 
{
	buffer *b = buffer_init();

	buffer_copy_string(b, str);

	return b;
}
//判断b是否为空
int buffer_is_empty(buffer *b) 
{
	if (!b) return 1;
	return (b->used == 0);
}

/**
 * check if two buffer contain the same data
 *
 * HISTORY: this function was pretty much optimized, but didn't handled
 * alignment properly.
 */

int buffer_is_equal(buffer *a, buffer *b) 
{
	if (a->used != b->used) return 0;
	/* a中没有数据，返回1 */
	if (a->used == 0) return 1;

	return (0 == strcmp(a->ptr, b->ptr));
}

//b中的数据是否等于s，b_len为s的长度
int buffer_is_equal_string(buffer *a, const char *s, size_t b_len) 
{
	buffer b;

	b.ptr = (char *)s;
	b.used = b_len + 1;

	return buffer_is_equal(a, &b);
}

/** 
 * simple-assumption:
 *
 * most parts are equal and doing a case conversion needs time
 * 假设比较的部分相同的较多且大小写转换需要时间。
 * 
 */
int buffer_caseless_compare(const char *a, size_t a_len, const char *b, size_t b_len) 
{
	size_t ndx = 0, max_ndx;
	size_t *al, *bl;
	size_t mask = sizeof(*al) - 1;

	al = (size_t *)a;
	bl = (size_t *)b;

	/* 一开始，将字符串数组转化成size_t类型的数组，通过比较size_t类型来比较是否相同 */
	/* libc的字符串比较函数也使用了相同的技巧，可有效的加快比较速度 */
	/* 检查a1和b1的位置是否对齐(size_t的类型长度的倍数?) ? */
	if ( ((size_t)al & mask) == 0 && ((size_t)bl & mask) == 0 ) 
	{
		/* 确定比较的长度 */
		max_ndx = ((a_len < b_len) ? a_len : b_len) & ~mask;

		for (; ndx < max_ndx; ndx += sizeof(*al)) 
		{
			if (*al != *bl) break;
			al++; bl++;

		}

	}
	/* 相同的部分比较完毕 */
	
	/* 开始比较字符串，并忽略大小写 */
	a = (char *)al;
	b = (char *)bl;

	max_ndx = ((a_len < b_len) ? a_len : b_len);

	for (; ndx < max_ndx; ndx++) 
	{
		char a1 = *a++, b1 = *b++;
		/*
			'A'的二进制表示为0100 0001，'a'的二进制表示为0110 0001，
			大写字母比小写字母的ASCII值小了32。
			通过或上一个32，可以使所有的字母全部转换成大写字母。
		*/
		if (a1 != b1) 
		{
			if ((a1 >= 'A' && a1 <= 'Z') && (b1 >= 'a' && b1 <= 'z'))
				a1 |= 32;
			else if ((a1 >= 'a' && a1 <= 'z') && (b1 >= 'A' && b1 <= 'Z'))
				b1 |= 32;
			if ((a1 - b1) != 0) return (a1 - b1);

		}
	}

	/* 相同 */
	if (a_len == b_len) return 0;

	/* 不同 */
	return (a_len - b_len);
}


/**
 * check if the rightmost bytes of the string are equal.
 * 判断b1和b2中，最右边的len个字符是否相同。
 */

int buffer_is_equal_right_len(buffer *b1, buffer *b2, size_t len) 
{
	/* no, len -> equal */
	if (len == 0) return 1;

	/* len > 0, but empty buffers -> not equal */
	if (b1->used == 0 || b2->used == 0) return 0;

	/* buffers too small -> not equal */
	if (b1->used - 1 < len || b1->used - 1 < len) return 0;

	if (0 == strncmp(b1->ptr + b1->used - 1 - len,
			 b2->ptr + b2->used - 1 - len, len))
	{
		return 1;
	}

	return 0;
}

int buffer_copy_string_hex(buffer *b, const char *in, size_t in_len) 
{
	size_t i;

	/* BO protection */
	if (in_len * 2 < in_len) return -1;

	buffer_prepare_copy(b, in_len * 2 + 1);

	for (i = 0; i < in_len; i++) 
	{
		b->ptr[b->used++] = hex_chars[(in[i] >> 4) & 0x0F];
		b->ptr[b->used++] = hex_chars[in[i] & 0x0F];
	}
	b->ptr[b->used++] = '\0';

	return 0;
}

/* everything except: ! ( ) * - . 0-9 A-Z _ a-z */
const char encoded_chars_rel_uri_part[] = 
{
	/*
	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	*/
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  00 -  0F control chars */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  10 -  1F */
	1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1,  /*  20 -  2F space " # $ % & ' + , / */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,  /*  30 -  3F : ; < = > ? */
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  40 -  4F @ */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,  /*  50 -  5F [ \ ] ^ */
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  60 -  6F ` */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1,  /*  70 -  7F { | } ~ DEL */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  80 -  8F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  90 -  9F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  A0 -  AF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  B0 -  BF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  C0 -  CF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  D0 -  DF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  E0 -  EF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  F0 -  FF */
};

/* everything except: ! ( ) * - . / 0-9 A-Z _ a-z */
const char encoded_chars_rel_uri[] = 
{
	/*
	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	*/
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  00 -  0F control chars */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  10 -  1F */
	1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0,  /*  20 -  2F space " # $ % & ' + , */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,  /*  30 -  3F : ; < = > ? */
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  40 -  4F @ */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,  /*  50 -  5F [ \ ] ^ */
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  60 -  6F ` */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1,  /*  70 -  7F { | } ~ DEL */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  80 -  8F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  90 -  9F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  A0 -  AF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  B0 -  BF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  C0 -  CF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  D0 -  DF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  E0 -  EF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  F0 -  FF */
};

const char encoded_chars_html[] = 
{

	/*
	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	*/
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  00 -  0F control chars */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  10 -  1F */
	0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  20 -  2F & */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0,  /*  30 -  3F < > */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  40 -  4F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  50 -  5F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  60 -  6F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,  /*  70 -  7F DEL */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  80 -  8F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  90 -  9F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  A0 -  AF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  B0 -  BF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  C0 -  CF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  D0 -  DF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  E0 -  EF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  F0 -  FF */
};

const char encoded_chars_minimal_xml[] = 
{
	/*
	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	*/
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  00 -  0F control chars */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  10 -  1F */
	0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  20 -  2F & */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0,  /*  30 -  3F < > */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  40 -  4F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  50 -  5F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  60 -  6F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,  /*  70 -  7F DEL */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  80 -  8F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  90 -  9F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  A0 -  AF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  B0 -  BF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  C0 -  CF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  D0 -  DF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  E0 -  EF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  F0 -  FF */
};

const char encoded_chars_hex[] = 
{
	/*
	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	*/
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  00 -  0F control chars */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  10 -  1F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  20 -  2F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  30 -  3F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  40 -  4F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  50 -  5F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  60 -  6F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  70 -  7F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  80 -  8F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  90 -  9F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  A0 -  AF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  B0 -  BF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  C0 -  CF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  D0 -  DF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  E0 -  EF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  F0 -  FF */
};

const char encoded_chars_http_header[] = 
{
	/*
	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	*/
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,  /*  00 -  0F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  10 -  1F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  20 -  2F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  30 -  3F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  40 -  4F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  50 -  5F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  60 -  6F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  70 -  7F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  80 -  8F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  90 -  9F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  A0 -  AF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  B0 -  BF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  C0 -  CF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  D0 -  DF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  E0 -  EF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  F0 -  FF */
};

/**
 * 将字符串s以指定的编码方式存入b中。
 * encoding指定编码的方式。
 */
int buffer_append_string_encoded(buffer *b, const char *s, size_t s_len, buffer_encoding_t encoding) 
{
	unsigned char *ds, *d;
	size_t d_len, ndx;
	const char *map = NULL;

	if (!s || !b) return -1;

	//b中存放的不是亦'\0'结尾的字符串。报错。
	if (b->ptr[b->used - 1] != '\0') 
	{
		abort();
	}

	if (s_len == 0) return 0;

	//根据编码方式，选择对应的编码数组，就是上面的那六个数组。
	switch(encoding) {
	case ENCODING_REL_URI:
		map = encoded_chars_rel_uri;
		break;
	case ENCODING_REL_URI_PART:
		map = encoded_chars_rel_uri_part;
		break;
	case ENCODING_HTML:
		map = encoded_chars_html;
		break;
	case ENCODING_MINIMAL_XML:
		map = encoded_chars_minimal_xml;
		break;
	case ENCODING_HEX:
		map = encoded_chars_hex;
		break;
	case ENCODING_HTTP_HEADER:
		map = encoded_chars_http_header;
		break;
	case ENCODING_UNSET:
		break;
	}

	assert(map != NULL);

	/* 
	 * count to-be-encoded-characters 
	 * 计算经过编码转换后的字符串s的长度。
	 * 不同的编码方式，对与不同的字符，其转换后的字符长度不同。
	 */
	for (ds = (unsigned char *)s, d_len = 0, ndx = 0; ndx < s_len; ds++, ndx++) 
	{
		if (map[*ds]) 
		{
			switch(encoding) 
			{
			case ENCODING_REL_URI:
			case ENCODING_REL_URI_PART:
				d_len += 3;
				break;
			case ENCODING_HTML:
			case ENCODING_MINIMAL_XML:
				d_len += 6;
				break;
			case ENCODING_HTTP_HEADER:
			case ENCODING_HEX:
				d_len += 2;
				break;
			case ENCODING_UNSET:
				break;
			}
		} 
		else //字符不需要转换 
		{
			d_len ++;
		}
	}

	buffer_prepare_append(b, d_len);

	//下面这个循环就是开始做实际的编码转换工作。
	//ds指向字符串s中的字符。d指向b的数据去存放字符的位置。
	for (ds = (unsigned char *)s, d = (unsigned char *)b->ptr + b->used - 1, d_len = 0, ndx = 0; 
						ndx < s_len; ds++, ndx++) 
	{
		if (map[*ds]) 
		{
			switch(encoding) 
			{
			case ENCODING_REL_URI: 			//此编码不需要转换
			case ENCODING_REL_URI_PART: 	//将字符ASCII码转化成其对应的十六进制的形式，并在前面加上'%'
				d[d_len++] = '%';
				d[d_len++] = hex_chars[((*ds) >> 4) & 0x0F];
				d[d_len++] = hex_chars[(*ds) & 0x0F];
				break;
			case ENCODING_HTML: 			//不需要转换
			case ENCODING_MINIMAL_XML: 		//也是转换成ASCII编码的十六进制形式，前面要加上"&#x"，尾随一个';'
				d[d_len++] = '&';
				d[d_len++] = '#';
				d[d_len++] = 'x';
				d[d_len++] = hex_chars[((*ds) >> 4) & 0x0F];
				d[d_len++] = hex_chars[(*ds) & 0x0F];
				d[d_len++] = ';';
				break;
			case ENCODING_HEX: 				//直接转换成ASCII码对应的十六进制。
				d[d_len++] = hex_chars[((*ds) >> 4) & 0x0F];
				d[d_len++] = hex_chars[(*ds) & 0x0F];
				break;
			case ENCODING_HTTP_HEADER:    	//这个处理HTTP头中的换行，统一转换成'\n\t'
				d[d_len++] = *ds;
				d[d_len++] = '\t';
				break;
			case ENCODING_UNSET:
				break;
			}
		} 
		else 
		{
			d[d_len++] = *ds;
		}
	}

	/* 
	 * terminate buffer and calculate new length 
	 * 在新字符串尾部加上一个'\0' 
	 */
	b->ptr[b->used + d_len - 1] = '\0';

	b->used += d_len; 		//新的字符串长度。

	return 0;
}


/* 
 * decodes url-special-chars inplace.
 * replaces non-printable characters with '_'
 * 将rul中存放的特殊编码的字符转换成正常的字符。这里的编码是指上面六种编码中的ENCODING_REL_RUL_PART.
 * 也就是把ASCII码的16进制表示，转换成正常的ASCII码。转换后的结果直接存放在url中。
 *
 * 这个is_query参数的作用仅仅控制是否将字符串中的'+'转换成空格' '。
 */

static int buffer_urldecode_internal(buffer *url, int is_query) 
{
	unsigned char high, low;
	const char *src;
	char *dst;

	if (!url || !url->ptr) return -1;
	
	//源字符串和目的字符串是同一个串。
	src = (const char*) url->ptr;
	dst = (char*) url->ptr;

	while ((*src) != '\0') 
	{
		if (is_query && *src == '+') 
		{
			*dst = ' ';
		} 
		else if (*src == '%') 
		{
			*dst = '%';
			//将后面16进制表示的ASCII码转换成正常的ASCII码。
			high = hex2int(*(src + 1));  		//高四位
			if (high != 0xFF)   				//0xFF表示转换出错。
			{
				low = hex2int(*(src + 2)); 		//低四位
				if (low != 0xFF) 
				{
					high = (high << 4) | low;  	//合并
					/* map control-characters out  判断是否是控制字符。*/
					if (high < 32 || high == 127) 
						high = '_';
					*dst = high;
					src += 2; 	
					//这个转换过程中，三个源字符转换成一个目的字符。
					//虽然是一个字符串，但源字符串遍历的更快，不会冲突。
				}
			}
		} 
		else 
		{
			*dst = *src;
		}

		dst++;
		src++;
	}

	*dst = '\0'; 	//新结尾。
	url->used = (dst - url->ptr) + 1;

	return 0;
}

int buffer_urldecode_path(buffer *url) 
{
	return buffer_urldecode_internal(url, 0);
}

int buffer_urldecode_query(buffer *url) 
{
	return buffer_urldecode_internal(url, 1);
}

/* Remove "/../", "//", "/./" parts from path.
 *
 * /blah/..         gets  /
 * /blah/../foo     gets  /foo
 * /abc/./xyz       gets  /abc/xyz
 * /abc//xyz        gets  /abc/xyz
 *
 * NOTE: src and dest can point to the same buffer, in which case,
 *       the operation is performed in-place.
 *
 * 删除路径字符串中的"/../"，"//"和"/./",简化路径，并不是简单的删除。
 * 对于"/../"在路径中相当与父目录，因此，实际的路径相当于删除"/../"和其前面的一个"/XX/".
 * 如： /home/test/../foo   ->   /home/foo
 * 而"//"和"/./"表示当前目录，简单的将其删去就可以了。
 * NOTE： 源缓冲src和目的缓冲可以指向同一个缓冲，在这种情况下，操作将源缓冲中的数据替换。
 */

int buffer_path_simplify(buffer *dest, buffer *src)
{
	int toklen;
	char c, pre1;
	char *start, *slash, *walk, *out;
	unsigned short pre; 	//pre两个字节，char一个字节，pre中可以存放两个字符。

	if (src == NULL || src->ptr == NULL || dest == NULL)
		return -1;

	if (src == dest)
		buffer_prepare_append(dest, 1);
	else
		buffer_prepare_copy(dest, src->used + 1);

	walk  = src->ptr;
	start = dest->ptr;
	out   = dest->ptr;
	slash = dest->ptr;

	//过滤掉开始的空格。
	while (*walk == ' ') 
	{
		walk++;
	}

	pre1 = *(walk++);
	c    = *(walk++);
	pre  = pre1;
	if (pre1 != '/')  //路径不是以'/'开始，在目的路径中加上'/'
	{
		pre = ('/' << 8) | pre1; //将prel指向的字符存放在pre的高八位。
		*(out++) = '/';
	}
	*(out++) = pre1;

	if (pre1 == '\0')  		//转换结束
	{
		dest->used = (out - start) + 1;
		return 0;
	}

	while (1) 
	{
		if (c == '/' || c == '\0') 
		{
			toklen = out - slash; //slash指向距离c指向的字符前面最近的一个'/'。
			if (toklen == 3 && pre == (('.' << 8) | '.')) // "/../"
			{
				out = slash;
				if (out > start) //删除"/../"前面的一层目录"/XX/".
				{
					out--;
					while (out > start && *out != '/') 
					{
						out--;
					}
				}

				if (c == '\0')
					out++;
			} 
			else if (toklen == 1 || pre == (('/' << 8) | '.')) // "//" 和 "/./"
			{
				out = slash;
				if (c == '\0')
					out++;
			}

			slash = out;
		}

		if (c == '\0')
			break;

		pre1 = c;
		pre  = (pre << 8) | pre1; //pre始终存放的是prel指向的字符和其前一个字符。
		c    = *walk;
		*out = pre1;

		out++;
		walk++;
	}

	*out = '\0';
	dest->used = (out - start) + 1;

	return 0;
}

int light_isdigit(int c) 
{
	return (c >= '0' && c <= '9');
}

int light_isxdigit(int c) 
{
	if (light_isdigit(c)) 
		return 1;

	c |= 32;
	return (c >= 'a' && c <= 'f');
}

int light_isalpha(int c) 
{
	c |= 32;
	return (c >= 'a' && c <= 'z');
}

int light_isalnum(int c) 
{
	return light_isdigit(c) || light_isalpha(c);
}

int buffer_to_lower(buffer *b) 
{
	char *c;

	if (b->used == 0) return 0;

	for (c = b->ptr; *c; c++) 
	{
		if (*c >= 'A' && *c <= 'Z') 
		{
			*c |= 32;
		}
	}

	return 0;
}


int buffer_to_upper(buffer *b) 
{
	char *c;

	if (b->used == 0) return 0;

	for (c = b->ptr; *c; c++) 
	{
		if (*c >= 'a' && *c <= 'z') 
		{
			*c &= ~32;
		}
	}

	return 0;
}
