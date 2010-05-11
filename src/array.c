#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <errno.h>
#include <assert.h>

#include "array.h"
#include "buffer.h"
#include "memoryleak.h"

array *array_init(void) 
{
	array *a;

	a = my_calloc(1, sizeof(*a));
	/* a所指向的空间已经全部清零 */
	assert(a);

	a -> next_power_of_2 = 1;

	return a;
}

array *array_init_array(array *src) 
{
	size_t i;
	array *a = array_init();

	a -> used = src -> used;
	a -> size = src -> size;
	a -> next_power_of_2 = src -> next_power_of_2;
	a -> unique_ndx = src -> unique_ndx;

	a -> data = my_malloc(sizeof(*src -> data) * src -> size);
	for (i = 0; i < src -> size; i++) 
	{
		if (src -> data[i]) 
		{
			a -> data[i] = src -> data[i] -> copy(src -> data[i]);
		}
		else 
		{
			a -> data[i] = NULL;
		}
	}

	a -> sorted = my_malloc(sizeof(*src -> sorted) * src -> size);
	memcpy(a -> sorted, src -> sorted, sizeof(*src -> sorted) * src -> size);
	return a;
}

void array_free(array *a) 
{
	size_t i;
	if (!a) return;

	for (i = 0; i < a -> size; i++) 
	{
		if (a -> data[i]) 
		{
			a -> data[i] -> free(a -> data[i]);
		}
	}

	if (a -> data) my_free(a -> data);
	if (a -> sorted) my_free(a -> sorted);

	my_free(a);
}

void array_reset(array *a) 
{
	size_t i;
	if (!a) return;

	for (i = 0; i < a -> used; i++) 
	{
		a -> data[i] -> reset(a -> data[i]);
	}

	a -> used = 0;
}

data_unset *array_pop(array *a) 
{
	data_unset *du;

	assert(a -> used != 0);

	a -> used --;
	du = a -> data[a -> used];
	a -> data[a -> used] = NULL;

	return du;
}


/*
 * 通过key获得数据的下标。
 * keylen是key的长度。
 * 返回的下标通过rndx返回。
 */
static int array_get_index(array *a, const char *key, size_t keylen, int *rndx) 
{
	int ndx = -1;
	int i, pos = 0;

	if (key == NULL) return -1;
	
	
	/* 
	 * sorted数组是个下标数组，存放的是排好序的输入元素的下标，相当于一个排好序的数组。
	 * 利用sorted数组进行二分查找。
	 * 若找到，返回元素在data数组中的位置，并通过rndx返回其在sorted数组中的位置。
	 * 若没有找到，通过rndx返回此元素在sorted中应该的位置，并返回-1
	 */
	for (i = pos = a -> next_power_of_2 / 2; ; i >>= 1) 
	{
		int cmp;

		if (pos < 0) 
		{
			pos += i;
		} 
		else if (pos >= (int)a -> used) 
		{
			pos -= i;
		} 
		else 
		{
			cmp = buffer_caseless_compare(key, keylen, a -> data[a -> sorted[pos]] -> key -> ptr
													, a -> data[a -> sorted[pos]] -> key -> used);
			if (cmp == 0) 
			{
				//找到
				ndx = a -> sorted[pos];
				break;
			} 
			else if (cmp < 0) 
			{
				pos -= i;
			} 
			else 
			{
				pos += i;
			}
		}
		if (i == 0) break;
	}

	if (rndx) *rndx = pos;
	return ndx;
}

data_unset *array_get_element(array *a, const char *key) 
{
	int ndx;

	if (-1 != (ndx = array_get_index(a, key, strlen(key) + 1, NULL)))
	 {
		return a -> data[ndx];
	}

	return NULL;
}

data_unset *array_get_unused_element(array *a, data_type_t t) 
{
	data_unset *ds = NULL;

	UNUSED(t);

	if (a -> size == 0) return NULL;
	if (a -> used == a -> size) return NULL;

	if (a -> data[a -> used]) 
	{
		ds = a -> data[a -> used];
		a -> data[a -> used] = NULL;
	}

	return ds;
}

data_unset *array_replace(array *a, data_unset *du)
{
	int ndx;

	if (-1 == (ndx = array_get_index(a, du -> key -> ptr, du -> key -> used, NULL))) 
	{
		array_insert_unique(a, du);
		return NULL;
	} 
	else 
	{
		data_unset *old = a -> data[ndx];
		a -> data[ndx] = du;
		return old;
	}
}

int array_insert_unique(array *a, data_unset *str) 
{
	int ndx = -1;
	int pos = 0;
	size_t j;

	//给str产生一个key。
	if (str -> key -> used == 0) 
	{
		buffer_copy_long(str -> key, a -> unique_ndx++);
	}

	//查看是否已经在数组中。
	if (-1 != (ndx = array_get_index(a, str -> key -> ptr, str -> key -> used, &pos))) 
	{
		//找到，替换之。
		/*
		 * 对于string类型数据，将要插入的数据追加到原来的数据中，并以','分割。
		 */
		if (a -> data[ndx] -> type == str -> type) 
		{
			str -> insert_dup(a -> data[ndx], str);
		}
			
		return 0;
	}

	//这个key不在数组中，将其插入数组。

	if (a -> used + 1 > INT_MAX) 
	{
		//数组达到最大容量。
		return -1;
	}

	if (a -> size == 0) 
	{
		a -> size = 16;
		a -> data = my_malloc(sizeof(*a -> data) * a -> size);
		a -> sorted = my_malloc(sizeof(*a -> sorted) * a -> size);
		assert(a -> data);
		assert(a -> sorted);
		for (j = a -> used; j < a -> size; j++) 
		{
			a -> data[j] = NULL;
		}
	} 
	else if (a -> size == a -> used) 
	{
		a -> size += 16;
		a -> data  = my_realloc(a -> data, sizeof(*a -> data) * a -> size);
		a -> sorted = my_realloc(a -> sorted, sizeof(*a -> sorted) * a -> size);
		assert(a -> data);
		assert(a -> sorted);
		for (j = a -> used; j < a -> size; j++) 
		{
			a -> data[j] = NULL;
		}
	}

	ndx = (int) a -> used;

	a -> data[a -> used++] = str;
	/* 在上面调用函数array_get_index的时候，已将str应该在数组sorted中位置存放在了pos中。 */
	if (pos != ndx && ((pos < 0) 
				|| buffer_caseless_compare(str -> key -> ptr, str -> key -> used
											, a -> data[a -> sorted[pos]] -> key -> ptr
											, a -> data[a -> sorted[pos]] -> key -> used) > 0)) 
	{
		/* 判断当前pos所对应的元素是否比str小，若是，这pos后移一位 */
		pos++;
	}

	//为新元素在sorted数组中腾出空间。
	if (pos != ndx) 
	{
		memmove(a -> sorted + (pos + 1), a -> sorted + (pos), (ndx - pos) * sizeof(*a -> sorted));
	}

	a -> sorted[pos] = ndx;
	if (a -> next_power_of_2 == (size_t)ndx) 
	{
		a -> next_power_of_2 <<= 1;
	}
	
	return 0;
}

size_t array_get_max_key_length(array *a) 
{
	size_t maxlen, i;

	maxlen = 0;
	for (i = 0; i < a -> used; i ++) 
	{
		data_unset *du = a -> data[i];
		size_t len = strlen(du -> key -> ptr);

		if (len > maxlen)
		{
			maxlen = len;
		}
	}
	return maxlen;
}

